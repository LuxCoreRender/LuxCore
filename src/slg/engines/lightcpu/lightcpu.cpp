/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
#include "slg/cameras/camera.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightCPURenderEngine
//------------------------------------------------------------------------------

LightCPURenderEngine::LightCPURenderEngine(RenderConfigRef rcfg) :
		CPUNoTileRenderEngine(rcfg)  {
	if (rcfg.GetScene().GetCamera().GetType() == Camera::STEREO)
		throw std::runtime_error("Light render engine doesn't support stereo camera");
}

LightCPURenderEngine::~LightCPURenderEngine() {}

void LightCPURenderEngine::InitFilm() {
	GetFilm().AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	GetFilm().SetRadianceGroupCount(renderConfig.GetScene().GetLightSources().GetLightGroupCount());
	GetFilm().SetThreadCount(renderThreads.size());
	GetFilm().Init();
}

RenderStateSPtr LightCPURenderEngine::GetRenderState() {
	return std::make_shared<LightCPURenderState>(bootStrapSeed);
}

void LightCPURenderEngine::StartLockLess() {
	const auto& cfg = renderConfig.GetConfig();

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

		//auto rs = static_pointer_cast<LightCPURenderEngine>(startRenderState);
		auto rs = static_pointer_cast<LightCPURenderState>(startRenderState);

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new LIGHTCPU seed: " + ToString(newSeed));
		SetSeed(newSeed);

		startRenderState = nullptr;
	}

	//--------------------------------------------------------------------------
	// Initialize the PathTracer class with rendering parameters
	//--------------------------------------------------------------------------

	pathTracer.ParseOptions(cfg, *GetDefaultProps());
	// To avoid to trace only caustic light paths
	pathTracer.hybridBackForwardEnable = false;

	pathTracer.InitPixelFilterDistribution(GetPixelFilter());

	SetSampleSplatter(GetPixelFilter());

	//--------------------------------------------------------------------------

	CPUNoTileRenderEngine::StartLockLess();
}

void LightCPURenderEngine::StopLockLess() {
	CPUNoTileRenderEngine::StopLockLess();
	
	pathTracer.DeletePixelFilterDistribution();

	ResetSampleSplatter();
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

PropertiesUPtr LightCPURenderEngine::ToProperties(const Properties &cfg) {
	PropertiesUPtr props = CPUNoTileRenderEngine::ToProperties(cfg);
	
	*props <<
				cfg.Get(GetDefaultProps()->Get("renderengine.type")) <<
			*PathTracer::ToProperties(cfg) <<
			*Sampler::ToProperties(cfg);
	
	return props;
}

RenderEngine *LightCPURenderEngine::FromProperties(RenderConfigRef rcfg) {
	return new LightCPURenderEngine(rcfg);
}

PropertiesUPtr LightCPURenderEngine::GetDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
			CPUNoTileRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			PathTracer::GetDefaultProps();

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
