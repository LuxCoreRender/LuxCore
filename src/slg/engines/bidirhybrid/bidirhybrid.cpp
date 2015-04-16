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

#include "slg/engines/bidirhybrid/bidirhybrid.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiDirHybridRenderEngine
//------------------------------------------------------------------------------

BiDirHybridRenderEngine::BiDirHybridRenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		HybridRenderEngine(rcfg, flm, flmMutex) {
	// For classic BiDir, the count is always 1
	eyePathCount = 1;
	lightPathCount = 1;

	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->Init();
}

void BiDirHybridRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	maxEyePathDepth = cfg.Get(Property("path.maxdepth")(5)).Get<int>();
	maxLightPathDepth = cfg.Get(Property("light.maxdepth")(5)).Get<int>();
	rrDepth = cfg.Get(Property("light.russianroulette.depth")(
			cfg.Get(Property("path.russianroulette.depth")(3)).Get<int>())).Get<int>();
	rrImportanceCap = cfg.Get(Property("light.russianroulette.cap")(
			cfg.Get(Property("path.russianroulette.cap")(.5f)).Get<float>())).Get<float>();

	HybridRenderEngine::StartLockLess();
}
