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

#include "slg/engines/pathcpu/pathcpu.h"
#include "slg/engines/pathcpu/pathcpurenderstate.h"
#include "slg/film/filters/filter.h"
#include "slg/samplers/sobol.h"
#include "slg/samplers/metropolis.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathCPURenderEngine
//------------------------------------------------------------------------------

PathCPURenderEngine::PathCPURenderEngine(const RenderConfig *rcfg) :
		CPUNoTileRenderEngine(rcfg), photonGICache(nullptr),
		lightSampleSplatter(nullptr), lightSamplerSharedData(nullptr) {
}

PathCPURenderEngine::~PathCPURenderEngine() {
	delete photonGICache;
	delete lightSampleSplatter;
	delete lightSamplerSharedData;
}

void PathCPURenderEngine::InitFilm() {
	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);

	// pathTracer has not yet been initialized
	const bool hybridBackForwardEnable = renderConfig->cfg.Get(PathTracer::GetDefaultProps().
			Get("path.hybridbackforward.enable")).Get<bool>();
	if (hybridBackForwardEnable)
		film->AddChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	film->SetRadianceGroupCount(renderConfig->scene->lightDefs.GetLightGroupCount());
	film->SetThreadCount(renderThreads.size());
	film->Init();
}

RenderState *PathCPURenderEngine::GetRenderState() {
	return new PathCPURenderState(bootStrapSeed, photonGICache);
}

void PathCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	if (GetType() == RTPATHCPU) {
		const string samplerType = cfg.Get(Property("sampler.type")(SobolSampler::GetObjectTag())).Get<string>();
		if (samplerType != "RTPATHCPUSAMPLER")
			throw runtime_error("RTPATHCPU render engine can use only RTPATHCPUSAMPLER");
	} else
		CheckSamplersForNoTile(RenderEngineType2String(GetType()), cfg);

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	const string samplerType = cfg.Get(Property("sampler.type")(SobolSampler::GetObjectTag())).Get<string>();
	if (GetType() == RTPATHCPU) {
		if (samplerType != "RTPATHCPUSAMPLER")
			throw runtime_error("RTPATHCPU render engine can use only RTPATHCPUSAMPLER");
	} else {
		if (samplerType == "RTPATHCPUSAMPLER")
			throw runtime_error("PATHCPU render engine can not use RTPATHCPUSAMPLER");		
	}

	//--------------------------------------------------------------------------
	// Restore render state if there is one
	//--------------------------------------------------------------------------

	if (startRenderState) {
		// Check if the render state is of the right type
		startRenderState->CheckEngineTag(GetObjectTag());

		PathCPURenderState *rs = (PathCPURenderState *)startRenderState;

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new PATHCPU seed: " + ToString(newSeed));
		SetSeed(newSeed);
		
		// Transfer the ownership of PhotonGI cache pointer
		photonGICache = rs->photonGICache;
		rs->photonGICache = nullptr;
		
		// I have to set the scene pointer in photonGICache because it is not
		// saved by serialization
		if (photonGICache)
			photonGICache->SetScene(renderConfig->scene);

		delete startRenderState;
		startRenderState = NULL;
	}

	//--------------------------------------------------------------------------
	// Allocate PhotonGICache if enabled
	//--------------------------------------------------------------------------

	// note: photonGICache could have been restored from the render state
	if ((GetType() != RTPATHCPU) && !photonGICache) {
		photonGICache = PhotonGICache::FromProperties(renderConfig->scene, cfg);

		// photonGICache will be nullptr if the cache is disabled
		if (photonGICache)
			photonGICache->Preprocess(renderThreads.size());
	}

	//--------------------------------------------------------------------------
	// Initialize the PathTracer class with rendering parameters
	//--------------------------------------------------------------------------

	pathTracer.ParseOptions(cfg, GetDefaultProps());

	if (pathTracer.hybridBackForwardEnable)
		lightSamplerSharedData = MetropolisSamplerSharedData::FromProperties(Properties(), &seedBaseGenerator, film);

	pathTracer.InitPixelFilterDistribution(pixelFilter);

	delete lightSampleSplatter;
	if (pathTracer.hybridBackForwardEnable)
		lightSampleSplatter = new FilmSampleSplatter(pixelFilter);

	pathTracer.SetPhotonGICache(photonGICache);
	
	//--------------------------------------------------------------------------

	CPUNoTileRenderEngine::StartLockLess();
}

void PathCPURenderEngine::StopLockLess() {
	CPUNoTileRenderEngine::StopLockLess();

	pathTracer.DeletePixelFilterDistribution();
	delete lightSampleSplatter;
	lightSampleSplatter = NULL;

	delete lightSamplerSharedData;
	lightSamplerSharedData = nullptr;

	delete photonGICache;
	photonGICache = nullptr;
}

void PathCPURenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	if (lightSamplerSharedData)
		lightSamplerSharedData->Reset();

	CPURenderEngine::EndSceneEditLockLess(editActions);
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties PathCPURenderEngine::ToProperties(const Properties &cfg) {
	Properties props;
	
	props << CPUNoTileRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			PathTracer::ToProperties(cfg) <<
			PhotonGICache::ToProperties(cfg);

	return props;
}

RenderEngine *PathCPURenderEngine::FromProperties(const RenderConfig *rcfg) {
	return new PathCPURenderEngine(rcfg);
}

const Properties &PathCPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			CPUNoTileRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			PathTracer::GetDefaultProps() <<
			PhotonGICache::GetDefaultProps();

	return props;
}
