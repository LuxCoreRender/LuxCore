/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <stdexcept>
#include <memory>

#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/devices/ocldevice.h"

#include "slg/slg.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/film/filters/filter.h"
#include "slg/scene/scene.h"

#include "luxcore/cfg.h"

using namespace luxrays;
using namespace slg;
using namespace std;

//------------------------------------------------------------------------------
// PathOCLBaseRenderEngine
//------------------------------------------------------------------------------

PathOCLBaseRenderEngine::PathOCLBaseRenderEngine(const RenderConfig *rcfg,
		const bool supportsNativeThreads) :	OCLRenderEngine(rcfg, supportsNativeThreads),
		compiledScene(nullptr), pixelFilterDistribution(nullptr), oclSampler(nullptr),
		oclPixelFilter(nullptr), photonGICache(nullptr) {
	writeKernelsToFile = false;

	//--------------------------------------------------------------------------
	// Allocate all devices
	//--------------------------------------------------------------------------
	
	vector<IntersectionDevice *> devs = ctx->AddIntersectionDevices(selectedDeviceDescs);

	//--------------------------------------------------------------------------
	// Add CUDA devices
	//--------------------------------------------------------------------------

	SLG_LOG("CUDA devices used:");
	for (size_t i = 0; i < devs.size(); ++i) {
		if (devs[i]->GetDeviceDesc()->GetType() & DEVICE_TYPE_CUDA_ALL) {
			const CUDADeviceDescription *cudaDesc = dynamic_cast<const CUDADeviceDescription *>(devs[i]->GetDeviceDesc());
			SLG_LOG("[" << devs[i]->GetName() << " (Optix enabled: " << cudaDesc->GetCUDAUseOptix() << ")]");
			intersectionDevices.push_back(devs[i]);

			// Suggested compiler options: --use_fast_math
			HardwareDevice *hwDev = dynamic_cast<HardwareDevice *>(devs[i]);

			vector<string> compileOpts;
			compileOpts.push_back("--use_fast_math");

			hwDev->SetAdditionalCompileOpts(compileOpts);
		}
	}

	//--------------------------------------------------------------------------
	// Add OpenCL devices
	//--------------------------------------------------------------------------

	SLG_LOG("OpenCL devices used:");
	for (size_t i = 0; i < devs.size(); ++i) {
		if (devs[i]->GetDeviceDesc()->GetType() & DEVICE_TYPE_OPENCL_ALL) {
			SLG_LOG("[" << devs[i]->GetName() << "]");
			intersectionDevices.push_back(devs[i]);

			OpenCLIntersectionDevice *oclIntersectionDevice = (OpenCLIntersectionDevice *)(devs[i]);
			OpenCLDeviceDescription *oclDeviceDesc = (OpenCLDeviceDescription *)oclIntersectionDevice->GetDeviceDesc();

			// Check if OpenCL 1.1 is available
			SLG_LOG("  Device OpenCL version: " << oclDeviceDesc->GetOpenCLVersion());
			if (!oclDeviceDesc->IsOpenCL_1_1()) {
				// NVIDIA drivers report OpenCL 1.0 even if they are 1.1 so I just
				// print a warning instead of throwing an exception
				SLG_LOG("WARNING: OpenCL version 1.1 or better is required. Device " + devs[i]->GetName() + " may not work.");
			}

			// Suggested compiler options: -cl-fast-relaxed-math -cl-mad-enable
			HardwareDevice *hwDev = dynamic_cast<HardwareDevice *>(devs[i]);

			vector<string> compileOpts;
			compileOpts.push_back("-cl-fast-relaxed-math");
			compileOpts.push_back("-cl-mad-enable");

			hwDev->SetAdditionalCompileOpts(compileOpts);
		}
	}

	//--------------------------------------------------------------------------
	// Add Native devices
	//--------------------------------------------------------------------------

	SLG_LOG("Native devices used: " << nativeRenderThreadCount);
	for (size_t i = 0; i < devs.size(); ++i) {
		if (devs[i]->GetDeviceDesc()->GetType() & DEVICE_TYPE_NATIVE)
			intersectionDevices.push_back(devs[i]);
	}
	
	//--------------------------------------------------------------------------
	// Setup render threads array
	//--------------------------------------------------------------------------

	SLG_LOG("Configuring " << oclRenderThreadCount << " OpenCL render threads");
	renderOCLThreads.resize(oclRenderThreadCount, NULL);

	SLG_LOG("Configuring " << nativeRenderThreadCount << " native render threads");
	renderNativeThreads.resize(nativeRenderThreadCount, NULL);
}

PathOCLBaseRenderEngine::~PathOCLBaseRenderEngine() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();

	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		delete renderOCLThreads[i];
	for (size_t i = 0; i < renderNativeThreads.size(); ++i)
		delete renderNativeThreads[i];

	delete compiledScene;
	delete photonGICache;
	delete[] pixelFilterDistribution;
	delete oclSampler;
	delete oclPixelFilter;
}

void PathOCLBaseRenderEngine::InitGPUTaskConfiguration() {
	// Scene configuration
	taskConfig.scene.defaultVolumeIndex = compiledScene->defaultWorldVolumeIndex;

	// Sampler configuration
	taskConfig.sampler = *oclSampler;

	// Path Tracer configuration
	taskConfig.pathTracer = compiledScene->compiledPathTracer;

	// Pixel filter configuration
	taskConfig.pixelFilter = *oclPixelFilter;

	// Film configuration
	CompiledScene::CompileFilm(*film, taskConfig.film);
	taskConfig.film.usePixelAtomics = renderConfig->GetProperty("pathocl.pixelatomics.enable").Get<bool>();
	if ((taskConfig.sampler.type == slg::ocl::SOBOL) && (taskConfig.sampler.sobol.overlapping > 1)) {
		// I need to use atomics in this case
		taskConfig.film.usePixelAtomics = true;
	}
}

void PathOCLBaseRenderEngine::InitPixelFilterDistribution() {
	unique_ptr<Filter> pixelFilter(renderConfig->AllocPixelFilter());

	// Compile sample distribution
	delete[] pixelFilterDistribution;
	const FilterDistribution filterDistribution(pixelFilter.get(), 64);
	pixelFilterDistribution = CompiledScene::CompileDistribution2D(
			filterDistribution.GetDistribution2D(), &pixelFilterDistributionSize);
}

void PathOCLBaseRenderEngine::InitFilm() {
	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);

	// pathTracer has not yet been initialized
	const bool hybridBackForwardEnable = renderConfig->cfg.Get(PathTracer::GetDefaultProps().
			Get("path.hybridbackforward.enable")).Get<bool>();
	if (hybridBackForwardEnable)
		film->AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	film->SetRadianceGroupCount(renderConfig->scene->lightDefs.GetLightGroupCount());
	film->Init();
}

string PathOCLBaseRenderEngine::GetCachedKernelsHash(const RenderConfig &renderConfig) {
	const string renderEngineType = renderConfig.GetProperty("renderengine.type").Get<string>();

	const float epsilonMin = renderConfig.GetProperty("scene.epsilon.min").Get<float>();
	const float epsilonMax = renderConfig.GetProperty("scene.epsilon.max").Get<float>();
		
	const Properties &cfg = renderConfig.cfg;
	const bool usePixelAtomics = cfg.Get(Property("pathocl.pixelatomics.enable")(false)).Get<bool>();
	const bool useCPUs = cfg.Get(GetDefaultProps().Get("opencl.cpu.use")).Get<bool>();
	const bool useGPUs = cfg.Get(GetDefaultProps().Get("opencl.gpu.use")).Get<bool>();
	const string oclDeviceConfig = cfg.Get(GetDefaultProps().Get("opencl.devices.select")).Get<string>();

	stringstream ssParams;
	ssParams.precision(6);
	ssParams << scientific <<
			renderEngineType << "##" <<
			epsilonMin << "##" <<
			epsilonMax << "##" <<
			usePixelAtomics << "##" <<
			useCPUs << "##" <<
			useGPUs << "##" <<
			oclDeviceConfig;

	const string kernelSource = PathOCLBaseOCLRenderThread::GetKernelSources();

	return oclKernelPersistentCache::HashString(ssParams.str()) + "-" + oclKernelPersistentCache::HashString(kernelSource);
}

void PathOCLBaseRenderEngine::SetCachedKernels(const RenderConfig &renderConfig) {
	const string kernelHash = GetCachedKernelsHash(renderConfig);

	const boost::filesystem::path dirPath = oclKernelPersistentCache::GetCacheDir("LUXCORE_" LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR);
	const boost::filesystem::path filePath = dirPath / (kernelHash + ".ck");
	const string fileName = filePath.generic_string();

	if (!boost::filesystem::exists(filePath)) {
		// Create an empty file
		boost::filesystem::create_directories(dirPath);

		// The use of boost::filesystem::path is required for UNICODE support: fileName
		// is supposed to be UTF-8 encoded.
		boost::filesystem::ofstream file(boost::filesystem::path(fileName),
				boost::filesystem::ofstream::out |
				boost::filesystem::ofstream::binary |
				boost::filesystem::ofstream::trunc);

		file.close();
	}
}

bool PathOCLBaseRenderEngine::HasCachedKernels(const RenderConfig &renderConfig) {
	const string kernelHash = GetCachedKernelsHash(renderConfig);

	const boost::filesystem::path dirPath = oclKernelPersistentCache::GetCacheDir("LUXCORE_" LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR);
	const boost::filesystem::path filePath = dirPath / (kernelHash + ".ck");

	return boost::filesystem::exists(filePath);
}

void PathOCLBaseRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Sampler
	//--------------------------------------------------------------------------

	oclSampler = Sampler::FromPropertiesOCL(cfg);

	//--------------------------------------------------------------------------
	// Filter
	//--------------------------------------------------------------------------

	oclPixelFilter = Filter::FromPropertiesOCL(cfg);

	InitPixelFilterDistribution();

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	if (cfg.IsDefined("opencl.memory.maxpagesize"))
		maxMemPageSize = cfg.Get(Property("opencl.memory.maxpagesize")(512 * 1024 * 1024)).Get<u_longlong>();
	else {
		// Look for the max. page size allowed
		maxMemPageSize = std::numeric_limits<size_t>::max();
		for (u_int i = 0; i < intersectionDevices.size(); ++i) {
			if (intersectionDevices[i]->GetDeviceDesc()->GetType() & DEVICE_TYPE_OPENCL_ALL)
				maxMemPageSize = Min(maxMemPageSize, ((OpenCLIntersectionDevice *)(intersectionDevices[i]))->GetDeviceDesc()->GetMaxMemoryAllocSize());
		}
	}
	SLG_LOG("[PathOCLBaseRenderEngine] OpenCL max. page memory size: " << maxMemPageSize / 1024 << "Kbytes");
	
	writeKernelsToFile = cfg.Get(Property("opencl.kernel.writetofile")(false)).Get<bool>();

	//--------------------------------------------------------------------------
	// Allocate PhotonGICache if enabled
	//--------------------------------------------------------------------------

	// note: photonGICache could have been restored from the render state
	if ((GetType() != RTPATHOCL) && !photonGICache) {
		delete photonGICache;
		photonGICache = PhotonGICache::FromProperties(renderConfig->scene, cfg);
		
		// photonGICache will be nullptr if the cache is disabled
		if (photonGICache)
			photonGICache->Preprocess(renderNativeThreads.size() + renderOCLThreads.size());
	}

	pathTracer.SetPhotonGICache(photonGICache);

	//--------------------------------------------------------------------------
	// Compile the scene
	//--------------------------------------------------------------------------

	compiledScene = new CompiledScene(renderConfig->scene, &pathTracer);
	compiledScene->SetMaxMemPageSize(maxMemPageSize);
	compiledScene->EnableCode(cfg.Get(Property("opencl.code.alwaysenabled")("")).Get<string>());
	compiledScene->Compile();

	//--------------------------------------------------------------------------
	// Compile the configuration
	//--------------------------------------------------------------------------

	InitGPUTaskConfiguration();
	
	//--------------------------------------------------------------------------
	// Start OpenCL render threads
	//--------------------------------------------------------------------------

	SLG_LOG("Starting "<< oclRenderThreadCount << " OpenCL render threads");
	for (size_t i = 0; i < oclRenderThreadCount; ++i) {
		if (!renderOCLThreads[i]) {
			renderOCLThreads[i] = CreateOCLThread(i,
					(HardwareIntersectionDevice *)(intersectionDevices[i]));
		}
	}

	for (size_t i = 0; i < renderOCLThreads.size(); ++i) {
		renderOCLThreads[i]->intersectionDevice->PushThreadCurrentDevice();
		renderOCLThreads[i]->Start();
		renderOCLThreads[i]->intersectionDevice->PopThreadCurrentDevice();
	}

	// I know kernels has been compiled at this point
	SetCachedKernels(*renderConfig);

	//--------------------------------------------------------------------------
	// Start native render threads
	//--------------------------------------------------------------------------

	SLG_LOG("Starting "<< nativeRenderThreadCount << " native render threads");
	for (size_t i = 0; i < nativeRenderThreadCount; ++i) {
		if (!renderNativeThreads[i]) {
			renderNativeThreads[i] = CreateNativeThread(i,
					(NativeIntersectionDevice *)(intersectionDevices[i + oclRenderThreadCount]));
		}
	}

	for (size_t i = 0; i < renderNativeThreads.size(); ++i)
		renderNativeThreads[i]->Start();
}

void PathOCLBaseRenderEngine::StopLockLess() {
	for (size_t i = 0; i < renderNativeThreads.size(); ++i) {
        if (renderNativeThreads[i])
            renderNativeThreads[i]->Interrupt();
    }
	for (size_t i = 0; i < renderOCLThreads.size(); ++i) {
        if (renderOCLThreads[i])
            renderOCLThreads[i]->Interrupt();
    }

	for (size_t i = 0; i < renderNativeThreads.size(); ++i) {
        if (renderNativeThreads[i])
            renderNativeThreads[i]->Stop();
    }
	for (size_t i = 0; i < renderOCLThreads.size(); ++i) {
        if (renderOCLThreads[i]) {
			renderOCLThreads[i]->intersectionDevice->PushThreadCurrentDevice();
            renderOCLThreads[i]->Stop();
			renderOCLThreads[i]->intersectionDevice->PopThreadCurrentDevice();
		}
    }

	delete compiledScene;
	compiledScene = nullptr;
	delete photonGICache;
	photonGICache = nullptr;
	delete[] pixelFilterDistribution;
	pixelFilterDistribution = nullptr;
}

void PathOCLBaseRenderEngine::BeginSceneEditLockLess() {
	for (size_t i = 0; i < renderNativeThreads.size(); ++i)
		renderNativeThreads[i]->Interrupt();
	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		renderOCLThreads[i]->Interrupt();

	for (size_t i = 0; i < renderNativeThreads.size(); ++i)
		renderNativeThreads[i]->BeginSceneEdit();
	for (size_t i = 0; i < renderOCLThreads.size(); ++i) {
		renderOCLThreads[i]->intersectionDevice->PushThreadCurrentDevice();
		renderOCLThreads[i]->BeginSceneEdit();
		renderOCLThreads[i]->intersectionDevice->PopThreadCurrentDevice();
	}
}

void PathOCLBaseRenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	compiledScene->Recompile(editActions);

	for (size_t i = 0; i < renderOCLThreads.size(); ++i) {
		renderOCLThreads[i]->intersectionDevice->PushThreadCurrentDevice();
		renderOCLThreads[i]->EndSceneEdit(editActions);
		renderOCLThreads[i]->intersectionDevice->PopThreadCurrentDevice();
	}
	for (size_t i = 0; i < renderNativeThreads.size(); ++i)
		renderNativeThreads[i]->EndSceneEdit(editActions);
}

bool PathOCLBaseRenderEngine::HasDone() const {
	for (size_t i = 0; i < renderOCLThreads.size(); ++i) {
		if (!renderOCLThreads[i]->HasDone())
			return false;
	}
	for (size_t i = 0; i < renderNativeThreads.size(); ++i) {
		if (!renderNativeThreads[i]->HasDone())
			return false;
	}

	return true;
}

void PathOCLBaseRenderEngine::WaitForDone() const {
	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		renderOCLThreads[i]->WaitForDone();
	for (size_t i = 0; i < renderNativeThreads.size(); ++i)
		renderNativeThreads[i]->WaitForDone();
}

#endif
