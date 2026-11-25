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

#include "slg/engines/bidircpu/bidircpu.h"
#include "slg/engines/bidircpu/bidircpurenderstate.h"
#include "slg/samplers/sobol.h"
#include "slg/cameras/camera.h"

using namespace luxrays;
using namespace slg;
using namespace std;

//------------------------------------------------------------------------------
// BiDirCPURenderEngine
//------------------------------------------------------------------------------

BiDirCPURenderEngine::BiDirCPURenderEngine(RenderConfigRef rcfg) :
		CPUNoTileRenderEngine(rcfg),
		photonGICache(nullptr) {
	if (rcfg.GetScene().GetCamera().GetType() == Camera::STEREO)
		throw std::runtime_error("BIDIRCPU render engine doesn't support stereo camera");

	lightPathsCount = 1;
	baseRadius = 0.f;
	radiusAlpha = 0.f;

	aovWarmupSamplerSharedData = nullptr;
}

BiDirCPURenderEngine::~BiDirCPURenderEngine() {
	delete photonGICache;
}

RenderStateSPtr BiDirCPURenderEngine::GetRenderState() {
	return std::make_shared<BiDirCPURenderState>(bootStrapSeed, photonGICache);
}

void BiDirCPURenderEngine::StartLockLess() {
	auto& cfg = renderConfig.GetConfig();

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	CheckSamplersForNoTile(RenderEngineType2String(GetType()), cfg);

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	maxEyePathDepth = (u_int)Max(1, cfg.Get(GetDefaultProps()->Get("path.maxdepth")).Get<int>());
	maxLightPathDepth = (u_int)Max(1, cfg.Get(GetDefaultProps()->Get("light.maxdepth")).Get<int>());
	
	rrDepth = (u_int)Max(1, cfg.Get(GetDefaultProps()->Get("path.russianroulette.depth")).Get<int>());
	rrImportanceCap = Clamp(cfg.Get(GetDefaultProps()->Get("path.russianroulette.cap")).Get<double>(), 0.0, 1.0);

	// Clamping settings
	// clamping.radiance.maxvalue is the old radiance clamping, now converted in variance clamping
	sqrtVarianceClampMaxValue = cfg.Get(Property("path.clamping.radiance.maxvalue")(0.0)).Get<double>();
	if (cfg.IsDefined("path.clamping.variance.maxvalue"))
		sqrtVarianceClampMaxValue = cfg.Get(GetDefaultProps()->Get("path.clamping.variance.maxvalue")).Get<double>();
	sqrtVarianceClampMaxValue = Max(0.f, sqrtVarianceClampMaxValue);

	// Albedo AOV settings
	albedoSpecularSetting = String2AlbedoSpecularSetting(cfg.Get(GetDefaultProps()->Get("path.albedospecular.type")).Get<string>());
	albedoSpecularGlossinessThreshold = Max(cfg.Get(GetDefaultProps()->Get("path.albedospecular.glossinessthreshold")).Get<double>(), 0.0);

	//--------------------------------------------------------------------------
	// Restore render state if there is one
	//--------------------------------------------------------------------------

	if (startRenderState) {
		// Check if the render state is of the right type
		startRenderState->CheckEngineTag(GetObjectTag());

		auto rs = static_pointer_cast<BiDirCPURenderState>(startRenderState);

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new BIDIRCPU seed: " + ToString(newSeed));
		SetSeed(newSeed);
		
		// Transfer the ownership of PhotonGI cache pointer
		photonGICache = rs->photonGICache;
		rs->photonGICache = nullptr;

		// I have to set the scene pointer in photonGICache because it is not
		// saved by serialization
		if (photonGICache)
			photonGICache->SetScene(renderConfig.GetScene());

		startRenderState = nullptr;
	}

	//--------------------------------------------------------------------------
	// Allocate PhotonGICache if enabled
	//--------------------------------------------------------------------------

	// note: photonGICache could have been restored from the render state
	if (!photonGICache) {
		photonGICache = PhotonGICache::FromProperties(renderConfig.GetScene(), cfg);

		// photonGICache will be nullptr if the cache is disabled
		if (photonGICache)
			photonGICache->Preprocess(renderThreads.size());
	}
	
	//--------------------------------------------------------------------------
	// Albedo and Normal AOV warm up settings
	//--------------------------------------------------------------------------

	aovWarmupSPP = Max(0u, cfg.Get(GetDefaultProps()->Get("path.aovs.warmup.spp")).Get<u_int>());
	if (!GetFilm().HasChannel(Film::ALBEDO) && !GetFilm().HasChannel(Film::AVG_SHADING_NORMAL))
		aovWarmupSPP = 0;
	if (aovWarmupSPP > 0)
		aovWarmupSamplerSharedData = std::make_shared<SobolSamplerSharedData>(
			seedBaseGenerator->uintValue(),
			GetFilmPtr()
		);

	//--------------------------------------------------------------------------

	SetSampleSplatter(std::make_unique<FilmSampleSplatter>(GetPixelFilter()));

	CPUNoTileRenderEngine::StartLockLess();
}

void BiDirCPURenderEngine::InitFilm() {
	GetFilm().AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	GetFilm().AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	GetFilm().SetRadianceGroupCount(renderConfig.GetScene().GetLightSources().GetLightGroupCount());
	GetFilm().SetThreadCount(renderThreads.size());
	GetFilm().Init();
}

void BiDirCPURenderEngine::StopLockLess() {
	CPUNoTileRenderEngine::StopLockLess();

	ResetSampleSplatter();

	delete photonGICache;
	photonGICache = nullptr;
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

PropertiesUPtr BiDirCPURenderEngine::ToProperties(const Properties &cfg) {
	PropertiesUPtr props = CPUNoTileRenderEngine::ToProperties(cfg);
	
	*props <<
				cfg.Get(GetDefaultProps()->Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps()->Get("path.maxdepth")) <<
			cfg.Get(GetDefaultProps()->Get("light.maxdepth")) <<
			cfg.Get(GetDefaultProps()->Get("path.aovs.warmup.spp")) <<
			cfg.Get(GetDefaultProps()->Get("path.russianroulette.depth")) <<
			cfg.Get(GetDefaultProps()->Get("path.russianroulette.cap")) <<
			cfg.Get(GetDefaultProps()->Get("path.clamping.variance.maxvalue")) <<
			cfg.Get(GetDefaultProps()->Get("path.albedospecular.type")) <<
			cfg.Get(GetDefaultProps()->Get("path.albedospecular.glossinessthreshold")) <<
			*Sampler::ToProperties(cfg) <<
			*PhotonGICache::ToProperties(cfg);
	
	return props;
}

RenderEngine *BiDirCPURenderEngine::FromProperties(RenderConfigRef rcfg) {
	return new BiDirCPURenderEngine(rcfg);
}

PropertiesUPtr BiDirCPURenderEngine::GetDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
		CPUNoTileRenderEngine::GetDefaultProps() <<
		Property("renderengine.type")(GetObjectTag()) <<
		Property("path.maxdepth")(5) <<
		Property("light.maxdepth")(5) <<
		Property("path.aovs.warmup.spp")(0) <<
		Property("path.russianroulette.depth")(3) <<
		Property("path.russianroulette.cap")(.5f) <<
		Property("path.clamping.variance.maxvalue")(0.f) <<
		Property("path.albedospecular.type")("REFLECT_TRANSMIT") <<
		Property("path.albedospecular.glossinessthreshold")(.05f) <<
		PhotonGICache::GetDefaultProps();

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
