/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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
#include "luxrays/core/oclintersectiondevice.h"

#include "slg/slg.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/film/filters/filter.h"
#include "slg/scene/scene.h"

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
	additionalKernelOptions = "";
	writeKernelsToFile = false;

	//--------------------------------------------------------------------------
	// Allocate all devices
	//--------------------------------------------------------------------------
	
	vector<IntersectionDevice *> devs = ctx->AddIntersectionDevices(selectedDeviceDescs);

	//--------------------------------------------------------------------------
	// Add OpenCL devices
	//--------------------------------------------------------------------------

	SLG_LOG("OpenCL devices used:");
	for (size_t i = 0; i < devs.size(); ++i) {
		if (devs[i]->GetType() & DEVICE_TYPE_OPENCL_ALL) {
			SLG_LOG("[" << devs[i]->GetName() << "]");
			intersectionDevices.push_back(devs[i]);

			OpenCLIntersectionDevice *oclIntersectionDevice = (OpenCLIntersectionDevice *)(devs[i]);
			// Disable the support for hybrid rendering in order to not waste resources
			oclIntersectionDevice->SetDataParallelSupport(false);

			// Check if OpenCL 1.1 is available
			SLG_LOG("  Device OpenCL version: " << oclIntersectionDevice->GetDeviceDesc()->GetOpenCLVersion());
			if (!oclIntersectionDevice->GetDeviceDesc()->IsOpenCL_1_1()) {
				// NVIDIA drivers report OpenCL 1.0 even if they are 1.1 so I just
				// print a warning instead of throwing an exception
				SLG_LOG("WARNING: OpenCL version 1.1 or better is required. Device " + devs[i]->GetName() + " may not work.");
			}
		}
	}

	//--------------------------------------------------------------------------
	// Add OpenCL devices
	//--------------------------------------------------------------------------

	SLG_LOG("Native devices used: " << nativeRenderThreadCount);
	for (size_t i = 0; i < devs.size(); ++i) {
		if (devs[i]->GetType() & DEVICE_TYPE_NATIVE_THREAD) {
			intersectionDevices.push_back(devs[i]);

			// Disable the support for hybrid rendering in order to not waste resources
			devs[i]->SetDataParallelSupport(false);
		}
	}
	
	//--------------------------------------------------------------------------
	// Setup render threads array
	//--------------------------------------------------------------------------

	SLG_LOG("Configuring " << oclRenderThreadCount << " OpenCL render threads");
	renderOCLThreads.resize(oclRenderThreadCount, NULL);

	SLG_LOG("Configuring " << nativeRenderThreadCount << " native render threads");
	renderNativeThreads.resize(nativeRenderThreadCount, NULL);

	usePixelAtomics = false;
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

	// Path Tracer configuration
	taskConfig.pathTracer = compiledScene->compiledPathTracer;

	// Pixel filter configuration
	taskConfig.pixelFilter = *oclPixelFilter;

	// Film configuration
	taskConfig.film.radianceGroupCount = film->GetRadianceGroupCount();
	taskConfig.film.bcdDenoiserEnable = film->GetDenoiser().IsEnabled();
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
			if (intersectionDevices[i]->GetType() & DEVICE_TYPE_OPENCL_ALL)
				maxMemPageSize = Min(maxMemPageSize, ((OpenCLIntersectionDevice *)(intersectionDevices[i]))->GetDeviceDesc()->GetMaxMemoryAllocSize());
		}
	}
	SLG_LOG("[PathOCLBaseRenderEngine] OpenCL max. page memory size: " << maxMemPageSize / 1024 << "Kbytes");

	// Suggested compiler options: -cl-fast-relaxed-math -cl-strict-aliasing -cl-mad-enable
	additionalKernelOptions = cfg.Get(Property("opencl.kernel.options")("")).Get<string>();
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
					(OpenCLIntersectionDevice *)(intersectionDevices[i]));
		}
	}

	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		renderOCLThreads[i]->Start();

	//--------------------------------------------------------------------------
	// Start native render threads
	//--------------------------------------------------------------------------

	SLG_LOG("Starting "<< nativeRenderThreadCount << " native render threads");
	for (size_t i = 0; i < nativeRenderThreadCount; ++i) {
		if (!renderNativeThreads[i]) {
			renderNativeThreads[i] = CreateNativeThread(i,
					(NativeThreadIntersectionDevice *)(intersectionDevices[i + oclRenderThreadCount]));
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
        if (renderOCLThreads[i])
            renderOCLThreads[i]->Stop();
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
	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		renderOCLThreads[i]->BeginSceneEdit();
}

void PathOCLBaseRenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	compiledScene->Recompile(editActions);

	for (size_t i = 0; i < renderOCLThreads.size(); ++i)
		renderOCLThreads[i]->EndSceneEdit(editActions);
	for (size_t i = 0; i < renderNativeThreads.size(); ++i)
		renderNativeThreads[i]->EndSceneEdit(editActions);
}

bool PathOCLBaseRenderEngine::IsMaterialCompiled(const MaterialType type) const {
	return (compiledScene == NULL) ? false : compiledScene->IsMaterialCompiled(type);
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
