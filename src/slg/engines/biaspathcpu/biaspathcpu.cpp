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

#include <limits>

#include "slg/engines/biaspathcpu/biaspathcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathCPURenderEngine
//------------------------------------------------------------------------------

BiasPathCPURenderEngine::BiasPathCPURenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		CPUTileRenderEngine(rcfg, flm, flmMutex) {
	pixelFilterDistribution = NULL;

	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->SetRadianceGroupCount(rcfg->scene->lightGroupCount);
	film->Init();
}

BiasPathCPURenderEngine::~BiasPathCPURenderEngine() {
	delete pixelFilterDistribution;
}

void BiasPathCPURenderEngine::InitPixelFilterDistribution() {
	// Compile sample distribution
	delete pixelFilterDistribution;
	pixelFilterDistribution = new FilterDistribution(film->GetFilter(), 64);
}

void BiasPathCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	// Path depth settings
	maxPathDepth.depth = Max(0, cfg.GetInt("biaspath.pathdepth.total", 10));
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
	radianceClampMaxValue = Max(0.f, cfg.GetFloat("biaspath.clamping.radiance.maxvalue", 10.f));
	pdfClampValue = Max(0.f, cfg.GetFloat("biaspath.clamping.pdf.value", 0.f));

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

	InitPixelFilterDistribution();

	//--------------------------------------------------------------------------
	// CPUTileRenderEngine initialization
	//--------------------------------------------------------------------------

	film->Reset();
	tileRepository = new TileRepository(Max(renderConfig->cfg.GetInt("tile.size", 32), 8));
	tileRepository->enableProgressiveRefinement = cfg.GetBoolean("tile.progressiverefinement.enable", false);
	tileRepository->enableMultipassRendering = cfg.GetBoolean("tile.multipass.enable", false);
	tileRepository->totalSamplesPerPixel = aaSamples * aaSamples; // Used for progressive rendering

	tileRepository->InitTiles(film->GetWidth(), film->GetHeight());
	printedRenderingTime = false;

	CPURenderEngine::StartLockLess();
}

PathDepthInfo::PathDepthInfo() {
	depth = 0;
	diffuseDepth = 0;
	glossyDepth = 0;
	specularDepth = 0;
}

void PathDepthInfo::IncDepths(const BSDFEvent event) {
	++depth;
	if (event & DIFFUSE)
		++diffuseDepth;
	if (event & GLOSSY)
		++glossyDepth;
	if (event & SPECULAR)
		++specularDepth;
}

bool PathDepthInfo::CheckDepths(const PathDepthInfo &maxPathDepth) const {
	return ((depth <= maxPathDepth.depth) &&
			(diffuseDepth <= maxPathDepth.diffuseDepth) &&
			(glossyDepth <= maxPathDepth.glossyDepth) &&
			(specularDepth <= maxPathDepth.specularDepth));
}
