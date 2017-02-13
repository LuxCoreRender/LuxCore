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

#include "slg/engines/bidircpu/bidircpu.h"
#include "slg/engines/bidircpu/bidircpurenderstate.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiDirCPURenderEngine
//------------------------------------------------------------------------------

BiDirCPURenderEngine::BiDirCPURenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		CPUNoTileRenderEngine(rcfg, flm, flmMutex), sampleSplatter(NULL) {
	if (rcfg->scene->camera->GetType() == Camera::STEREO)
		throw std::runtime_error("BiDir render engine doesn't support stereo camera");

	lightPathsCount = 1;
	baseRadius = 0.f;
	radiusAlpha = 0.f;

	InitFilm();
}

void BiDirCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	maxEyePathDepth = (u_int)Max(1, cfg.Get(GetDefaultProps().Get("path.maxdepth")).Get<int>());
	maxLightPathDepth = (u_int)Max(1, cfg.Get(GetDefaultProps().Get("light.maxdepth")).Get<int>());
	
	rrDepth = (u_int)Max(1, cfg.Get(GetDefaultProps().Get("path.russianroulette.depth")).Get<int>());
	rrImportanceCap = Clamp(cfg.Get(GetDefaultProps().Get("path.russianroulette.cap")).Get<float>(), 0.f, 1.f);

	//--------------------------------------------------------------------------
	// Restore render state if there is one
	//--------------------------------------------------------------------------

	if (startRenderState) {
		// Check if the render state is of the right type
		startRenderState->CheckEngineTag(GetObjectTag());

		BiDirCPURenderState *rs = (BiDirCPURenderState *)startRenderState;

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new BIDIRCPU seed: " + ToString(newSeed));
		SetSeed(newSeed);
		
		delete startRenderState;
		startRenderState = NULL;

		hasStartFilm = true;
	} else
		hasStartFilm = false;

	//--------------------------------------------------------------------------

	delete sampleSplatter;
	sampleSplatter = new FilmSampleSplatter(pixelFilter);

	CPUNoTileRenderEngine::StartLockLess();
}

void BiDirCPURenderEngine::InitFilm() {
	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->SetRadianceGroupCount(renderConfig->scene->lightDefs.GetLightGroupCount());
	film->Init();
}

void BiDirCPURenderEngine::StopLockLess() {
	CPUNoTileRenderEngine::StopLockLess();

	delete sampleSplatter;
	sampleSplatter = NULL;
}

RenderState *BiDirCPURenderEngine::GetRenderState() {
	return new BiDirCPURenderState(bootStrapSeed);
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties BiDirCPURenderEngine::ToProperties(const Properties &cfg) {
	return CPUNoTileRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("path.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("light.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.depth")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.cap")) <<
			Sampler::ToProperties(cfg);
}

RenderEngine *BiDirCPURenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new BiDirCPURenderEngine(rcfg, flm, flmMutex);
}

const Properties &BiDirCPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			CPUNoTileRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("path.maxdepth")(5) <<
			Property("light.maxdepth")(5) <<
			Property("path.russianroulette.depth")(3) <<
			Property("path.russianroulette.cap")(.5f);

	return props;
}
