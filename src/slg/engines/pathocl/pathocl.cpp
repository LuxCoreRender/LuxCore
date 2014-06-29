/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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
#include "slg/film/filter.h"
#include "slg/sdl/scene.h"
#include "slg/engines/rtpathocl/rtpathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLRenderEngine
//------------------------------------------------------------------------------

PathOCLRenderEngine::PathOCLRenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex,
		const bool realTime) : PathOCLBaseRenderEngine(rcfg, flm, flmMutex, realTime) {
}

PathOCLRenderEngine::~PathOCLRenderEngine() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();

	delete sampler;
	delete filter;
}

PathOCLRenderThread *PathOCLRenderEngine::CreateOCLThread(const u_int index,
    OpenCLIntersectionDevice *device) {
    return new PathOCLRenderThread(index, device, this);
}

void PathOCLRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	if (!cfg.IsDefined("opencl.task.count") && (GetEngineType() == RTPATHOCL)) {
		// In this case, I will tune task count for RTPATHOCL
		taskCount = film->GetWidth() * film->GetHeight() / intersectionDevices.size();
	} else {
		taskCount = cfg.Get(Property("opencl.task.count")(65536)).Get<u_int>();
		// I don't know yet the workgroup size of each device so I can not
		// round up task count to be a multiple of workgroups size of all devices
		// used. rounding to 2048 is a simple trick based on the assumption that
		// workgroup size is a power of 2 and <= 8192.
		taskCount = RoundUp<u_int>(taskCount, 8192);
		SLG_LOG("[PathOCLRenderEngine] OpenCL task count: " << taskCount);
	}

	//--------------------------------------------------------------------------
	// General path tracing settings
	//--------------------------------------------------------------------------	
	
	maxPathDepth = Max(1, cfg.Get(Property("path.maxdepth")(5)).Get<int>());
	rrDepth = Max(1, cfg.Get(Property("path.russianroulette.depth")(3)).Get<int>());
	rrImportanceCap = Clamp(cfg.Get(Property("path.russianroulette.cap")(.5f)).Get<float>(), 0.f, 1.f);

	//--------------------------------------------------------------------------
	// Sampler
	//--------------------------------------------------------------------------

	sampler = new slg::ocl::Sampler();
	const SamplerType samplerType = Sampler::String2SamplerType(cfg.Get(Property("sampler.type")("RANDOM")).Get<string>());
	switch (samplerType) {
		case RANDOM:
			sampler->type = slg::ocl::RANDOM;
			break;
		case METROPOLIS: {
			const float largeMutationProbability = cfg.Get(Property("sampler.metropolis.largesteprate")(.4f)).Get<float>();
			const float imageMutationRange = cfg.Get(Property("sampler.metropolis.imagemutationrate")(.1f)).Get<float>();
			const u_int maxRejects = cfg.Get(Property("sampler.metropolis.maxconsecutivereject")(512)).Get<u_int>();

			sampler->type = slg::ocl::METROPOLIS;
			sampler->metropolis.largeMutationProbability = largeMutationProbability;
			sampler->metropolis.imageMutationRange = imageMutationRange;
			sampler->metropolis.maxRejects = maxRejects;
			break;
		}
		case SOBOL:
			sampler->type = slg::ocl::SOBOL;
			break;
		default:
			throw std::runtime_error("Unknown sampler.type: " + boost::lexical_cast<std::string>(samplerType));
	}

	//--------------------------------------------------------------------------
	// Filter
	//--------------------------------------------------------------------------

	const Filter *filmFilter = film->GetFilter();
	const FilterType filterType = filmFilter ? filmFilter->GetType() : FILTER_NONE;

	filter = new slg::ocl::Filter();
	// Force pixel filter to NONE if I'm RTPATHOCL
	if ((filterType == FILTER_NONE) || (GetEngineType() == RTPATHOCL))
		filter->type = slg::ocl::FILTER_NONE;
	else if (filterType == FILTER_BOX) {
		filter->type = slg::ocl::FILTER_BOX;
		filter->box.widthX = filmFilter->xWidth;
		filter->box.widthY = filmFilter->yWidth;
	} else if (filterType == FILTER_GAUSSIAN) {
		filter->type = slg::ocl::FILTER_GAUSSIAN;
		filter->gaussian.widthX = filmFilter->xWidth;
		filter->gaussian.widthY = filmFilter->yWidth;
		filter->gaussian.alpha = ((GaussianFilter *)filmFilter)->alpha;
	} else if (filterType == FILTER_MITCHELL) {
		filter->type = slg::ocl::FILTER_MITCHELL;
		filter->mitchell.widthX = filmFilter->xWidth;
		filter->mitchell.widthY = filmFilter->yWidth;
		filter->mitchell.B = ((MitchellFilter *)filmFilter)->B;
		filter->mitchell.C = ((MitchellFilter *)filmFilter)->C;
	} else if (filterType == FILTER_MITCHELL_SS) {
		filter->type = slg::ocl::FILTER_MITCHELL;
		filter->mitchell.widthX = filmFilter->xWidth;
		filter->mitchell.widthY = filmFilter->yWidth;
		filter->mitchell.B = ((MitchellFilterSS *)filmFilter)->B;
		filter->mitchell.C = ((MitchellFilterSS *)filmFilter)->C;
	} else if (filterType == FILTER_BLACKMANHARRIS) {
		filter->type = slg::ocl::FILTER_BLACKMANHARRIS;
		filter->blackmanharris.widthX = filmFilter->xWidth;
		filter->blackmanharris.widthY = filmFilter->yWidth;
	} else
		throw std::runtime_error("Unknown path.filter.type: " + boost::lexical_cast<std::string>(filterType));

	usePixelAtomics = cfg.Get(Property("path.pixelatomics.enable")(false)).Get<bool>();

	PathOCLBaseRenderEngine::StartLockLess();
}

void PathOCLRenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	film->Reset();
	for (size_t i = 0; i < renderThreads.size(); ++i) {
        if (renderThreads[i])
            film->AddFilm(*(((PathOCLRenderThread *)(renderThreads[i]))->threadFilm));
    }
}

void PathOCLRenderEngine::UpdateCounters() {
	elapsedTime = WallClockTime() - startTime;

	// Update the sample count statistic
	double totalCount = 0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		slg::ocl::pathocl::GPUTaskStats *stats = ((PathOCLRenderThread *)(renderThreads[i]))->gpuTaskStats;

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

#endif
