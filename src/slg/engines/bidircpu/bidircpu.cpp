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

#include "slg/engines/bidircpu/bidircpu.h"

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

	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->SetRadianceGroupCount(rcfg->scene->lightDefs.GetLightGroupCount());
	film->Init();
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

	delete sampleSplatter;
	sampleSplatter = new FilmSampleSplatter(pixelFilter);

	CPUNoTileRenderEngine::StartLockLess();
}

void BiDirCPURenderEngine::StopLockLess() {
	CPUNoTileRenderEngine::StopLockLess();

	delete sampleSplatter;
	sampleSplatter = NULL;
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties BiDirCPURenderEngine::ToProperties(const Properties &cfg) {
	return CPURenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("path.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("light.maxdepth")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.depth")) <<
			cfg.Get(GetDefaultProps().Get("path.russianroulette.cap"));
}

RenderEngine *BiDirCPURenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new BiDirCPURenderEngine(rcfg, flm, flmMutex);
}

Properties BiDirCPURenderEngine::GetDefaultProps() {
	static Properties props = CPURenderEngine::GetDefaultProps() <<
			Property("path.maxdepth")(5) <<
			Property("light.maxdepth")(5) <<
			Property("path.russianroulette.depth")(3) <<
			Property("path.russianroulette.cap")(.5f);

	return props;
}
