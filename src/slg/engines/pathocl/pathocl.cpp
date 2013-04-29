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
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/core/oclintersectiondevice.h"

#include "slg/slg.h"
#include "slg/engines/pathocl/pathocl.h"
#include "slg/engines/pathocl/rtpathocl.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/film/filter.h"
#include "slg/sdl/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLRenderEngine
//------------------------------------------------------------------------------

PathOCLRenderEngine::PathOCLRenderEngine(RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		OCLRenderEngine(rcfg, flm, flmMutex) {
	film->SetPerPixelNormalizedBufferFlag(true);
	film->SetPerScreenNormalizedBufferFlag(false);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->Init();

	const Properties &cfg = renderConfig->cfg;
	compiledScene = NULL;

	//--------------------------------------------------------------------------
	// Allocate devices
	//--------------------------------------------------------------------------

	std::vector<IntersectionDevice *> devs = ctx->AddIntersectionDevices(selectedDeviceDescs);

	// Check if I have to disable image storage and set max. QBVH stack size
	const bool enableImageStorage = cfg.GetBoolean("accelerator.imagestorage.enable", true);
	const size_t qbvhStackSize = cfg.GetInt("accelerator.qbvh.stacksize.max",
			OCLRenderEngine::GetQBVHEstimatedStackSize(*(renderConfig->scene->dataSet)));
	SLG_LOG("OpenCL Devices used:");
	for (size_t i = 0; i < devs.size(); ++i) {
		SLG_LOG("[" << devs[i]->GetName() << "]");
		devs[i]->SetMaxStackSize(qbvhStackSize);
		intersectionDevices.push_back(devs[i]);

		OpenCLIntersectionDevice *oclIntersectionDevice = (OpenCLIntersectionDevice *)(devs[i]);
		oclIntersectionDevice->SetEnableImageStorage(enableImageStorage);
		// Disable the support for hybrid rendering
		oclIntersectionDevice->SetDataParallelSupport(false);

		// Check if OpenCL 1.1 is available
		SLG_LOG("  Device OpenCL version: " << oclIntersectionDevice->GetDeviceDesc()->GetOpenCLVersion());
		if (!oclIntersectionDevice->GetDeviceDesc()->IsOpenCL_1_1()) {
			// NVIDIA drivers report OpenCL 1.0 even if they are 1.1 so I just
			// print a warning instead of throwing an exception
			SLG_LOG("WARNING: OpenCL version 1.1 or better is required. Device " + devs[i]->GetName() + " may not work.");
		}
	}

	// Set the LuxRays SataSet
	ctx->SetDataSet(renderConfig->scene->dataSet);

	//--------------------------------------------------------------------------
	// Setup render threads array
	//--------------------------------------------------------------------------

	const size_t renderThreadCount = intersectionDevices.size();
	SLG_LOG("Configuring "<< renderThreadCount << " CPU render threads");
	renderThreads.resize(renderThreadCount, NULL);
}

PathOCLRenderEngine::~PathOCLRenderEngine() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];

	delete compiledScene;
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

	if (!cfg.IsDefined("opencl.task.count") && dynamic_cast<RTPathOCLRenderEngine *>(this)) {
		// In this case, I will tune task count for RTPATHOCL

		// The task count is set in order to have one task per pixel
		// +2 because the frame buffer on the dice is 2 pixels larger
		taskCount = (film->GetWidth() + 2) * (film->GetHeight() + 2) / intersectionDevices.size();
	} else 
		taskCount = cfg.GetInt("opencl.task.count", 65536);
	// I don't know yet the workgroup size of each device so I can not
	// round up task count to be a multiply of workgroups size of all devices
	// used. rounding to 2048 is a simple trick base don the assumption that
	// workgroup size is a power of 2 and <= 2048.
	taskCount = RoundUp<u_int>(taskCount, 2048);
	SLG_LOG("[PathOCLRenderEngine] OpenCL task count: " << taskCount);

	if (cfg.IsDefined("opencl.memory.maxpagesize"))
		maxMemPageSize = cfg.GetSize("opencl.memory.maxpagesize", 512 * 1024 * 1024);
	else {
		// Look for the max. page size allowed
		maxMemPageSize = ((OpenCLIntersectionDevice *)(intersectionDevices[0]))->GetDeviceDesc()->GetMaxMemoryAllocSize();
		for (u_int i = 1; i < intersectionDevices.size(); ++i)
			maxMemPageSize = Min(maxMemPageSize, ((OpenCLIntersectionDevice *)(intersectionDevices[i]))->GetDeviceDesc()->GetMaxMemoryAllocSize());
	}
	SLG_LOG("[PathOCLRenderEngine] OpenCL max. page memory size: " << maxMemPageSize / 1024 << "Kbytes");

	maxPathDepth = cfg.GetInt("path.maxdepth", 5);
	rrDepth = cfg.GetInt("path.russianroulette.depth", 3);
	rrImportanceCap = cfg.GetFloat("path.russianroulette.cap", .5f);

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

	const string filterType = cfg.GetString("path.filter.type", "NONE");
	const float filterWidthX = cfg.GetFloat("path.filter.width.x", 1.5f);
	const float filterWidthY = cfg.GetFloat("path.filter.width.y", 1.5f);
	if ((filterWidthX <= 0.f) || (filterWidthX > 1.5f))
		throw std::runtime_error("path.filter.width.x must be between 0.0 and 1.5");
	if ((filterWidthY <= 0.f) || (filterWidthY > 1.5f))
		throw std::runtime_error("path.filter.width.y must be between 0.0 and 1.5");

	filter = new slg::ocl::Filter();
	// Force pixel filter to NONE if I'm RTOPENCL
	if ((filterType.compare("NONE") == 0) || (dynamic_cast<RTPathOCLRenderEngine *>(this)))
		filter->type = slg::ocl::FILTER_NONE;
	else if (filterType.compare("BOX") == 0) {
		filter->type = slg::ocl::FILTER_BOX;
		filter->box.widthX = filterWidthX;
		filter->box.widthY = filterWidthY;
	} else if (filterType.compare("GAUSSIAN") == 0) {
		const float alpha = cfg.GetFloat("path.filter.alpha", 2.f);
		filter->type = slg::ocl::FILTER_GAUSSIAN;
		filter->gaussian.widthX = filterWidthX;
		filter->gaussian.widthY = filterWidthY;
		filter->gaussian.alpha = alpha;
	} else if (filterType.compare("MITCHELL") == 0) {
		const float B = cfg.GetFloat("path.filter.B", 1.f / 3.f);
		const float C = cfg.GetFloat("path.filter.C", 1.f / 3.f);
		filter->type = slg::ocl::FILTER_MITCHELL;
		filter->mitchell.widthX = filterWidthX;
		filter->mitchell.widthY = filterWidthY;
		filter->mitchell.B = B;
		filter->mitchell.C = C;
	} else
		throw std::runtime_error("Unknown path.filter.type: " + boost::lexical_cast<std::string>(filterType));

	usePixelAtomics = (cfg.GetInt("path.pixelatomics.enable", 0) != 0);	

	//--------------------------------------------------------------------------
	// Compile the scene
	//--------------------------------------------------------------------------

	compiledScene = new CompiledScene(renderConfig->scene, film, maxMemPageSize);

	//--------------------------------------------------------------------------
	// Start render threads
	//--------------------------------------------------------------------------

	const size_t renderThreadCount = intersectionDevices.size();
	SLG_LOG("Starting "<< renderThreadCount << " PathOCL render threads");
	for (size_t i = 0; i < renderThreadCount; ++i) {
		if (!renderThreads[i]) {
			renderThreads[i] = CreateOCLThread(i,
					(OpenCLIntersectionDevice *)(intersectionDevices[i]));
		}
	}

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
}

void PathOCLRenderEngine::StopLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();

	delete compiledScene;
	compiledScene = NULL;
}

void PathOCLRenderEngine::BeginEditLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->BeginEdit();
}

void PathOCLRenderEngine::EndEditLockLess(const EditActionList &editActions) {
	compiledScene->Recompile(editActions);

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->EndEdit(editActions);
}

void PathOCLRenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	const u_int imgWidth = film->GetWidth();
	const u_int imgHeight = film->GetHeight();

	film->Reset();

	for (u_int y = 0; y < imgHeight; ++y) {
		u_int pGPU = 1 + (y + 1) * (imgWidth + 2);

		for (u_int x = 0; x < imgWidth; ++x) {
			Spectrum radiance;
			float alpha = 0.0f;
			float count = 0.f;
			for (size_t i = 0; i < renderThreads.size(); ++i) {
				if (renderThreads[i]->frameBuffer) {
					radiance.r += renderThreads[i]->frameBuffer[pGPU].c.r;
					radiance.g += renderThreads[i]->frameBuffer[pGPU].c.g;
					radiance.b += renderThreads[i]->frameBuffer[pGPU].c.b;
					count += renderThreads[i]->frameBuffer[pGPU].count;
				}

				if (renderThreads[i]->alphaFrameBuffer)
					alpha += renderThreads[i]->alphaFrameBuffer[pGPU].alpha;
			}

			if ((count > 0) && !radiance.IsNaN()) {
				film->AddSampleCount(1.f);
				// -.5f is to align correctly the pixel after the splat
				film->SplatFiltered(PER_PIXEL_NORMALIZED, x - .5f, y - .5f,
						radiance / count, isnan(alpha) ? 0.f : alpha / count, count);
			}

			++pGPU;
		}
	}
}

void PathOCLRenderEngine::UpdateCounters() {
	// Update the sample count statistic
	unsigned long long totalCount = 0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		slg::ocl::GPUTaskStats *stats = renderThreads[i]->gpuTaskStats;

		for (size_t i = 0; i < taskCount; ++i)
			totalCount += stats[i].sampleCount;
	}

	samplesCount = totalCount;

	// Update the ray count statistic
	totalCount = 0.0;
	for (size_t i = 0; i < intersectionDevices.size(); ++i)
		totalCount += intersectionDevices[i]->GetTotalRaysCount();
	raysCount = totalCount;
}

#endif
