/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/format.hpp>

#include "luxrays/core/oclintersectiondevice.h"

#include "slg/slg.h"
#include "slg/engines/tilepathocl/tilepathocl.h"
#include "slg/engines/tilepathocl/tilepathoclrenderstate.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TilePathOCLRenderEngine
//------------------------------------------------------------------------------

TilePathOCLRenderEngine::TilePathOCLRenderEngine(const RenderConfig *rcfg, Film *flm,
		boost::mutex *flmMutex) : PathOCLStateKernelBaseRenderEngine(rcfg, flm, flmMutex) {
	tileRepository = NULL;
}

TilePathOCLRenderEngine::~TilePathOCLRenderEngine() {
	delete tileRepository;
}

PathOCLBaseRenderThread *TilePathOCLRenderEngine::CreateOCLThread(const u_int index,
	OpenCLIntersectionDevice *device) {
	return new TilePathOCLRenderThread(index, device, this);
}

void TilePathOCLRenderEngine::InitTaskCount() {
	if (GetType() == RTPATHOCL) {
		// Check the maximum number of task to execute. I have to
		// consider preview and normal phase

		const u_int tileWidth = tileRepository->tileWidth;
		const u_int tileHeight = tileRepository->tileHeight;
		const u_int threadFilmPixelCount = tileWidth * tileHeight;

		RTPathOCLRenderEngine *rtengine = (RTPathOCLRenderEngine *)this;
		taskCount = threadFilmPixelCount / Sqr(rtengine->previewResolutionReduction);
		taskCount = Max(taskCount, threadFilmPixelCount / Sqr(rtengine->resolutionReduction));
	} else
		taskCount = tileRepository->tileWidth * tileRepository->tileHeight * aaSamples * aaSamples;

	// I don't know yet the workgroup size of each device so I can not
	// round up task count to be a multiple of workgroups size of all devices
	// used. Rounding to 8192 is a simple trick based on the assumption that
	// workgroup size is a power of 2 and <= 8192.
	taskCount = RoundUp<u_int>(taskCount, 8192);
	//SLG_LOG("[TilePathOCLRenderEngine] OpenCL task count: " << taskCount);
}

void TilePathOCLRenderEngine::InitTileRepository() {
	if (tileRepository)
		delete tileRepository;
	tileRepository = NULL;

	Properties tileProps = renderConfig->cfg.GetAllProperties("tile");
	if (GetType() == RTPATHOCL) {
		tileProps.Delete("tile.size");

		// Check if I'm going to use a single device
		u_int tileWidth, tileHeight;
		if (intersectionDevices.size() == 1) {
			// The best configuration, with a single device, is to use a tile
			// as large as the complete image
			tileWidth = film->GetWidth();
			tileHeight = film->GetHeight();
		} else {
			// One slice for each device
			tileWidth = (film->GetWidth() + 1) / intersectionDevices.size();
			tileHeight = film->GetHeight();
		}

		// Tile width must be a multiple of RESOLUTION_REDUCTION to support RT variable resolution rendering
		RTPathOCLRenderEngine *rtengine = (RTPathOCLRenderEngine *)this;
		u_int rup = Max(rtengine->previewResolutionReduction, rtengine->resolutionReduction);
		tileWidth = RoundUp(tileWidth, rup);

		tileProps <<
				Property("tile.size.x")(tileWidth) <<
				Property("tile.size.y")(tileHeight);
	}

	tileRepository = TileRepository::FromProperties(tileProps);
	if (GetType() == RTPATHOCL)
		tileRepository->enableMultipassRendering = false;
	tileRepository->varianceClamping = VarianceClamping(pathTracer.sqrtVarianceClampMaxValue);
	tileRepository->InitTiles(*film);

	InitTaskCount();
}

RenderState *TilePathOCLRenderEngine::GetRenderState() {
	return new TilePathOCLRenderState(bootStrapSeed, tileRepository);
}

void TilePathOCLRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Check to have the right sampler settings
	//--------------------------------------------------------------------------

	// Sobol is the default sampler (but it can not work with TILEPATH)
	const string samplerType = cfg.Get(Property("sampler.type")(SobolSampler::GetObjectTag())).Get<string>();
	if (samplerType != "TILEPATHSAMPLER")
		throw runtime_error("(RT)TILEPATHOCL render engine can use only TILEPATHSAMPLER");

	//--------------------------------------------------------------------------
	// Initialize the PathTracer class with rendering parameters
	//--------------------------------------------------------------------------

	const Properties &defaultProps = (GetType() == TILEPATHOCL) ?
		TilePathOCLRenderEngine::GetDefaultProps() :
		RTPathOCLRenderEngine::GetDefaultProps();

	// TilePath specific settings
	aaSamples = (GetType() == TILEPATHOCL) ?
		Max(1, cfg.Get(defaultProps.Get("tilepath.sampling.aa.size")).Get<int>()) :
		1;
	usePixelAtomics = true;
	maxTilePerDevice = cfg.Get(Property("tilepathocl.devices.maxtiles")(16)).Get<u_int>();

	pathTracer.ParseOptions(cfg, defaultProps);

	//--------------------------------------------------------------------------
	// Restore render state if there is one
	//--------------------------------------------------------------------------

	if (startRenderState) {
		// Check if the render state is of the right type
		startRenderState->CheckEngineTag(GetObjectTag());

		TilePathOCLRenderState *rs = (TilePathOCLRenderState *)startRenderState;

		// Use a new seed to continue the rendering
		const u_int newSeed = rs->bootStrapSeed + 1;
		SLG_LOG("Continuing the rendering with new TILEPATHCPU seed: " + ToString(newSeed));
		SetSeed(newSeed);

		tileRepository = rs->tileRepository;
		
		delete startRenderState;
		startRenderState = NULL;
		
		InitTaskCount();
	} else {
		film->Reset();

		InitTileRepository();
	}

	//--------------------------------------------------------------------------

	PathOCLStateKernelBaseRenderEngine::StartLockLess();
}

void TilePathOCLRenderEngine::StopLockLess() {
	PathOCLBaseRenderEngine::StopLockLess();

	delete tileRepository;
	tileRepository = NULL;
}

void TilePathOCLRenderEngine::EndSceneEditLockLess(const EditActionList &editActions) {
	if (GetType() != RTPATHOCL) {
		// RTPATHOCL will InitTiles() on next frame
		tileRepository->Clear();
		tileRepository->InitTiles(*film);
	}

	PathOCLBaseRenderEngine::EndSceneEditLockLess(editActions);
}

void TilePathOCLRenderEngine::UpdateCounters() {
	// Update the sample count statistic
	samplesCount = film->GetTotalSampleCount();

	// Update the ray count statistic
	double totalCount = 0.0;
	for (size_t i = 0; i < intersectionDevices.size(); ++i)
		totalCount += intersectionDevices[i]->GetTotalRaysCount();
	raysCount = totalCount;

	if (!tileRepository->done) {
		// Update the time only while rendering is not finished
		elapsedTime = WallClockTime() - startTime;
	} else
		convergence = 1.f;
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties TilePathOCLRenderEngine::ToProperties(const Properties &cfg) {
	return OCLRenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("tilepath.sampling.aa.size")) <<
			cfg.Get(GetDefaultProps().Get("tilepathocl.devices.maxtiles")) <<
			PathTracer::ToProperties(cfg) <<
			TileRepository::ToProperties(cfg);
}

RenderEngine *TilePathOCLRenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new TilePathOCLRenderEngine(rcfg, flm, flmMutex);
}

const Properties &TilePathOCLRenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			OCLRenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("tilepath.sampling.aa.size")(3) <<
			Property("tilepathocl.devices.maxtiles")(16) <<
			PathTracer::GetDefaultProps() <<
			TileRepository::GetDefaultProps();

	return props;
}

#endif
