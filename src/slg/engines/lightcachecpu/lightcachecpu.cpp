/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "slg/engines/lightcachecpu/lightcachecpu.h"
#include "slg/engines/lightcachecpu/lightcachecpurenderstate.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightCacheCPURenderEngine
//------------------------------------------------------------------------------

LightCacheCPURenderEngine::LightCacheCPURenderEngine(const RenderConfig *rcfg) :
		CPUNoTileRenderEngine(rcfg), sampleSplatter(NULL) {
	if (rcfg->scene->camera->GetType() == Camera::STEREO)
		throw std::runtime_error("Light render engine doesn't support stereo camera");
}

LightCacheCPURenderEngine::~LightCacheCPURenderEngine() {
	delete sampleSplatter;
}

void LightCacheCPURenderEngine::InitFilm() {
	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->SetRadianceGroupCount(renderConfig->scene->lightDefs.GetLightGroupCount());
	film->Init();
}

RenderState *LightCacheCPURenderEngine::GetRenderState() {
	return new LightCacheCPURenderState(bootStrapSeed);
}

void LightCacheCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	CheckSamplersForNoTile(RenderEngineType2String(GetType()), cfg);

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	maxPathDepth = cfg.Get(GetDefaultProps().Get("path.maxdepth")).Get<int>();
	rrDepth = cfg.Get(GetDefaultProps().Get("path.russianroulette.depth")).Get<int>();
	rrImportanceCap = cfg.Get(GetDefaultProps().Get("path.russianroulette.cap")).Get<float>();

	// Clamping settings
	// clamping.radiance.maxvalue is the old radiance clamping, now converted in variance clamping
	sqrtVarianceClampMaxValue = cfg.Get(Property("path.clamping.radiance.maxvalue")(0.f)).Get<float>();
	if (cfg.IsDefined("path.clamping.variance.maxvalue"))
		sqrtVarianceClampMaxValue = cfg.Get(GetDefaultProps().Get("path.clamping.variance.maxvalue")).Get<float>();
	sqrtVarianceClampMaxValue = Max(0.f, sqrtVarianceClampMaxValue);

	//--------------------------------------------------------------------------
	// Restore render state if there is one
	//--------------------------------------------------------------------------

	if (startRenderState) {
		// Check if the render state is of the right type
		startRenderState->CheckEngineTag(GetObjectTag());

		LightCacheCPURenderEngine *rs = (LightCacheCPURenderEngine *)startRenderState;

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new LIGHTCACHECPU seed: " + ToString(newSeed));
		SetSeed(newSeed);
		
		delete startRenderState;
		startRenderState = NULL;
	}

	//--------------------------------------------------------------------------

	delete sampleSplatter;
	sampleSplatter = new FilmSampleSplatter(pixelFilter);

	CPUNoTileRenderEngine::StartLockLess();
}

void LightCacheCPURenderEngine::StopLockLess() {
	CPUNoTileRenderEngine::StopLockLess();

	delete sampleSplatter;
	sampleSplatter = NULL;
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties LightCacheCPURenderEngine::ToProperties(const Properties &cfg) {
	return CPUNoTileRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("path.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.depth")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.cap")) <<
			Sampler::ToProperties(cfg);
}

RenderEngine *LightCacheCPURenderEngine::FromProperties(const RenderConfig *rcfg) {
	return new LightCacheCPURenderEngine(rcfg);
}

const Properties &LightCacheCPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			CPUNoTileRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("path.maxdepth")(5) <<
			Property("path.russianroulette.depth")(3) <<
			Property("path.russianroulette.cap")(.5f) <<
			Property("path.clamping.variance.maxvalue")(0.f);

	return props;
}
