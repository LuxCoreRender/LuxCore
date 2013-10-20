/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string.h>
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
		EndEdit(EditActionList());
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
		taskCount = cfg.GetInt("opencl.task.count", 65536);
		// I don't know yet the workgroup size of each device so I can not
		// round up task count to be a multiply of workgroups size of all devices
		// used. rounding to 2048 is a simple trick base don the assumption that
		// workgroup size is a power of 2 and <= 2048.
		taskCount = RoundUp<u_int>(taskCount, 2048);
		SLG_LOG("[PathOCLRenderEngine] OpenCL task count: " << taskCount);
	}

	//--------------------------------------------------------------------------
	// General path tracing settings
	//--------------------------------------------------------------------------	
	
	maxPathDepth = Max(1, cfg.GetInt("path.maxdepth", 5));
	rrDepth = Max(1, cfg.GetInt("path.russianroulette.depth", 3));
	rrImportanceCap = Clamp(cfg.GetFloat("path.russianroulette.cap", .5f), 0.f, 1.f);

	//--------------------------------------------------------------------------
	// Sampler
	//--------------------------------------------------------------------------

	sampler = new slg::ocl::Sampler();
	const SamplerType samplerType = Sampler::String2SamplerType(cfg.GetString("sampler.type", "RANDOM"));
	switch (samplerType) {
		case RANDOM:
			sampler->type = slg::ocl::RANDOM;
			break;
		case METROPOLIS: {
			const float largeMutationProbability = cfg.GetFloat("sampler.largesteprate", .4f);
			const float imageMutationRange = cfg.GetFloat("sampler.imagemutationrate", .1f);
			const float maxRejects = cfg.GetFloat("sampler.maxconsecutivereject", 512);

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
	// Force pixel filter to NONE if I'm RTOPENCL
	if ((filterType == FILTER_NONE) || (GetEngineType() == RTPATHOCL))
		filter->type = slg::ocl::FILTER_NONE;
	else if (filterType == FILTER_BOX) {
		filter->type = slg::ocl::FILTER_BOX;
		filter->box.widthX = Min(filmFilter->xWidth, 1.5f);
		filter->box.widthY = Min(filmFilter->yWidth, 1.5f);
	} else if (filterType == FILTER_GAUSSIAN) {
		filter->type = slg::ocl::FILTER_GAUSSIAN;
		filter->gaussian.widthX = Min(filmFilter->xWidth, 1.5f);
		filter->gaussian.widthY = Min(filmFilter->yWidth, 1.5f);
		filter->gaussian.alpha = ((GaussianFilter *)filmFilter)->alpha;
	} else if (filterType == FILTER_MITCHELL) {
		filter->type = slg::ocl::FILTER_MITCHELL;
		filter->mitchell.widthX = Min(filmFilter->xWidth, 1.5f);
		filter->mitchell.widthY = Min(filmFilter->yWidth, 1.5f);
		filter->mitchell.B = ((MitchellFilter *)filmFilter)->B;
		filter->mitchell.C = ((MitchellFilter *)filmFilter)->C;
	} else if (filterType == FILTER_MITCHELL_SS) {
		filter->type = slg::ocl::FILTER_MITCHELL;
		filter->mitchell.widthX = Min(filmFilter->xWidth, 1.5f);
		filter->mitchell.widthY = Min(filmFilter->yWidth, 1.5f);
		filter->mitchell.B = ((MitchellFilterSS *)filmFilter)->B;
		filter->mitchell.C = ((MitchellFilterSS *)filmFilter)->C;
	} else
		throw std::runtime_error("Unknown path.filter.type: " + boost::lexical_cast<std::string>(filterType));

	usePixelAtomics = (cfg.GetInt("path.pixelatomics.enable", 0) != 0);	

	PathOCLBaseRenderEngine::StartLockLess();
}

void PathOCLRenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	film->Reset();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		film->AddFilm(*(((PathOCLRenderThread *)(renderThreads[i]))->threadFilm));
}

void PathOCLRenderEngine::UpdateCounters() {
	elapsedTime = WallClockTime() - startTime;

	// Update the sample count statistic
	unsigned long long totalCount = 0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		slg::ocl::pathocl::GPUTaskStats *stats = ((PathOCLRenderThread *)(renderThreads[i]))->gpuTaskStats;

		for (size_t i = 0; i < taskCount; ++i)
			totalCount += stats[i].sampleCount;
	}

	samplesCount = totalCount;
	// This is a bit tricky because film is reseted in UpdateFilmLockLess()
	film->SetSampleCount(samplesCount);

	// Update the ray count statistic
	totalCount = 0.0;
	for (size_t i = 0; i < intersectionDevices.size(); ++i)
		totalCount += intersectionDevices[i]->GetTotalRaysCount();
	raysCount = totalCount;
}

#endif
