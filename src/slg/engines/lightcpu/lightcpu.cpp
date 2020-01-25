/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

LightCPURenderEngine::LightCPURenderEngine(const RenderConfig *rcfg) :
		CPUNoTileRenderEngine(rcfg), sampleSplatter(nullptr) {
	if (rcfg->scene->camera->GetType() == Camera::STEREO)
		throw std::runtime_error("Light render engine doesn't support stereo camera");
}

LightCPURenderEngine::~LightCPURenderEngine() {
	delete sampleSplatter;
}

void LightCPURenderEngine::InitFilm() {
	film->AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	film->SetRadianceGroupCount(renderConfig->scene->lightDefs.GetLightGroupCount());
	film->Init();
}

RenderState *LightCPURenderEngine::GetRenderState() {
	return new LightCPURenderState(bootStrapSeed);
}

void LightCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	CheckSamplersForNoTile(RenderEngineType2String(GetType()), cfg);

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
	}

	//--------------------------------------------------------------------------
	// Initialize the PathTracer class with rendering parameters
	//--------------------------------------------------------------------------

	pathTracer.ParseOptions(cfg, GetDefaultProps());
	// To avoid to trace only caustic light paths
	pathTracer.hybridBackForwardEnable = false;

	pathTracer.InitPixelFilterDistribution(pixelFilter);

	delete sampleSplatter;
	sampleSplatter = new FilmSampleSplatter(pixelFilter);

	//--------------------------------------------------------------------------

	CPUNoTileRenderEngine::StartLockLess();
}

void LightCPURenderEngine::StopLockLess() {
	CPUNoTileRenderEngine::StopLockLess();
	
	pathTracer.DeletePixelFilterDistribution();

	delete sampleSplatter;
	sampleSplatter = NULL;
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties LightCPURenderEngine::ToProperties(const Properties &cfg) {
	return CPUNoTileRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			PathTracer::ToProperties(cfg) <<
			Sampler::ToProperties(cfg);
}

RenderEngine *LightCPURenderEngine::FromProperties(const RenderConfig *rcfg) {
	return new LightCPURenderEngine(rcfg);
}

const Properties &LightCPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			CPUNoTileRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			PathTracer::GetDefaultProps();

	return props;
}
