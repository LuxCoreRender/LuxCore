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
	maxPathDepth.diffuseDepth = Max(0, cfg.Get(Property("biaspath.pathdepth.diffuse")(1)).Get<int>());
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
	lowLightThreashold = Max(0.f, cfg.Get(Property("biaspath.lights.lowthreshold")(.001f)).Get<float>());
	nearStartLight = Max(0.f, cfg.Get(Property("biaspath.lights.nearstart")(.001f)).Get<float>());

	string lightStratType = cfg.Get(Property("biaspath.lights.samplingstrategy.type")("ALL")).Get<string>();
	if (lightStratType == "ALL")
		lightSamplingStrategyONE = false;
	else if (lightStratType == "ONE")
		lightSamplingStrategyONE = true;
	else
		throw std::runtime_error("Unknown light sampling strategy type: " + lightStratType);

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

	if (GetEngineType() == RTBIASPATHOCL) {
		tileRepository->enableProgressiveRefinement = false;
		tileRepository->enableMultipassRendering = false;
	} else {
		tileRepository->enableProgressiveRefinement = cfg.Get(Property("tile.progressiverefinement.enable")(false)).Get<bool>();
		tileRepository->enableMultipassRendering = cfg.Get(Property("tile.multipass.enable")(false)).Get<bool>();
	}
	tileRepository->totalSamplesPerPixel = aaSamples * aaSamples; // Used for progressive rendering
	tileRepository->InitTiles(film->GetWidth(), film->GetHeight());

	taskCount = (tileRepository->enableProgressiveRefinement) ?
		(tileRepository->tileSize * tileRepository->tileSize) :
		(tileRepository->tileSize * tileRepository->tileSize * tileRepository->totalSamplesPerPixel);

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
		tileRepository->InitTiles(film->GetWidth(), film->GetHeight());
		printedRenderingTime = false;
	}

	PathOCLBaseRenderEngine::EndSceneEditLockLess(editActions);
}

const bool BiasPathOCLRenderEngine::NextTile(TileRepository::Tile **tile, const Film *tileFilm) {
	// Check if I have to add the tile to the film
	if (*tile) {
		boost::unique_lock<boost::mutex> lock(*filmMutex);

		film->AddFilm(*tileFilm,
				0, 0,
				Min(tileRepository->tileSize, film->GetWidth() - (*tile)->xStart),
				Min(tileRepository->tileSize, film->GetHeight() - (*tile)->yStart),
				(*tile)->xStart, (*tile)->yStart);
	}

	if (!tileRepository->NextTile(tile, film->GetWidth(), film->GetHeight())) {
		// RTBIASPATHOCL would end in dead-lock on engineMutex
		if (GetEngineType() != RTBIASPATHOCL) {
			boost::unique_lock<boost::mutex> lock(engineMutex);

			if (!printedRenderingTime && tileRepository->done) {
				elapsedTime = WallClockTime() - startTime;
				SLG_LOG(boost::format("Rendering time: %.2f secs") % elapsedTime);
				printedRenderingTime = true;
			}
		}

		return false;
	} else
		return true;
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

	// Update the time only until when the rendering is not finished
	if (!tileRepository->done)
		elapsedTime = WallClockTime() - startTime;
}

#endif
