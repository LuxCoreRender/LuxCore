/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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
}

PathOCLRenderEngine::~PathOCLRenderEngine() {
}

PathOCLBaseRenderThread *PathOCLRenderEngine::CreateOCLThread(const u_int index,
    OpenCLIntersectionDevice *device) {
    return new PathOCLRenderThread(index, device, this);
}

void PathOCLRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	Properties defaultProps = (GetType() == PATHOCL) ?
		PathOCLRenderEngine::GetDefaultProps() :
		RTPathOCLRenderEngine::GetDefaultProps();

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	const string samplerType = cfg.Get(Property("sampler.type")(SobolSampler::GetObjectTag())).Get<string>();
	if ((samplerType != "RANDOM") && (samplerType != "SOBOL") && (samplerType != "METROPOLIS"))
		throw runtime_error("(RT)PATHOCL render engine can use only RANDOM, SOBOL or METROPOLIS samplers");

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	UpdateTaskCount();

	//--------------------------------------------------------------------------
	// General path tracing settings
	//--------------------------------------------------------------------------	
	
	// Path depth settings
	maxPathDepth.depth = Max(0, cfg.Get(GetDefaultProps().Get("path.pathdepth.total")).Get<int>());
	maxPathDepth.diffuseDepth = Max(0, cfg.Get(GetDefaultProps().Get("path.pathdepth.diffuse")).Get<int>());
	maxPathDepth.glossyDepth = Max(0, cfg.Get(GetDefaultProps().Get("path.pathdepth.glossy")).Get<int>());
	maxPathDepth.specularDepth = Max(0, cfg.Get(GetDefaultProps().Get("path.pathdepth.specular")).Get<int>());

	// For compatibility with the past
	if (cfg.IsDefined("path.maxdepth") &&
			!cfg.IsDefined("path.pathdepth.total") &&
			!cfg.IsDefined("path.pathdepth.diffuse") &&
			!cfg.IsDefined("path.pathdepth.glossy") &&
			!cfg.IsDefined("path.pathdepth.specular")) {
		const u_int maxDepth = Max(0, cfg.Get("path.maxdepth").Get<int>());
		maxPathDepth.depth = maxDepth;
		maxPathDepth.diffuseDepth = maxDepth;
		maxPathDepth.glossyDepth = maxDepth;
		maxPathDepth.specularDepth = maxDepth;
	}

	// Russian Roulette settings
	rrDepth = (u_int)Max(1, cfg.Get(defaultProps.Get("path.russianroulette.depth")).Get<int>());
	rrImportanceCap = Clamp(cfg.Get(defaultProps.Get("path.russianroulette.cap")).Get<float>(), 0.f, 1.f);

	// Clamping settings
	// clamping.radiance.maxvalue is the old radiance clamping, now converted in variance clamping
	sqrtVarianceClampMaxValue = cfg.Get(Property("path.clamping.radiance.maxvalue")(0.f)).Get<float>();
	if (cfg.IsDefined("path.clamping.variance.maxvalue"))
		sqrtVarianceClampMaxValue = cfg.Get(defaultProps.Get("path.clamping.variance.maxvalue")).Get<float>();
	sqrtVarianceClampMaxValue = Max(0.f, sqrtVarianceClampMaxValue);
	pdfClampValue = Max(0.f, cfg.Get(defaultProps.Get("path.clamping.pdf.value")).Get<float>());

	useFastPixelFilter = cfg.Get(defaultProps.Get("path.fastpixelfilter.enable")).Get<bool>();
	usePixelAtomics = cfg.Get(Property("pathocl.pixelatomics.enable")(false)).Get<bool>();
	forceBlackBackground = cfg.Get(GetDefaultProps().Get("path.forceblackbackground.enable")).Get<bool>();

	PathOCLStateKernelBaseRenderEngine::StartLockLess();
}

void PathOCLRenderEngine::MergeThreadFilms() {
	film->Reset();
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
			cfg.Get(GetDefaultProps().Get("renderengine.type"));

	if (cfg.IsDefined("path.maxdepth") &&
			!cfg.IsDefined("path.pathdepth.total") &&
			!cfg.IsDefined("path.pathdepth.diffuse") &&
			!cfg.IsDefined("path.pathdepth.glossy") &&
			!cfg.IsDefined("path.pathdepth.specular")) {
		const u_int maxDepth = Max(0, cfg.Get("path.maxdepth").Get<int>());
		props << 
				Property("path.pathdepth.total")(maxDepth) <<
				Property("path.pathdepth.diffuse")(maxDepth) <<
				Property("path.pathdepth.glossy")(maxDepth) <<
				Property("path.pathdepth.specular")(maxDepth);
	} else {
		props <<
				cfg.Get(GetDefaultProps().Get("path.pathdepth.total")) <<
				cfg.Get(GetDefaultProps().Get("path.pathdepth.diffuse")) <<
				cfg.Get(GetDefaultProps().Get("path.pathdepth.glossy")) <<
				cfg.Get(GetDefaultProps().Get("path.pathdepth.specular"));
	}

	props <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.depth")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.cap")) <<
			cfg.Get(GetDefaultProps().Get("path.clamping.variance.maxvalue")) <<
			cfg.Get(GetDefaultProps().Get("path.clamping.pdf.value")) <<
			cfg.Get(GetDefaultProps().Get("path.fastpixelfilter.enable")) <<
			cfg.Get(GetDefaultProps().Get("pathocl.pixelatomics.enable")) <<
			cfg.Get(GetDefaultProps().Get("path.forceblackbackground.enable")) <<
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
			Property("path.pathdepth.total")(6) <<
			Property("path.pathdepth.diffuse")(4) <<
			Property("path.pathdepth.glossy")(4) <<
			Property("path.pathdepth.specular")(6) <<
			Property("path.russianroulette.depth")(3) <<
			Property("path.russianroulette.cap")(.5f) <<
			Property("path.clamping.variance.maxvalue")(0.f) <<
			Property("path.clamping.pdf.value")(0.f) <<
			Property("path.fastpixelfilter.enable")(true) <<
			Property("pathocl.pixelatomics.enable")(false) <<
			Property("path.forceblackbackground.enable")(false) <<
			Property("opencl.task.count")("AUTO");

	return props;
}

#endif
