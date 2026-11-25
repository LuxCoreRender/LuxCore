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

#include <limits>

#include "slg/samplers/tilepathsampler.h"
#include "slg/engines/tilepathcpu/tilepathcpu.h"
#include "slg/engines/tilepathcpu/tilepathcpurenderstate.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TilePathCPURenderEngine
//------------------------------------------------------------------------------

TilePathCPURenderEngine::TilePathCPURenderEngine(RenderConfigRef rcfg) :
		CPUTileRenderEngine(rcfg), photonGICache(nullptr) {
}

TilePathCPURenderEngine::~TilePathCPURenderEngine() {
	delete photonGICache;
}

void TilePathCPURenderEngine::InitFilm() {
	GetFilm().AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	GetFilm().SetRadianceGroupCount(renderConfig.GetScene().GetLightSources().GetLightGroupCount());
	GetFilm().Init();
}

RenderStateSPtr TilePathCPURenderEngine::GetRenderState() {
	return std::make_shared<TilePathCPURenderState>(bootStrapSeed, tileRepository, photonGICache);
}

void TilePathCPURenderEngine::StartLockLess() {
	const auto& cfg = renderConfig.GetConfig();

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	// Sobol is the default sampler (but it can not work with TILEPATH)
	CheckSamplersForTile(RenderEngineType2String(GetType()), cfg);

	//--------------------------------------------------------------------------
	// Initialize rendering parameters
	//--------------------------------------------------------------------------

	aaSamples = Max(1, cfg.Get(GetDefaultProps()->Get("tilepath.sampling.aa.size")).Get<int>());

	// pathTracer must be configured here because it is then used
	// to set tileRepository->varianceClamping, etc.
	pathTracer.ParseOptions(cfg, *GetDefaultProps());

	//--------------------------------------------------------------------------
	// Restore render state if there is one
	//--------------------------------------------------------------------------

	if (startRenderState) {
		// Check if the render state is of the right type
		startRenderState->CheckEngineTag(GetObjectTag());

		auto rs = static_pointer_cast<TilePathCPURenderState>(startRenderState);

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new TILEPATHCPU seed: " + ToString(newSeed));
		SetSeed(newSeed);

		// Transfer the ownership of TileRepository pointer
		tileRepository = rs->tileRepository;
		rs->tileRepository = nullptr;

		// Transfer the ownership of PhotonGI cache pointer
		photonGICache = rs->photonGICache;
		rs->photonGICache = nullptr;

		startRenderState = nullptr;
	} else {
		GetFilm().Reset();

		tileRepository = TileRepository::FromProperties(renderConfig.GetConfig());
		tileRepository->varianceClamping = VarianceClamping(pathTracer.sqrtVarianceClampMaxValue);
		tileRepository->InitTiles(GetFilm());
	}

	//--------------------------------------------------------------------------
	// Allocate PhotonGICache if enabled
	//--------------------------------------------------------------------------

	// note: photonGICache could have been restored from the render state
	if ((GetType() != RTPATHCPU) && !photonGICache) {
		photonGICache = PhotonGICache::FromProperties(renderConfig.GetScene(), cfg);

		// photonGICache will be nullptr if the cache is disabled
		if (photonGICache)
			photonGICache->Preprocess(renderThreads.size());
	}

	//--------------------------------------------------------------------------
	// Initialize the PathTracer class with rendering parameters
	//--------------------------------------------------------------------------

	pathTracer.InitPixelFilterDistribution(GetPixelFilter());
	pathTracer.SetPhotonGICache(photonGICache);

	//--------------------------------------------------------------------------

	CPURenderEngine::StartLockLess();
}

void TilePathCPURenderEngine::StopLockLess() {
	CPUTileRenderEngine::StopLockLess();

	pathTracer.DeletePixelFilterDistribution();
	
	delete photonGICache;
	photonGICache = nullptr;
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

PropertiesUPtr TilePathCPURenderEngine::ToProperties(const Properties &cfg) {
	PropertiesUPtr props = std::make_unique<Properties>();
	
	*props <<
				CPUTileRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps()->Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps()->Get("tilepath.sampling.aa.size")) <<
			PathTracer::ToProperties(cfg) <<
			PhotonGICache::ToProperties(cfg);

	return props;
}

RenderEngine *TilePathCPURenderEngine::FromProperties(RenderConfigRef rcfg) {
	return new TilePathCPURenderEngine(rcfg);
}

PropertiesUPtr TilePathCPURenderEngine::GetDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
			CPUTileRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("tilepath.sampling.aa.size")(3) <<
			PathTracer::GetDefaultProps() <<
			PhotonGICache::GetDefaultProps();

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
