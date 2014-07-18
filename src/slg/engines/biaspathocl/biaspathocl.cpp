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

#include <boost/format.hpp>

#include "luxrays/core/oclintersectiondevice.h"

#include "slg/slg.h"
#include "slg/engines/biaspathocl/biaspathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiasPathOCLRenderEngine
//------------------------------------------------------------------------------

BiasPathOCLRenderEngine::BiasPathOCLRenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex,
			const bool realTime) :
		PathOCLBaseRenderEngine(rcfg, flm, flmMutex, realTime) {
	pixelFilterDistribution = NULL;
	tileRepository = NULL;
}

BiasPathOCLRenderEngine::~BiasPathOCLRenderEngine() {
	delete[] pixelFilterDistribution;
	delete tileRepository;
}

PathOCLBaseRenderThread *BiasPathOCLRenderEngine::CreateOCLThread(const u_int index,
	OpenCLIntersectionDevice *device) {
	return new BiasPathOCLRenderThread(index, device, this);
}

void BiasPathOCLRenderEngine::InitPixelFilterDistribution() {
	// Compile sample distribution
	delete[] pixelFilterDistribution;
	const Filter *filter = film->GetFilter();
	const FilterDistribution filterDistribution(filter, 64);
	pixelFilterDistribution = CompiledScene::CompileDistribution2D(
			filterDistribution.GetDistribution2D(), &pixelFilterDistributionSize);
}

void BiasPathOCLRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	printedRenderingTime = false;
	film->Reset();

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	// Path depth settings
	maxPathDepth.depth = Max(0, cfg.Get(Property("biaspath.pathdepth.total")(10)).Get<int>());
	maxPathDepth.diffuseDepth = Max(0, cfg.Get(Property("biaspath.pathdepth.diffuse")(2)).Get<int>());
	maxPathDepth.glossyDepth = Max(0, cfg.Get(Property("biaspath.pathdepth.glossy")(1)).Get<int>());
	maxPathDepth.specularDepth = Max(0, cfg.Get(Property("biaspath.pathdepth.specular")(2)).Get<int>());

	// Samples settings
	aaSamples = Max(1, cfg.Get(Property("biaspath.sampling.aa.size")(3)).Get<int>());
	diffuseSamples = Max(0, cfg.Get(Property("biaspath.sampling.diffuse.size")(2)).Get<int>());
	glossySamples = Max(0, cfg.Get(Property("biaspath.sampling.glossy.size")(2)).Get<int>());
	specularSamples = Max(0, cfg.Get(Property("biaspath.sampling.specular.size")(1)).Get<int>());
	directLightSamples = Max(1, cfg.Get(Property("biaspath.sampling.directlight.size")(1)).Get<int>());

	// Clamping settings
	radianceClampMaxValue = Max(0.f, cfg.Get(Property("biaspath.clamping.radiance.maxvalue")(10.f)).Get<float>());
	pdfClampValue = Max(0.f, cfg.Get(Property("biaspath.clamping.pdf.value")(0.f)).Get<float>());

	// Light settings
	lowLightThreashold = Max(0.f, cfg.Get(Property("biaspath.lights.lowthreshold")(0.f)).Get<float>());
	nearStartLight = Max(0.f, cfg.Get(Property("biaspath.lights.nearstart")(.001f)).Get<float>());
	firstVertexLightSampleCount = Max(1, cfg.Get(Property("biaspath.lights.firstvertexsamples")(4)).Get<int>());

	// Tile related parameters
	u_int defaultTileSize;
	if (GetEngineType() == RTBIASPATHOCL) {
		// Check if I'm going to use a single device
		if (intersectionDevices.size() == 1) {
			// The best configuration, with a single device, is to use a tile
			// as large as the complete image
			defaultTileSize = Max(film->GetWidth(), film->GetHeight());
		} else
			defaultTileSize = Max(film->GetWidth(), film->GetHeight()) / 4;
	} else
		defaultTileSize = 32;
	tileRepository = new TileRepository(Max(renderConfig->cfg.Get(Property("tile.size")(defaultTileSize)).Get<u_int>(), 8u));

	if (GetEngineType() == RTBIASPATHOCL)
		tileRepository->enableMultipassRendering = false;
	else
		tileRepository->enableMultipassRendering = cfg.Get(Property("tile.multipass.enable")(true)).Get<bool>();
	tileRepository->convergenceTestThreshold = cfg.Get(Property("tile.multipass.convergencetest.threshold")(.04f)).Get<float>();
	tileRepository->convergenceTestThresholdReduction = cfg.Get(Property("tile.multipass.convergencetest.threshold.reduction")(0.f)).Get<float>();
	tileRepository->totalSamplesPerPixel = aaSamples * aaSamples;

	tileRepository->InitTiles(film);

	taskCount = tileRepository->tileSize * tileRepository->tileSize * tileRepository->totalSamplesPerPixel;

	InitPixelFilterDistribution();
	
	PathOCLBaseRenderEngine::StartLockLess();
}

void BiasPathOCLRenderEngine::StopLockLess() {
	PathOCLBaseRenderEngine::StopLockLess();

	delete[] pixelFilterDistribution;
	pixelFilterDistribution = NULL;
	delete tileRepository;
	tileRepository = NULL;
}

void BiasPathOCLRenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	if (GetEngineType() != RTBIASPATHOCL) {
		// RTBIASPATHOCL will InitTiles() on next frame
		tileRepository->Clear();
		tileRepository->InitTiles(film);
		printedRenderingTime = false;
	}

	PathOCLBaseRenderEngine::EndSceneEditLockLess(editActions);
}

void BiasPathOCLRenderEngine::UpdateCounters() {
	// Update the sample count statistic
	samplesCount = film->GetTotalSampleCount();

	// Update the ray count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < renderThreads.size(); ++i) {
		const BiasPathOCLRenderThread *thread = (BiasPathOCLRenderThread *)renderThreads[i];
		totalCount += thread->intersectionDevice->GetTotalRaysCount();
	}
	raysCount = totalCount;

	if (!tileRepository->done) {
		// Update the time only while rendering is not finished
		elapsedTime = WallClockTime() - startTime;
	} else
		convergence = 1.f;
}

#endif
