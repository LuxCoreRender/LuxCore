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
	film->SetRadianceGroupCount(rcfg->scene->lightDefs.GetLightGroupCount());
	film->Init();
}

BiasPathCPURenderEngine::~BiasPathCPURenderEngine() {
	delete pixelFilterDistribution;
}

void BiasPathCPURenderEngine::PrintSamplesInfo() const {
	// There is pretty much the same method in BiasPathOCLRenderEngine

	// Pixel samples
	const u_int aaSamplesCount = aaSamples * aaSamples;
	SLG_LOG("[BiasPathCPURenderEngine] Pixel samples: " << aaSamplesCount);

	// Diffuse samples
	const int maxDiffusePathDepth = Max<int>(0, Min<int>(maxPathDepth.depth, maxPathDepth.diffuseDepth - 1));
	const u_int diffuseSamplesCount = aaSamplesCount * (diffuseSamples * diffuseSamples);
	const u_int maxDiffuseSamplesCount = diffuseSamplesCount * maxDiffusePathDepth;
	SLG_LOG("[BiasPathCPURenderEngine] Diffuse samples: " << diffuseSamplesCount <<
			" (with max. bounces " << maxDiffusePathDepth <<": " << maxDiffuseSamplesCount << ")");

	// Glossy samples
	const int maxGlossyPathDepth = Max<int>(0, Min<int>(maxPathDepth.depth, maxPathDepth.glossyDepth - 1));
	const u_int glossySamplesCount = aaSamplesCount * (glossySamples * glossySamples);
	const u_int maxGlossySamplesCount = glossySamplesCount * maxGlossyPathDepth;
	SLG_LOG("[BiasPathCPURenderEngine] Glossy samples: " << glossySamplesCount <<
			" (with max. bounces " << maxGlossyPathDepth <<": " << maxGlossySamplesCount << ")");

	// Specular samples
	const int maxSpecularPathDepth = Max<int>(0, Min<int>(maxPathDepth.depth, maxPathDepth.specularDepth - 1));
	const u_int specularSamplesCount = aaSamplesCount * (specularSamples * specularSamples);
	const u_int maxSpecularSamplesCount = specularSamplesCount * maxSpecularPathDepth;
	SLG_LOG("[BiasPathCPURenderEngine] Specular samples: " << specularSamplesCount <<
			" (with max. bounces " << maxSpecularPathDepth <<": " << maxSpecularSamplesCount << ")");

	// Direct light samples
	const u_int directLightSamplesCount = aaSamplesCount * firstVertexLightSampleCount *
			(directLightSamples * directLightSamples) * renderConfig->scene->lightDefs.GetSize();
	SLG_LOG("[BiasPathCPURenderEngine] Direct light samples on first hit: " << directLightSamplesCount);

	// Total samples for a pixel with hit on diffuse surfaces
	SLG_LOG("[BiasPathCPURenderEngine] Total samples for a pixel with hit on diffuse surfaces: " <<
			// Direct light sampling on first hit
			directLightSamplesCount +
			// Diffuse samples
			maxDiffuseSamplesCount +
			// Direct light sampling for diffuse samples
			diffuseSamplesCount * Max<int>(0, maxDiffusePathDepth - 1));
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
	maxPathDepth.depth = Max(0, cfg.Get(Property("biaspath.pathdepth.total")(10)).Get<int>());
	maxPathDepth.diffuseDepth = Max(0, cfg.Get(Property("biaspath.pathdepth.diffuse")(4)).Get<int>());
	maxPathDepth.glossyDepth = Max(0, cfg.Get(Property("biaspath.pathdepth.glossy")(3)).Get<int>());
	maxPathDepth.specularDepth = Max(0, cfg.Get(Property("biaspath.pathdepth.specular")(3)).Get<int>());

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

	PrintSamplesInfo();

	InitPixelFilterDistribution();

	//--------------------------------------------------------------------------
	// CPUTileRenderEngine initialization
	//--------------------------------------------------------------------------

	film->Reset();

	u_int tileWidth = 32;
	u_int tileHeight = 32;
	if (renderConfig->cfg.IsDefined("tile.size"))
		tileWidth = tileHeight = Max(renderConfig->cfg.Get(Property("tile.size")(32)).Get<u_int>(), 8u);
	tileWidth = Max(renderConfig->cfg.Get(Property("tile.size.x")(tileWidth)).Get<u_int>(), 8u);
	tileHeight = Max(renderConfig->cfg.Get(Property("tile.size.y")(tileHeight)).Get<u_int>(), 8u);
	tileRepository = new TileRepository(tileWidth, tileHeight);

	tileRepository->maxPassCount = renderConfig->GetProperty("batch.haltdebug").Get<u_int>();
	tileRepository->enableMultipassRendering = cfg.Get(Property("tile.multipass.enable")(true)).Get<bool>();
	tileRepository->convergenceTestThreshold = cfg.Get(Property("tile.multipass.convergencetest.threshold")(.04f)).Get<float>();
	tileRepository->convergenceTestThresholdReduction = cfg.Get(Property("tile.multipass.convergencetest.threshold.reduction")(0.f)).Get<float>();
	tileRepository->totalSamplesPerPixel = aaSamples * aaSamples;

	tileRepository->InitTiles(*film);

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

bool PathDepthInfo::IsLastPathVertex(const PathDepthInfo &maxPathDepth, const BSDFEvent possibleEvents) const {
	return (depth + 1 == maxPathDepth.depth) ||
			((possibleEvents & DIFFUSE) && (diffuseDepth + 1 == maxPathDepth.diffuseDepth)) ||
			((possibleEvents & GLOSSY) && (glossyDepth + 1 == maxPathDepth.glossyDepth)) ||
			((possibleEvents & SPECULAR) && (specularDepth + 1 == maxPathDepth.specularDepth));
}
