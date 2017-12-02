/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#include "slg/samplers/tilepathsampler.h"
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
	InitFilm();
}

TilePathCPURenderEngine::~TilePathCPURenderEngine() {
}

void TilePathCPURenderEngine::InitFilm() {
	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->SetRadianceGroupCount(renderConfig->scene->lightDefs.GetLightGroupCount());
	film->Init();
}

RenderState *TilePathCPURenderEngine::GetRenderState() {
	return new TilePathCPURenderState(bootStrapSeed, tileRepository);
}

void TilePathCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	const string samplerType = cfg.Get(Property("sampler.type")("")).Get<string>();
	if (samplerType != "TILEPATHSAMPLER")
		throw runtime_error("(RT)TILEPATHCPU render engine can use only TILEPATHSAMPLER");

	//--------------------------------------------------------------------------
	// Initialize the PathTracer class with rendering parameters
	//--------------------------------------------------------------------------

	aaSamples = Max(1, cfg.Get(GetDefaultProps().Get("tilepath.sampling.aa.size")).Get<int>());

	pathTracer.ParseOptions(cfg, GetDefaultProps());

	pathTracer.InitPixelFilterDistribution(pixelFilter);

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
		tileRepository->varianceClamping = VarianceClamping(pathTracer.sqrtVarianceClampMaxValue);
		tileRepository->InitTiles(*film);
	}

	//--------------------------------------------------------------------------

	CPURenderEngine::StartLockLess();
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties TilePathCPURenderEngine::ToProperties(const Properties &cfg) {
	Properties props;
	
	props <<
			CPUTileRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.sampling.aa.size")) <<
			PathTracer::ToProperties(cfg);

	return props;
}

RenderEngine *TilePathCPURenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new TilePathCPURenderEngine(rcfg, flm, flmMutex);
}

const Properties &TilePathCPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			CPUTileRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("tilepath.sampling.aa.size")(3) <<
			PathTracer::GetDefaultProps();

	return props;
}
