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

#include "slg/engines/tilepathcpu/tilepathcpu.h"
#include "slg/engines/tilepathcpu/tilepathcpurenderstate.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TilePathCPURenderEngine
//------------------------------------------------------------------------------

TilePathCPURenderEngine::TilePathCPURenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		CPUTileRenderEngine(rcfg, flm, flmMutex) {
	pixelFilterDistribution = NULL;

	InitFilm();
}

TilePathCPURenderEngine::~TilePathCPURenderEngine() {
	delete pixelFilterDistribution;
}

void TilePathCPURenderEngine::InitFilm() {
	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->SetRadianceGroupCount(renderConfig->scene->lightDefs.GetLightGroupCount());
	film->Init();
}

void TilePathCPURenderEngine::PrintSamplesInfo() const {
	// There is pretty much the same method in TilePathOCLRenderEngine

	// Pixel samples
	const u_int aaSamplesCount = aaSamples * aaSamples;
	SLG_LOG("[TilePathCPURenderEngine] Pixel samples: " << aaSamplesCount);

	// Diffuse samples
	const int maxDiffusePathDepth = Max<int>(0, Min<int>(maxPathDepth.depth, maxPathDepth.diffuseDepth - 1));
	const u_int diffuseSamplesCount = aaSamplesCount * (diffuseSamples * diffuseSamples);
	const u_int maxDiffuseSamplesCount = diffuseSamplesCount * maxDiffusePathDepth;
	SLG_LOG("[TilePathCPURenderEngine] Diffuse samples: " << diffuseSamplesCount <<
			" (with max. bounces " << maxDiffusePathDepth <<": " << maxDiffuseSamplesCount << ")");

	// Glossy samples
	const int maxGlossyPathDepth = Max<int>(0, Min<int>(maxPathDepth.depth, maxPathDepth.glossyDepth - 1));
	const u_int glossySamplesCount = aaSamplesCount * (glossySamples * glossySamples);
	const u_int maxGlossySamplesCount = glossySamplesCount * maxGlossyPathDepth;
	SLG_LOG("[TilePathCPURenderEngine] Glossy samples: " << glossySamplesCount <<
			" (with max. bounces " << maxGlossyPathDepth <<": " << maxGlossySamplesCount << ")");

	// Specular samples
	const int maxSpecularPathDepth = Max<int>(0, Min<int>(maxPathDepth.depth, maxPathDepth.specularDepth - 1));
	const u_int specularSamplesCount = aaSamplesCount * (specularSamples * specularSamples);
	const u_int maxSpecularSamplesCount = specularSamplesCount * maxSpecularPathDepth;
	SLG_LOG("[TilePathCPURenderEngine] Specular samples: " << specularSamplesCount <<
			" (with max. bounces " << maxSpecularPathDepth <<": " << maxSpecularSamplesCount << ")");

	// Direct light samples
	const u_int directLightSamplesCount = aaSamplesCount * firstVertexLightSampleCount *
			(directLightSamples * directLightSamples) * renderConfig->scene->lightDefs.GetSize();
	SLG_LOG("[TilePathCPURenderEngine] Direct light samples on first hit: " << directLightSamplesCount);

	// Total samples for a pixel with hit on diffuse surfaces
	SLG_LOG("[TilePathCPURenderEngine] Total samples for a pixel with hit on diffuse surfaces: " <<
			// Direct light sampling on first hit
			directLightSamplesCount +
			// Diffuse samples
			maxDiffuseSamplesCount +
			// Direct light sampling for diffuse samples
			diffuseSamplesCount * Max<int>(0, maxDiffusePathDepth - 1));
}

void TilePathCPURenderEngine::InitPixelFilterDistribution() {
	// Compile sample distribution
	delete pixelFilterDistribution;
	pixelFilterDistribution = new FilterDistribution(pixelFilter, 64);
}

RenderState *TilePathCPURenderEngine::GetRenderState() {
	return new TilePathCPURenderState(bootStrapSeed, tileRepository);
}

void TilePathCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	// Path depth settings
	maxPathDepth.depth = Max(0, cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.total")).Get<int>());
	maxPathDepth.diffuseDepth = Max(0, cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.diffuse")).Get<int>());
	maxPathDepth.glossyDepth = Max(0, cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.glossy")).Get<int>());
	maxPathDepth.specularDepth = Max(0, cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.specular")).Get<int>());

	// Samples settings
	aaSamples = Max(1, cfg.Get(GetDefaultProps().Get("tilepath.sampling.aa.size")).Get<int>());
	diffuseSamples = Max(0, cfg.Get(GetDefaultProps().Get("tilepath.sampling.diffuse.size")).Get<int>());
	glossySamples = Max(0, cfg.Get(GetDefaultProps().Get("tilepath.sampling.glossy.size")).Get<int>());
	specularSamples = Max(0, cfg.Get(GetDefaultProps().Get("tilepath.sampling.specular.size")).Get<int>());
	directLightSamples = Max(1, cfg.Get(GetDefaultProps().Get("tilepath.sampling.directlight.size")).Get<int>());

	// Clamping settings
	// clamping.radiance.maxvalue is the old radiance clamping, now converted in variance clamping
	sqrtVarianceClampMaxValue = cfg.Get(Property("tilepath.clamping.radiance.maxvalue")(0.f)).Get<float>();
	if (cfg.IsDefined("tilepath.clamping.variance.maxvalue"))
		sqrtVarianceClampMaxValue = cfg.Get(GetDefaultProps().Get("tilepath.clamping.variance.maxvalue")).Get<float>();
	sqrtVarianceClampMaxValue = Max(0.f, sqrtVarianceClampMaxValue);
	pdfClampValue = Max(0.f, cfg.Get(GetDefaultProps().Get("tilepath.clamping.pdf.value")).Get<float>());

	// Light settings
	firstVertexLightSampleCount = Max(1, cfg.Get(GetDefaultProps().Get("tilepath.lights.firstvertexsamples")).Get<int>());

	forceBlackBackground = cfg.Get(GetDefaultProps().Get("tilepath.forceblackbackground.enable")).Get<bool>();

	PrintSamplesInfo();

	InitPixelFilterDistribution();

	//--------------------------------------------------------------------------
	// Restore render state if there is one
	//--------------------------------------------------------------------------

	if (startRenderState) {
		// Check if the render state is of the right type
		startRenderState->CheckEngineTag(GetObjectTag());

		TilePathCPURenderState *rs = (TilePathCPURenderState *)startRenderState;

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new TILEPATHCPU seed: " + ToString(newSeed));
		SetSeed(newSeed);

		tileRepository = rs->tileRepository;
		
		delete startRenderState;
		startRenderState = NULL;
	} else {
		film->Reset();

		tileRepository = TileRepository::FromProperties(renderConfig->cfg);
		tileRepository->varianceClamping = VarianceClamping(sqrtVarianceClampMaxValue);
		tileRepository->InitTiles(*film);
	}

	//--------------------------------------------------------------------------

	CPURenderEngine::StartLockLess();
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties TilePathCPURenderEngine::ToProperties(const Properties &cfg) {
	return CPUTileRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.total")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.diffuse")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.glossy")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.pathdepth.specular")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.sampling.aa.size")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.sampling.diffuse.size")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.sampling.glossy.size")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.sampling.specular.size")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.sampling.directlight.size")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.clamping.variance.maxvalue")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.clamping.pdf.value")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.lights.firstvertexsamples")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.forceblackbackground.enable"));
}

RenderEngine *TilePathCPURenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new TilePathCPURenderEngine(rcfg, flm, flmMutex);
}

const Properties &TilePathCPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			CPUTileRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("tilepath.pathdepth.total")(10) <<
			Property("tilepath.pathdepth.diffuse")(4) <<
			Property("tilepath.pathdepth.glossy")(3) <<
			Property("tilepath.pathdepth.specular")(3) <<
			Property("tilepath.sampling.aa.size")(3) <<
			Property("tilepath.sampling.diffuse.size")(2) <<
			Property("tilepath.sampling.glossy.size")(2) <<
			Property("tilepath.sampling.specular.size")(2) <<
			Property("tilepath.sampling.directlight.size")(1) <<
			Property("tilepath.clamping.variance.maxvalue")(0.f) <<
			Property("tilepath.clamping.pdf.value")(0.f) <<
			Property("tilepath.lights.firstvertexsamples")(4) <<
			Property("tilepath.forceblackbackground.enable")(false);

	return props;
}
