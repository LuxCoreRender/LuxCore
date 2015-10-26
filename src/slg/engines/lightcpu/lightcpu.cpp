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

#include "slg/engines/lightcpu/lightcpu.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightCPURenderEngine
//------------------------------------------------------------------------------

LightCPURenderEngine::LightCPURenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		CPUNoTileRenderEngine(rcfg, flm, flmMutex), sampleSplatter(NULL) {
	if (rcfg->scene->camera->GetType() == Camera::STEREO)
		throw std::runtime_error("Light render engine doesn't support stereo camera");

	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->SetRadianceGroupCount(rcfg->scene->lightDefs.GetLightGroupCount());
	film->Init();

}

LightCPURenderEngine::~LightCPURenderEngine() {
	delete sampleSplatter;
}

void LightCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	maxPathDepth = cfg.Get(GetDefaultProps().Get("light.maxdepth")).Get<int>();
	rrDepth = cfg.Get(GetDefaultProps().Get("light.russianroulette.depth")).Get<int>();
	rrImportanceCap = cfg.Get(GetDefaultProps().Get("light.russianroulette.cap")).Get<float>();

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
	return CPURenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("light.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("light.russianroulette.depth")) <<
			cfg.Get(GetDefaultProps().Get("light.russianroulette.cap"));
}

RenderEngine *LightCPURenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new LightCPURenderEngine(rcfg, flm, flmMutex);
}

Properties LightCPURenderEngine::GetDefaultProps() {
	static Properties props = CPURenderEngine::GetDefaultProps() <<
			Property("light.maxdepth")(5) <<
			Property("light.russianroulette.depth")(3) <<
			Property("light.russianroulette.cap")(.5f);

	return props;
}
