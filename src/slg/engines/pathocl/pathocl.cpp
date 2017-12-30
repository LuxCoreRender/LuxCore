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

PathOCLRenderEngine::PathOCLRenderEngine(const RenderConfig *rcfg, Film *flm,
		boost::mutex *flmMutex) : PathOCLStateKernelBaseRenderEngine(rcfg, flm, flmMutex) {
	hasStartFilm = false;
}

PathOCLRenderEngine::~PathOCLRenderEngine() {
}

PathOCLBaseRenderThread *PathOCLRenderEngine::CreateOCLThread(const u_int index,
    OpenCLIntersectionDevice *device) {
    return new PathOCLRenderThread(index, device, this);
}

RenderState *PathOCLRenderEngine::GetRenderState() {
	return new PathOCLRenderState(bootStrapSeed);
}

void PathOCLRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	const Properties &defaultProps = PathOCLRenderEngine::GetDefaultProps();

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	const string samplerType = cfg.Get(Property("sampler.type")(SobolSampler::GetObjectTag())).Get<string>();
	if ((samplerType != "RANDOM") && (samplerType != "SOBOL") && (samplerType != "METROPOLIS"))
		throw runtime_error("PATHOCL render engine can use only RANDOM, SOBOL or METROPOLIS samplers");

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	UpdateTaskCount();

	//--------------------------------------------------------------------------
	// Initialize the PathTracer class with rendering parameters
	//--------------------------------------------------------------------------	
	
	usePixelAtomics = cfg.Get(Property("pathocl.pixelatomics.enable")(false)).Get<bool>();

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
		
		delete startRenderState;
		startRenderState = NULL;

		hasStartFilm = true;
	} else
		hasStartFilm = false;

	//--------------------------------------------------------------------------

	PathOCLStateKernelBaseRenderEngine::StartLockLess();
}

void PathOCLRenderEngine::MergeThreadFilms() {
	film->Clear();
	for (size_t i = 0; i < renderThreads.size(); ++i) {
        if (renderThreads[i])
            film->AddFilm(*(((PathOCLRenderThread *)(renderThreads[i]))->threadFilms[0]->film));
    }
}

void PathOCLRenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	MergeThreadFilms();
}

void PathOCLRenderEngine::UpdateCounters() {
	elapsedTime = WallClockTime() - startTime;

	// Update the sample count statistic
	double totalCount = 0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		slg::ocl::pathoclstatebase::GPUTaskStats *stats = ((PathOCLRenderThread *)(renderThreads[i]))->gpuTaskStats;

		for (size_t i = 0; i < taskCount; ++i)
			totalCount += stats[i].sampleCount;
	}

	samplesCount = totalCount;
	// This is a bit tricky because film is reset in UpdateFilmLockLess()
	film->SetSampleCount(samplesCount);

	// Update the ray count statistic
	totalCount = 0.0;
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
		BOOST_FOREACH(DeviceDescription *devDescs, selectedDeviceDescs) {
			if (devDescs->GetMaxMemoryAllocSize() >= 512u * 1024u * 1024u)
				taskCap = Min(taskCap, 512u * 1024u);
			else if (devDescs->GetMaxMemoryAllocSize() >= 256u * 1024u * 1024u)
				taskCap = Min(taskCap, 256u * 1024u);
			else if (devDescs->GetMaxMemoryAllocSize() >= 128u * 1024u * 1024u)
				taskCap = Min(taskCap, 128u * 1024u);
			else
				taskCap = Min(taskCap, 64u * 1024u);
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
			Sampler::ToProperties(cfg);

	return props;
}

RenderEngine *PathOCLRenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new PathOCLRenderEngine(rcfg, flm, flmMutex);
}

const Properties &PathOCLRenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			OCLRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			PathTracer::GetDefaultProps() <<
			Property("pathocl.pixelatomics.enable")(false) <<
			Property("opencl.task.count")("AUTO");

	return props;
}

#endif
