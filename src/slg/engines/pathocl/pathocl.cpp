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

#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/oclintersectiondevice.h"

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

PathOCLRenderEngine::PathOCLRenderEngine(const RenderConfig *rcfg) :
		PathOCLBaseRenderEngine(rcfg, true) {
	samplerSharedData = NULL;
	hasStartFilm = false;
}

PathOCLRenderEngine::~PathOCLRenderEngine() {
	delete samplerSharedData;
}

PathOCLBaseOCLRenderThread *PathOCLRenderEngine::CreateOCLThread(const u_int index,
    OpenCLIntersectionDevice *device) {
    return new PathOCLOpenCLRenderThread(index, device, this);
}

PathOCLBaseNativeRenderThread *PathOCLRenderEngine::CreateNativeThread(const u_int index,
			luxrays::NativeThreadIntersectionDevice *device) {
	return new PathOCLNativeRenderThread(index, device, this);
}

RenderState *PathOCLRenderEngine::GetRenderState() {
	return new PathOCLRenderState(bootStrapSeed, photonGICache);
}

void PathOCLRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

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
	
	usePixelAtomics = cfg.Get(Property("pathocl.pixelatomics.enable")(false)).Get<bool>();

	const Properties &defaultProps = PathOCLRenderEngine::GetDefaultProps();
	pathTracer.ParseOptions(cfg, defaultProps);

	//--------------------------------------------------------------------------
	// Restore render state if there is one
	//--------------------------------------------------------------------------

	if (startRenderState) {
		// Check if the render state is of the right type
		startRenderState->CheckEngineTag(GetObjectTag());

		PathOCLRenderState *rs = (PathOCLRenderState *)startRenderState;

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new PATHOCL seed: " + ToString(newSeed));
		SetSeed(newSeed);

		// Transfer the ownership of PhotonGI cache pointer
		photonGICache = rs->photonGICache;
		rs->photonGICache = nullptr;
		
		// I have to set the scene pointer in photonGICache because it is not
		// saved by serialization
		photonGICache->SetScene(renderConfig->scene);

		delete startRenderState;
		startRenderState = NULL;

		hasStartFilm = true;
	} else
		hasStartFilm = false;

	//--------------------------------------------------------------------------
	// Initialize sampler shared data
	//--------------------------------------------------------------------------

	if (nativeRenderThreadCount > 0)
		samplerSharedData = renderConfig->AllocSamplerSharedData(&seedBaseGenerator, film);

	//--------------------------------------------------------------------------

	// Initialize the PathTracer class
	pathTracer.InitPixelFilterDistribution(pixelFilter);

	PathOCLBaseRenderEngine::StartLockLess();

	// Set pathTracer PhotonGI. photonGICache is eventually initialized
	// inside PathOCLBaseRenderEngine::StartLockLess()
	pathTracer.SetPhotonGICache(photonGICache);
}

void PathOCLRenderEngine::StopLockLess() {
	PathOCLBaseRenderEngine::StopLockLess();

	pathTracer.DeletePixelFilterDistribution();

	delete samplerSharedData;
	samplerSharedData = NULL;

	delete photonGICache;
	photonGICache = nullptr;
}

void PathOCLRenderEngine::MergeThreadFilms() {
	film->Clear();
	film->GetDenoiser().Clear();

	for (size_t i = 0; i < renderOCLThreads.size(); ++i) {
        if (renderOCLThreads[i])
            film->AddFilm(*(((PathOCLOpenCLRenderThread *)(renderOCLThreads[i]))->threadFilms[0]->film));
    }
	
	if (renderNativeThreads.size() > 0) {
		// All threads use the film of the first one
		if (renderNativeThreads[0])
			film->AddFilm(*(((PathOCLNativeRenderThread *)(renderNativeThreads[0]))->threadFilm));
	}
}

void PathOCLRenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	MergeThreadFilms();
}

void PathOCLRenderEngine::UpdateCounters() {
	// Update the ray count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < intersectionDevices.size(); ++i)
		totalCount += intersectionDevices[i]->GetTotalRaysCount();
	raysCount = totalCount;
}

void PathOCLRenderEngine::UpdateTaskCount() {
	const Properties &cfg = renderConfig->cfg;

	if (!cfg.IsDefined("opencl.task.count") && (GetType() == RTPATHOCL)) {
		// In this case, I will tune task count for RTPATHOCL
		taskCount = film->GetWidth() * film->GetHeight() / intersectionDevices.size();
		taskCount = RoundUp<u_int>(taskCount, 8192);
	} else {
		const u_int defaultTaskCount = 1024u * 1024u;

		// Compute the cap to the number of tasks
		u_int taskCap = defaultTaskCount;
		BOOST_FOREACH(DeviceDescription *devDesc, selectedDeviceDescs) {
			if (devDesc->GetType() & DEVICE_TYPE_OPENCL_ALL) {
				if (devDesc->GetMaxMemoryAllocSize() >= 512u * 1024u * 1024u)
					taskCap = Min(taskCap, 512u * 1024u);
				else if (devDesc->GetMaxMemoryAllocSize() >= 256u * 1024u * 1024u)
					taskCap = Min(taskCap, 256u * 1024u);
				else if (devDesc->GetMaxMemoryAllocSize() >= 128u * 1024u * 1024u)
					taskCap = Min(taskCap, 128u * 1024u);
				else
					taskCap = Min(taskCap, 64u * 1024u);
			}
		}

		if (cfg.Get(Property("opencl.task.count")(defaultTaskCount)).Get<string>() == "AUTO")
			taskCount = defaultTaskCount;
		else
			taskCount = cfg.Get(Property("opencl.task.count")(defaultTaskCount)).Get<u_int>();
		taskCount = Min(taskCount, taskCap);
	}
	
	// I don't know yet the workgroup size of each device so I can not
	// round up task count to be a multiple of workgroups size of all devices
	// used. Rounding to 8192 is a simple trick based on the assumption that
	// workgroup size is a power of 2 and <= 8192.
	taskCount = RoundUp<u_int>(taskCount, 8192);
	if(GetType() != RTPATHOCL)
		SLG_LOG("[PathOCLRenderEngine] OpenCL task count: " << taskCount);
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties PathOCLRenderEngine::ToProperties(const Properties &cfg) {
	Properties props;

	props <<
			OCLRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			PathTracer::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("pathocl.pixelatomics.enable")) <<
			cfg.Get(GetDefaultProps().Get("opencl.task.count")) <<
			Sampler::ToProperties(cfg) <<
			PhotonGICache::ToProperties(cfg);

	return props;
}

RenderEngine *PathOCLRenderEngine::FromProperties(const RenderConfig *rcfg) {
	return new PathOCLRenderEngine(rcfg);
}

const Properties &PathOCLRenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			OCLRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			PathTracer::GetDefaultProps() <<
			Property("pathocl.pixelatomics.enable")(false) <<
			Property("opencl.task.count")("AUTO") <<
			PhotonGICache::GetDefaultProps();

	return props;
}

#endif
