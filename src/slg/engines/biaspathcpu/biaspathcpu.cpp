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
	pixelFilterDistribution = new FilterDistribution(pixelFilter, 64);
}

void BiasPathCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	// Path depth settings
	maxPathDepth.depth = Max(0, cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.total")).Get<int>());
	maxPathDepth.diffuseDepth = Max(0, cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.diffuse")).Get<int>());
	maxPathDepth.glossyDepth = Max(0, cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.glossy")).Get<int>());
	maxPathDepth.specularDepth = Max(0, cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.specular")).Get<int>());

	// Samples settings
	aaSamples = Max(1, cfg.Get(GetDefaultProps().Get("biaspath.sampling.aa.size")).Get<int>());
	diffuseSamples = Max(0, cfg.Get(GetDefaultProps().Get("biaspath.sampling.diffuse.size")).Get<int>());
	glossySamples = Max(0, cfg.Get(GetDefaultProps().Get("biaspath.sampling.glossy.size")).Get<int>());
	specularSamples = Max(0, cfg.Get(GetDefaultProps().Get("biaspath.sampling.specular.size")).Get<int>());
	directLightSamples = Max(1, cfg.Get(GetDefaultProps().Get("biaspath.sampling.directlight.size")).Get<int>());

	// Clamping settings
	// clamping.radiance.maxvalue is the old radiance clamping, now converted in variance clamping
	sqrtVarianceClampMaxValue = cfg.Get(Property("biaspath.clamping.radiance.maxvalue")(0.f)).Get<float>();
	if (cfg.IsDefined("biaspath.clamping.variance.maxvalue"))
		sqrtVarianceClampMaxValue = cfg.Get(GetDefaultProps().Get("biaspath.clamping.variance.maxvalue")).Get<float>();
	sqrtVarianceClampMaxValue = Max(0.f, sqrtVarianceClampMaxValue);
	pdfClampValue = Max(0.f, cfg.Get(GetDefaultProps().Get("biaspath.clamping.pdf.value")).Get<float>());

	// Light settings
	lowLightThreashold = Max(0.f, cfg.Get(GetDefaultProps().Get("biaspath.lights.lowthreshold")).Get<float>());
	nearStartLight = Max(0.f, cfg.Get(GetDefaultProps().Get("biaspath.lights.nearstart")).Get<float>());
	firstVertexLightSampleCount = Max(1, cfg.Get(GetDefaultProps().Get("biaspath.lights.firstvertexsamples")).Get<int>());

	PrintSamplesInfo();

	InitPixelFilterDistribution();

	//--------------------------------------------------------------------------
	// Tile related parameters
	//--------------------------------------------------------------------------

	film->Reset();

	tileRepository = TileRepository::FromProperties(renderConfig->cfg);
	tileRepository->varianceClamping = VarianceClamping(sqrtVarianceClampMaxValue);
	tileRepository->InitTiles(*film);

	CPURenderEngine::StartLockLess();
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties BiasPathCPURenderEngine::ToProperties(const Properties &cfg) {
	return CPUTileRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.total")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.diffuse")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.glossy")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.pathdepth.specular")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.aa.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.diffuse.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.glossy.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.specular.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.sampling.directlight.size")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.clamping.variance.maxvalue")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.clamping.pdf.value")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.lights.lowthreshold")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.lights.nearstart")) <<
			cfg.Get(GetDefaultProps().Get("biaspath.lights.firstvertexsamples"));
}

RenderEngine *BiasPathCPURenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new BiasPathCPURenderEngine(rcfg, flm, flmMutex);
}

Properties BiasPathCPURenderEngine::GetDefaultProps() {
	static Properties props = CPURenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("biaspath.pathdepth.total")(10) <<
			Property("biaspath.pathdepth.diffuse")(4) <<
			Property("biaspath.pathdepth.glossy")(3) <<
			Property("biaspath.pathdepth.specular")(3) <<
			Property("biaspath.sampling.aa.size")(3) <<
			Property("biaspath.sampling.diffuse.size")(2) <<
			Property("biaspath.sampling.glossy.size")(2) <<
			Property("biaspath.sampling.specular.size")(2) <<
			Property("biaspath.sampling.directlight.size")(1) <<
			Property("biaspath.clamping.variance.maxvalue")(0.f) <<
			Property("biaspath.clamping.pdf.value")(0.f) <<
			Property("biaspath.lights.lowthreshold")(0.f) <<
			Property("biaspath.lights.nearstart")(.001f) <<
			Property("biaspath.lights.firstvertexsamples")(4);

	return props;
}

//------------------------------------------------------------------------------
// PathDepthInfo
//------------------------------------------------------------------------------

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
