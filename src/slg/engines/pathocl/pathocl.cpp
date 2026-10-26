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

#include "luxrays/usings.h"
#include <memory>
#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <stdexcept>
#include <mutex>

#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/devices/ocldevice.h"

#include "slg/slg.h"
#include "slg/engines/pathocl/pathocl.h"
#include "slg/engines/pathocl/pathoclrenderstate.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/film/filters/box.h"
#include "slg/film/filters/gaussian.h"
#include "slg/film/filters/mitchell.h"
#include "slg/film/filters/mitchellss.h"
#include "slg/film/filters/blackmanharris.h"
#include "slg/scene/scene.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLRenderEngine
//------------------------------------------------------------------------------

PathOCLRenderEngine::PathOCLRenderEngine(RenderConfigRef rcfg) :

		PathOCLBaseRenderEngine(rcfg, true) {
	lightSampleSplatter = nullptr; 
	eyeSamplerSharedData = nullptr;
	hasStartFilm = false;
	allRenderingThreadsStarted = false;
}

PathOCLRenderEngine::~PathOCLRenderEngine() {
}

PathOCLBaseOCLRenderThread *PathOCLRenderEngine::CreateOCLThread(const u_int index,
    HardwareIntersectionDeviceRef device) {
    return new PathOCLOpenCLRenderThread(index, device, this);
}

PathOCLBaseNativeRenderThread *PathOCLRenderEngine::CreateNativeThread(const u_int index,
			luxrays::NativeIntersectionDeviceRef device) {
	return new PathOCLNativeRenderThread(index, device, this);
}

RenderStateSPtr PathOCLRenderEngine::GetRenderState() {
	return std::make_shared<PathOCLRenderState>(bootStrapSeed, photonGICache);
}

void PathOCLRenderEngine::StartLockLess() {
	auto& cfg = renderConfig.GetConfig();

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	CheckSamplersForNoTile(RenderEngineType2String(GetType()), cfg);

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	UpdateTaskCount();

	//--------------------------------------------------------------------------
	// Initialize rendering parameters
	//--------------------------------------------------------------------------	

	auto defaultProps = PathOCLRenderEngine::GetDefaultProps();
	pathTracer.ParseOptions(cfg, *defaultProps);

	//--------------------------------------------------------------------------
	// Restore render state if there is one
	//--------------------------------------------------------------------------

	if (startRenderState) {
		// Check if the render state is of the right type
		startRenderState->CheckEngineTag(GetObjectTag());

		auto rs = static_pointer_cast<PathOCLRenderState>(startRenderState);

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new PATHOCL seed: " + ToString(newSeed));
		SetSeed(newSeed);

		// Transfer the ownership of PhotonGI cache pointer
		photonGICache = rs->photonGICache;
		rs->photonGICache = nullptr;

		// I have to set the scene pointer in photonGICache because it is not
		// saved by serialization
		if (photonGICache)
			photonGICache->SetScene(renderConfig.GetScene());

		startRenderState = nullptr;

		hasStartFilm = true;
	} else
		hasStartFilm = false;

	//--------------------------------------------------------------------------
	// Initialize sampler shared data
	//--------------------------------------------------------------------------

	if (nativeRenderThreadCount > 0) {
		eyeSamplerSharedData = renderConfig.AllocSamplerSharedData(seedBaseGenerator, GetFilm());

	}

	//--------------------------------------------------------------------------

	// Initialize the PathTracer class
	pathTracer.InitPixelFilterDistribution(GetPixelFilter());

	lightSampleSplatter.reset();
	if (pathTracer.hybridBackForwardEnable)
		lightSampleSplatter = std::make_unique<FilmSampleSplatter>(GetPixelFilter());

	PathOCLBaseRenderEngine::StartLockLess();

	allRenderingThreadsStarted = true;
}

void PathOCLRenderEngine::StopLockLess() {
	allRenderingThreadsStarted = false;

	PathOCLBaseRenderEngine::StopLockLess();

	pathTracer.DeletePixelFilterDistribution();
	lightSampleSplatter.reset();

	eyeSamplerSharedData.reset();

	delete photonGICache;
	photonGICache = nullptr;
}

void PathOCLRenderEngine::MergeThreadFilms() {
	// Film may have been not initialized because of an error during Start()
	if (GetFilm().IsInitiliazed()) {
		GetFilm().Clear();
		GetFilm().GetDenoiser().Clear();

		for (size_t i = 0; i < renderOCLThreads.size(); ++i) {
			if (renderOCLThreads[i])
				GetFilm().AddFilm((((PathOCLOpenCLRenderThread *)(renderOCLThreads[i]))->threadFilms[0]->GetFilm()));
		}

		if (renderNativeThreads.size() > 0) {
			// All threads use the film of the first one
			if (renderNativeThreads[0])
				GetFilm().AddFilm(
					*static_cast<PathOCLNativeRenderThread *>(renderNativeThreads[0])->threadFilm
				);
		}
	}
}

void PathOCLRenderEngine::UpdateFilmLockLess() {
	std::unique_lock<std::mutex> lock(*filmMutex);

	MergeThreadFilms();
}

void PathOCLRenderEngine::UpdateCounters() {
	// Update the ray count statistic
	double totalCount = 0.0;
	for (IntersectionDeviceRef dev : intersectionDevices)
		totalCount += dev.GetTotalRaysCount();
	raysCount = totalCount;
}

void PathOCLRenderEngine::UpdateTaskCount() {
	auto& cfg = renderConfig.GetConfig();
	if (!cfg.IsDefined("opencl.task.count") && (GetType() == RTPATHOCL)) {
		// In this case, I will tune task count for RTPATHOCL
		taskCount = GetFilm().GetWidth() * GetFilm().GetHeight() / intersectionDevices.size();
	} else {
		// More paths in flight keep the GPU busier. 1M paths use about 2.7GB
		// of per-path buffers (LynxSpider), so only cards with more than 10GB
		// get them.
		const u_int defaultTaskCount = 1024ull * 1024ull;

		// Compute the cap to the number of tasks
		u_int taskCap = defaultTaskCount;
		for(DeviceDescription& devDesc: selectedDeviceDescs) {
			if (devDesc.GetMaxMemory() <= 10ull * 1024ull * 1024ull * 1024ull)
				taskCap = Min(taskCap, 512u * 1024u);
			if (devDesc.GetMaxMemory() <= 8ull* 1024ull * 1024ull * 1024ull) // For 8GB cards
				taskCap = Min(taskCap, 256u * 1024u);
			if (devDesc.GetMaxMemory() <= 4ull * 1024ull * 1024ull * 1024ull) // For 4GB cards
				taskCap = Min(taskCap, 128u * 1024u);
			if (devDesc.GetMaxMemory() <= 2ull * 1024ull * 1024ull * 1024ull) // For 2GB cards
				taskCap = Min(taskCap, 64u * 1024u);
		}

		if (cfg.Get(Property("opencl.task.count")("AUTO")).Get<string>() == "AUTO")
			taskCount = taskCap;
		else
			taskCount = cfg.Get(Property("opencl.task.count")(taskCap)).Get<u_int>();
	}

	// I don't know yet the workgroup size of each device so I can not
	// round up task count to be a multiple of workgroups size of all devices
	// used. Rounding to 8192 is a simple trick based on the assumption that
	// workgroup size is a power of 2 and <= 8192.
	taskCount = RoundUp<u_int>(taskCount, 8192);
	if(GetType() != RTPATHOCL)
		SLG_LOG("[PathOCLRenderEngine] OpenCL task count: " << taskCount);
}

u_int PathOCLRenderEngine::GetTotalEyeSPP() const {
	u_int spp = 0;

	// I can access rendering threads film only after all have been started
	if (allRenderingThreadsStarted) {
		for (size_t i = 0; i < renderOCLThreads.size(); ++i) {
			if (renderOCLThreads[i]) {
				const PathOCLOpenCLRenderThread *thread = (const PathOCLOpenCLRenderThread *)renderOCLThreads[i];
				FilmConstRef film = thread->threadFilms[0]->GetFilm();
				spp += GetFilm().GetTotalEyeSampleCount() / GetFilm().GetPixelCount();
			}
		}

		if (renderNativeThreads.size() > 0) {
			// All threads use the film of the first one
			if (renderNativeThreads[0]) {
				const PathOCLNativeRenderThread *thread = (const PathOCLNativeRenderThread *)renderNativeThreads[0];
				FilmConstRef film = *thread->threadFilm;
				spp += GetFilm().GetTotalEyeSampleCount() / GetFilm().GetPixelCount();
			}
		}
	}

	return spp;
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

PropertiesUPtr PathOCLRenderEngine::ToProperties(const Properties &cfg) {
	auto props_ptr = std::make_unique<Properties>();
	auto& props = *props_ptr;

	props <<
			OCLRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps()->Get("renderengine.type")) <<
			PathTracer::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps()->Get("pathocl.pixelatomics.enable")) <<
			cfg.Get(GetDefaultProps()->Get("opencl.task.count")) <<
			Sampler::ToProperties(cfg) <<
			PhotonGICache::ToProperties(cfg);

	return props_ptr;
}

RenderEngine *PathOCLRenderEngine::FromProperties(RenderConfigRef rcfg) {
	return new PathOCLRenderEngine(rcfg);
}

PropertiesUPtr PathOCLRenderEngine::GetDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
			OCLRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			PathTracer::GetDefaultProps() <<
			Property("pathocl.pixelatomics.enable")(true) <<
			Property("opencl.task.count")("AUTO") <<
			PhotonGICache::GetDefaultProps();

	return props;
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
