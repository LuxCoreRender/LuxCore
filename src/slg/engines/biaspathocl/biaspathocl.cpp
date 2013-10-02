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

BiasPathOCLRenderEngine::BiasPathOCLRenderEngine(RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		PathOCLBaseRenderEngine(rcfg, flm, flmMutex, false) {
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
	maxPathDepth.depth = Max(1, cfg.GetInt("biaspath.pathdepth.total", 10));
	maxPathDepth.diffuseDepth = Max(0, cfg.GetInt("biaspath.pathdepth.diffuse", 1));
	maxPathDepth.glossyDepth = Max(0, cfg.GetInt("biaspath.pathdepth.glossy", 1));
	maxPathDepth.specularDepth = Max(0, cfg.GetInt("biaspath.pathdepth.specular", 2));

	// Samples settings
	aaSamples = Max(1, cfg.GetInt("biaspath.sampling.aa.size", 3));
	diffuseSamples = Max(0, cfg.GetInt("biaspath.sampling.diffuse.size", 2));
	glossySamples = Max(0, cfg.GetInt("biaspath.sampling.glossy.size", 2));
	specularSamples = Max(0, cfg.GetInt("biaspath.sampling.specular.size", 1));
	directLightSamples = Max(1, cfg.GetInt("biaspath.sampling.directlight.size", 1));

	// Clamping settings
	clampValueEnabled = cfg.GetBoolean("biaspath.clamping.enable", true);
	clampMaxValue = Max(0.f, cfg.GetFloat("biaspath.clamping.maxvalue", 10.f));

	// Light settings
	lowLightThreashold = Max(0.f, cfg.GetFloat("biaspath.lights.lowthreshold", .001f));
	nearStartLight = Max(0.f, cfg.GetFloat("biaspath.lights.nearstart", .001f));

	string lightStratType = cfg.GetString("biaspath.lights.samplingstrategy.type", "ALL");
	if (lightStratType == "ALL")
		lightSamplingStrategyONE = false;
	else if (lightStratType == "ONE")
		lightSamplingStrategyONE = true;
	else
		throw std::runtime_error("Unknown light sampling strategy type: " + lightStratType);

	tileRepository = new TileRepository(Max(renderConfig->cfg.GetInt("tile.size", 64), 8));
	tileRepository->enableProgressiveRefinement = cfg.GetBoolean("tile.progressiverefinement.enable", false);
	tileRepository->enableMultipassRendering = cfg.GetBoolean("tile.multipass.enable", false);
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

void BiasPathOCLRenderEngine::EndEditLockLess(const EditActionList &editActions) {
	if (editActions.Has(FILM_EDIT))
		InitPixelFilterDistribution();

	tileRepository->Clear();
	tileRepository->InitTiles(film->GetWidth(), film->GetHeight());
	printedRenderingTime = false;

	PathOCLBaseRenderEngine::EndEditLockLess(editActions);
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
		boost::unique_lock<boost::mutex> lock(engineMutex);

		if (!printedRenderingTime && tileRepository->done) {
			elapsedTime = WallClockTime() - startTime;
			SLG_LOG(boost::format("Rendering time: %.2f secs") % elapsedTime);
			printedRenderingTime = true;
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
