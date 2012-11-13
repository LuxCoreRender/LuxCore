/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

#include "smalllux.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "pathocl/pathocl.h"
#include "pathocl/kernels/kernels.h"
#include "renderconfig.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/accelerators/mqbvhaccel.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/opencl/intersectiondevice.h"

//------------------------------------------------------------------------------
// PathOCLRenderEngine
//------------------------------------------------------------------------------

PathOCLRenderEngine::PathOCLRenderEngine(RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		OCLRenderEngine(rcfg, flm, flmMutex) {
	const Properties &cfg = renderConfig->cfg;
	compiledScene = NULL;

	//--------------------------------------------------------------------------
	// Allocate devices
	//--------------------------------------------------------------------------

	std::vector<IntersectionDevice *> devs = ctx->AddIntersectionDevices(selectedDeviceDescs);

	// Check if I have to disable image storage and set max. QBVH stack size
	const bool forcedDisableImageStorage = (renderConfig->scene->GetAccelType() == 2);
	const size_t qbvhStackSize = cfg.GetInt("accelerator.qbvh.stacksize.max", 24);
	SLG_LOG("OpenCL Devices used:");
	for (size_t i = 0; i < devs.size(); ++i) {
		SLG_LOG("[" << devs[i]->GetName() << "]");
		devs[i]->SetMaxStackSize(qbvhStackSize);
		intersectionDevices.push_back(devs[i]);

		OpenCLIntersectionDevice *oclIntersectionDevice = (OpenCLIntersectionDevice *)(devs[i]);
		oclIntersectionDevice->DisableImageStorage(forcedDisableImageStorage);
		// Disable the support for hybrid rendering
		oclIntersectionDevice->SetHybridRenderingSupport(false);
	}

	// Set the Luxrays SataSet
	ctx->SetDataSet(renderConfig->scene->dataSet);

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	taskCount = RoundUpPow2(cfg.GetInt("opencl.task.count", 65536));
	SLG_LOG("[PathOCLRenderThread] OpenCL task count: " << taskCount);

	if (cfg.IsDefined("opencl.memory.maxpagesize"))
		maxMemPageSize = cfg.GetSize("opencl.memory.maxpagesize", 512 * 1024 * 1024);
	else {
		// Look for the max. page size allowed
		maxMemPageSize = ((OpenCLIntersectionDevice *)(intersectionDevices[0]))->GetDeviceDesc()->GetMaxMemoryAllocSize();
		for (u_int i = 1; i < intersectionDevices.size(); ++i)
			maxMemPageSize = Min(maxMemPageSize, ((OpenCLIntersectionDevice *)(intersectionDevices[i]))->GetDeviceDesc()->GetMaxMemoryAllocSize());
	}
	SLG_LOG("[PathOCLRenderThread] OpenCL max. page memory size: " << maxMemPageSize / 1024 << "Kbytes");
	
	maxPathDepth = cfg.GetInt("path.maxdepth", 5);
	maxDiffusePathVertexCount = cfg.GetInt("path.maxdiffusebounce", 5);
	rrDepth = cfg.GetInt("path.russianroulette.depth", 3);
	rrImportanceCap = cfg.GetFloat("path.russianroulette.cap", 0.125f);
	epsilon = cfg.GetFloat("scene.epsilon", .0001f);

	//--------------------------------------------------------------------------
	// Sampler
	//--------------------------------------------------------------------------

	const string samplerTypeName = cfg.GetString("path.sampler.type", "INLINED_RANDOM");
	if (samplerTypeName.compare("INLINED_RANDOM") == 0)
		sampler = new PathOCL::InlinedRandomSampler();
	else if (samplerTypeName.compare("RANDOM") == 0)
		sampler = new PathOCL::RandomSampler();
	else if (samplerTypeName.compare("STRATIFIED") == 0) {
		const unsigned int xSamples = cfg.GetInt("path.sampler.xsamples", 3);
		const unsigned int ySamples = cfg.GetInt("path.sampler.ysamples", 3);

		sampler = new PathOCL::StratifiedSampler(xSamples, ySamples);
	} else if (samplerTypeName.compare("METROPOLIS") == 0) {
		const float rate = cfg.GetFloat("path.sampler.largesteprate", .4f);
		const float reject = cfg.GetFloat("path.sampler.maxconsecutivereject", 512);
		const float mutationrate = cfg.GetFloat("path.sampler.imagemutationrate", .1f);

		sampler = new PathOCL::MetropolisSampler(rate, reject, mutationrate);
	} else
		throw std::runtime_error("Unknown path.sampler.type");

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

	if (filterType.compare("NONE") == 0)
		filter = new PathOCL::NoneFilter();
	else if (filterType.compare("BOX") == 0)
		filter = new PathOCL::BoxFilter(filterWidthX, filterWidthY);
	else if (filterType.compare("GAUSSIAN") == 0) {
		const float alpha = cfg.GetFloat("path.filter.alpha", 2.f);
		filter = new PathOCL::GaussianFilter(filterWidthX, filterWidthY, alpha);
	} else if (filterType.compare("MITCHELL") == 0) {
		const float B = cfg.GetFloat("path.filter.B", 1.f / 3.f);
		const float C = cfg.GetFloat("path.filter.C", 1.f / 3.f);
		filter = new PathOCL::MitchellFilter(filterWidthX, filterWidthY, B, C);
	} else
		throw std::runtime_error("Unknown path.filter.type");

	usePixelAtomics = (cfg.GetInt("path.pixelatomics.enable", 0) != 0);	

	const unsigned int seedBase = (unsigned int)(WallClockTime() / 1000.0);

	film->EnableOverlappedScreenBufferUpdate(true);

	//--------------------------------------------------------------------------
	// Create and start render threads
	//--------------------------------------------------------------------------

	const size_t renderThreadCount = intersectionDevices.size();
	SLG_LOG("Starting "<< renderThreadCount << " PathOCL render threads");
	for (size_t i = 0; i < renderThreadCount; ++i) {
		PathOCLRenderThread *t = new PathOCLRenderThread(i,
			seedBase + i * taskCount, i / (float)renderThreadCount,
			(OpenCLIntersectionDevice *)(intersectionDevices[i]),
			this);
		renderThreads.push_back(t);
	}
}

PathOCLRenderEngine::~PathOCLRenderEngine() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];

	delete sampler;
	delete filter;
}

void PathOCLRenderEngine::StartLockLess() {
	compiledScene = new CompiledScene(renderConfig->scene, film, maxMemPageSize);

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

	const unsigned int imgWidth = film->GetWidth();
	const unsigned int imgHeight = film->GetHeight();

	film->Reset();

	const bool isAlphaChannelEnabled = film->IsAlphaChannelEnabled();

	switch (film->GetFilterType()) {
		case FILTER_GAUSSIAN: {
			for (unsigned int y = 0; y < imgHeight; ++y) {
				unsigned int pGPU = 1 + (y + 1) * (imgWidth + 2);

				for (unsigned int x = 0; x < imgWidth; ++x) {
					Spectrum c;
					float alpha = 0.0f;
					float count = 0.f;
					for (size_t i = 0; i < renderThreads.size(); ++i) {
						if (renderThreads[i]->frameBuffer) {
							c += renderThreads[i]->frameBuffer[pGPU].c;
							count += renderThreads[i]->frameBuffer[pGPU].count;
						}

						if (renderThreads[i]->alphaFrameBuffer)
							alpha += renderThreads[i]->alphaFrameBuffer[pGPU].alpha;
					}

					if ((count > 0) && !c.IsNaN()) {
						c /= count;
						film->AddSampleCount(1.f);
						// -.5f is to align correctly the pixel after the splat
						film->SplatFiltered(PER_PIXEL_NORMALIZED, x - .5f, y - .5f, c);

						if (isAlphaChannelEnabled && !isnan(alpha))
							film->SplatFilteredAlpha(x - .5f, y - .5f, alpha / count);
					}

					++pGPU;
				}
			}
			break;
		}
		case FILTER_NONE: {
			for (unsigned int y = 0; y < imgHeight; ++y) {
				unsigned int pGPU = 1 + (y + 1) * (imgWidth + 2);

				for (unsigned int x = 0; x < imgWidth; ++x) {
					Spectrum c;
					float alpha = 0.0f;
					float count = 0.f;
					for (size_t i = 0; i < renderThreads.size(); ++i) {
						if (renderThreads[i]->frameBuffer) {
							c += renderThreads[i]->frameBuffer[pGPU].c;
							count += renderThreads[i]->frameBuffer[pGPU].count;
						}
						
						if (renderThreads[i]->alphaFrameBuffer)
							alpha += renderThreads[i]->alphaFrameBuffer[pGPU].alpha;
					}

					if ((count > 0) && !c.IsNaN()) {
						film->AddSampleCount(1.f);
						film->AddRadiance(PER_PIXEL_NORMALIZED, x, y, c / count, count);

						if (isAlphaChannelEnabled && !isnan(alpha))
							film->AddAlpha(x, y, alpha / count, 1.f);
					}

					++pGPU;
				}
			}
			break;
		}
		default:
			assert (false);
			break;
	}
}

#endif
