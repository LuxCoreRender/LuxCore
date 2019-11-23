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

#include <boost/thread/barrier.hpp>

#include "slg/engines/bakecpu/bakecpu.h"
#include "slg/engines/bakecpu/bakecpurenderstate.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/film/filters/filter.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BakeCPURenderEngine
//------------------------------------------------------------------------------

BakeCPURenderEngine::BakeCPURenderEngine(const RenderConfig *rcfg) :
		CPUNoTileRenderEngine(rcfg), photonGICache(nullptr), mapFilm(nullptr),
		currentSceneObjsDist(nullptr), threadsSyncBarrier(nullptr) {
	// Read the list of bake maps to render

	const Properties &cfg = rcfg->cfg;
	vector<string> mapKeys = cfg.GetAllUniqueSubNames("bake.maps");
	for (auto const &mapKey : mapKeys) {
		// Extract the image pipeline priority name
		const string mapTagStr = Property::ExtractField(mapKey, 2);
		if (mapTagStr == "")
			throw runtime_error("Syntax error in bake map definition: " + mapKey);

		const string prefix = "bake.maps." + mapTagStr;

		BakeMapInfo mapInfo;

		// Read the map type
		const string mapType = cfg.Get(Property(prefix + ".type")("COMBINED")).Get<string>();
		if (mapType == "COMBINED")
			mapInfo.type = COMBINED;
		else if (mapType == "LIGHTMAP")
			mapInfo.type = LIGHTMAP;
		else
			throw runtime_error("Unknown bake map type: " + mapType);

		// Read the map file name and size
		mapInfo.fileName = cfg.Get(Property(prefix + ".filename")("image.png")).Get<string>();
		mapInfo.imagePipelineIndex = cfg.Get(Property(prefix + ".imagepipelineindex")(0)).Get<u_int>();
		mapInfo.width = cfg.Get(Property(prefix + ".width")(512u)).Get<u_int>();
		mapInfo.height = cfg.Get(Property(prefix + ".height")(512u)).Get<u_int>();
		mapInfo.uvindex = Clamp(cfg.Get(Property(prefix + ".uvindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		// Read the list of objects to bake
		const Property &objNamesProp = cfg.Get(Property(prefix + ".objectnames")("objectNameToBake"));
		for (u_int i = 0; i < objNamesProp.GetSize(); ++i)
			mapInfo.objectNames.push_back(objNamesProp.Get<string>(i));

		mapInfos.push_back(mapInfo);
	}

	SLG_LOG("Number of maps to bake: " + ToString(mapInfos.size()));
}

BakeCPURenderEngine::~BakeCPURenderEngine() {
	for (auto dist : currentSceneObjDist)
		delete dist;
	currentSceneObjDist.clear();
	delete currentSceneObjsDist;
	delete threadsSyncBarrier;
	delete mapFilm;
	delete photonGICache;
}

void BakeCPURenderEngine::InitFilm() {
	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);

	film->Init();
}

RenderState *BakeCPURenderEngine::GetRenderState() {
	return new BakeCPURenderState(bootStrapSeed, photonGICache);
}

void BakeCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	CheckSamplersForNoTile(RenderEngineType2String(GetType()), cfg);

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	const string samplerType = cfg.Get(Property("sampler.type")(SobolSampler::GetObjectTag())).Get<string>();
	if (samplerType == "RTPATHCPUSAMPLER")
		throw runtime_error("BAKECPU render engine can not use RTPATHCPUSAMPLER");		

	//--------------------------------------------------------------------------
	// Restore render state if there is one
	//--------------------------------------------------------------------------

	if (startRenderState) {
		// Check if the render state is of the right type
		startRenderState->CheckEngineTag(GetObjectTag());

		BakeCPURenderState *rs = (BakeCPURenderState *)startRenderState;

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new BAKECPU seed: " + ToString(newSeed));
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
	if (!photonGICache) {
		photonGICache = PhotonGICache::FromProperties(renderConfig->scene, cfg);

		// photonGICache will be nullptr if the cache is disabled
		if (photonGICache)
			photonGICache->Preprocess(renderThreads.size());
	}

	//--------------------------------------------------------------------------
	// Initialize the PathTracer class with rendering parameters
	//--------------------------------------------------------------------------

	pathTracer.ParseOptions(cfg, GetDefaultProps());

	pathTracer.InitPixelFilterDistribution(pixelFilter);
	pathTracer.SetPhotonGICache(photonGICache);
	
	//--------------------------------------------------------------------------
	
	threadsSyncBarrier = new boost::barrier(renderThreads.size());

	//--------------------------------------------------------------------------

	CPUNoTileRenderEngine::StartLockLess();
}

void BakeCPURenderEngine::StopLockLess() {
	CPUNoTileRenderEngine::StopLockLess();

	for (auto dist : currentSceneObjDist)
		delete dist;
	currentSceneObjDist.clear();

	delete currentSceneObjsDist;
	currentSceneObjsDist = nullptr;
	
	delete threadsSyncBarrier;
	threadsSyncBarrier = nullptr;

	pathTracer.DeletePixelFilterDistribution();

	delete photonGICache;
	photonGICache = nullptr;
}

void BakeCPURenderEngine::UpdateFilmLockLess() {
	boost::unique_lock<boost::mutex> lock(*filmMutex);

	// Film may have been not initialized because of an error during Start()
	if (film->IsInitiliazed()) {
		film->Clear();
		film->GetDenoiser().Clear();

		if (mapFilm) {
			film->AddFilm(*mapFilm,
					0, 0,
					Min(mapFilm->GetWidth(), film->GetWidth()),
					Min(mapFilm->GetHeight(), film->GetHeight()),
					0, 0);
		}
	}
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties BakeCPURenderEngine::ToProperties(const Properties &cfg) {
	Properties props;
	
	props << CPUNoTileRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			PathTracer::ToProperties(cfg) <<
			PhotonGICache::ToProperties(cfg);

	props << cfg.GetAllProperties("bake.maps.");

	return props;
}

RenderEngine *BakeCPURenderEngine::FromProperties(const RenderConfig *rcfg) {
	return new BakeCPURenderEngine(rcfg);
}

const Properties &BakeCPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			CPUNoTileRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			PathTracer::GetDefaultProps() <<
			PhotonGICache::GetDefaultProps();

	return props;
}
