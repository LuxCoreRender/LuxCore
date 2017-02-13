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

#include "slg/engines/lightcpu/lightcpu.h"
#include "slg/engines/lightcpu/lightcpurenderstate.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightCPURenderEngine
//------------------------------------------------------------------------------

LightCPURenderEngine::LightCPURenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		CPUNoTileRenderEngine(rcfg, flm, flmMutex), sampleSplatter(NULL) {
	if (rcfg->scene->camera->GetType() == Camera::STEREO)
		throw std::runtime_error("Light render engine doesn't support stereo camera");

	InitFilm();
}

LightCPURenderEngine::~LightCPURenderEngine() {
	delete sampleSplatter;
}

void LightCPURenderEngine::InitFilm() {
	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->SetRadianceGroupCount(renderConfig->scene->lightDefs.GetLightGroupCount());
	film->Init();
}

RenderState *LightCPURenderEngine::GetRenderState() {
	return new LightCPURenderState(bootStrapSeed);
}

void LightCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	maxPathDepth = cfg.Get(GetDefaultProps().Get("light.maxdepth")).Get<int>();
	rrDepth = cfg.Get(GetDefaultProps().Get("light.russianroulette.depth")).Get<int>();
	rrImportanceCap = cfg.Get(GetDefaultProps().Get("light.russianroulette.cap")).Get<float>();

	//--------------------------------------------------------------------------
	// Restore render state if there is one
	//--------------------------------------------------------------------------

	if (startRenderState) {
		// Check if the render state is of the right type
		startRenderState->CheckEngineTag(GetObjectTag());

		LightCPURenderEngine *rs = (LightCPURenderEngine *)startRenderState;

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new LIGHTCPU seed: " + ToString(newSeed));
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

void LightCPURenderEngine::StopLockLess() {
	CPUNoTileRenderEngine::StopLockLess();

	delete sampleSplatter;
	sampleSplatter = NULL;
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties LightCPURenderEngine::ToProperties(const Properties &cfg) {
	return CPUNoTileRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("light.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("light.russianroulette.depth")) <<
			cfg.Get(GetDefaultProps().Get("light.russianroulette.cap")) <<
			Sampler::ToProperties(cfg);
}

RenderEngine *LightCPURenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new LightCPURenderEngine(rcfg, flm, flmMutex);
}

const Properties &LightCPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			CPUNoTileRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("light.maxdepth")(5) <<
			Property("light.russianroulette.depth")(3) <<
			Property("light.russianroulette.cap")(.5f);

	return props;
}
