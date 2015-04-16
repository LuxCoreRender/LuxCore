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
		CPUNoTileRenderEngine(rcfg, flm, flmMutex) {
	lightPathsCount = 1;
	baseRadius = 0.f;
	radiusAlpha = 0.f;

	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->Init();
}

void BiDirCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	maxEyePathDepth = (u_int)Max(1, cfg.Get(Property("path.maxdepth")(5)).Get<int>());
	maxLightPathDepth = (u_int)Max(1, cfg.Get(Property("light.maxdepth")(5)).Get<int>());
	rrDepth = (u_int)Max(1, cfg.Get(Property("light.russianroulette.depth")(
			cfg.Get(Property("path.russianroulette.depth")(3)).Get<int>())).Get<int>());
	rrImportanceCap = cfg.Get(Property("light.russianroulette.cap")(
			cfg.Get(Property("path.russianroulette.cap")(.5f)).Get<float>())).Get<float>();

	CPUNoTileRenderEngine::StartLockLess();
}
