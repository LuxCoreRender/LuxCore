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
		CPUNoTileRenderEngine(rcfg, flm, flmMutex) {
	if (rcfg->scene->camera->GetType() == Camera::STEREO)
		throw std::runtime_error("Light render engine doesn't support stereo camera");

	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	film->Init();

}

void LightCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	maxPathDepth = cfg.Get(Property("light.maxdepth")(
			cfg.Get(Property("path.maxdepth")(5)).Get<int>())).Get<int>();
	rrDepth = cfg.Get(Property("light.russianroulette.depth")(
			cfg.Get(Property("path.russianroulette.depth")(3)).Get<int>())).Get<int>();
	rrImportanceCap = cfg.Get(Property("light.russianroulette.cap")(
			cfg.Get(Property("path.russianroulette.cap")(.5f)).Get<float>())).Get<float>();

	CPUNoTileRenderEngine::StartLockLess();
}
