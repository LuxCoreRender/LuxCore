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

#include "slg/film/film.h"
#include "slg/usings.h"
#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "luxrays/utils/thread.h"
#include "luxrays/core/intersectiondevice.h"

#include "slg/slg.h"
#include "slg/engines/tilepathocl/tilepathocl.h"
#include "slg/samplers/tilepathsampler.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace std::literals::chrono_literals;

//------------------------------------------------------------------------------
// TilePathNativeRenderThread
//------------------------------------------------------------------------------

TilePathNativeRenderThread::TilePathNativeRenderThread(const u_int index,
	NativeIntersectionDeviceRef device, TilePathOCLRenderEngine *re) : 
	PathOCLBaseNativeRenderThread(index, device, re), tileFilm(nullptr)
{}

TilePathNativeRenderThread::~TilePathNativeRenderThread() {
}

void TilePathNativeRenderThread::StartRenderThread() {

	TilePathOCLRenderEngine *engine = (TilePathOCLRenderEngine *)renderEngine;
	tileFilm = Film::Create(
			engine->tileRepository->tileWidth,
			engine->tileRepository->tileHeight,
			nullptr
	);
	tileFilm->CopyDynamicSettings(engine->GetFilm());
	tileFilm->Init();

	PathOCLBaseNativeRenderThread::StartRenderThread();
}

void TilePathNativeRenderThread::SampleGrid(RandomGenerator *rndGen, const u_int size,
		const u_int ix, const u_int iy, float *u0, float *u1) const {
	*u0 = rndGen->floatValue();
	*u1 = rndGen->floatValue();

	if (size > 1) {
		const float idim = 1.f / size;
		*u0 = (ix + *u0) * idim;
		*u1 = (iy + *u1) * idim;
	}
}

void TilePathNativeRenderThread::RenderThreadImpl(std::stop_token stop_token) {
	//SLG_LOG("[TilePathNativeRenderThread::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);

	TilePathOCLRenderEngine *engine = (TilePathOCLRenderEngine *)renderEngine;
	const PathTracer &pathTracer = engine->pathTracer;
	auto rndGen = std::make_unique<RandomGenerator>(engine->seedBase + threadIndex);

	// Setup the sampler
	auto genericSampler = engine->renderConfig.AllocSampler(
		rndGen, engine->GetFilm(), engine->GetSampleSplatter(), nullptr, Properties()
	);
	genericSampler->RequestSamples(PIXEL_NORMALIZED_ONLY, pathTracer.eyeSampleSize);

	auto& sampler = dynamic_cast<TilePathSampler&>(*genericSampler);
	sampler.SetAASamples(engine->aaSamples);

	// Initialize SampleResult
	std::vector<SampleResult> sampleResults(1);
	PathTracer::InitEyeSampleResults(engine->GetFilm(), sampleResults);

	//--------------------------------------------------------------------------
	// Extract the tile to render
	//--------------------------------------------------------------------------

	TileWork tileWork;
	while (engine->tileRepository->NextTile(engine->GetFilm(), engine->filmMutex, tileWork, GetTileFilm()) && !stop_token.stop_requested()) {
		// Check if we are in pause mode
		if (engine->pauseMode) {
			// Check every 100ms if I have to continue the rendering
			while (!stop_token.stop_requested() && engine->pauseMode)
				std::this_thread::sleep_for(100ms);

			if (stop_token.stop_requested())
				break;
		}

		// Render the tile
		tileFilm->Reset();
		if (tileFilm->GetDenoiser().IsEnabled())
			tileFilm->GetDenoiser().SetReferenceFilm(
				FilmPtr(&engine->GetFilm()),
				tileWork.GetCoord().x,
				tileWork.GetCoord().y
			);
		//SLG_LOG("[TilePathNativeRenderThread::" << threadIndex << "] TileWork: " << tileWork);

		//----------------------------------------------------------------------
		// Render the tile
		//----------------------------------------------------------------------

		sampler.Init(&tileWork, GetTileFilmPtr());

		for (u_int y = 0; y < tileWork.GetCoord().height && !stop_token.stop_requested(); ++y) {
			for (u_int x = 0; x < tileWork.GetCoord().width && !stop_token.stop_requested(); ++x) {
				for (u_int sampleY = 0; sampleY < engine->aaSamples; ++sampleY) {
					for (u_int sampleX = 0; sampleX < engine->aaSamples; ++sampleX) {
						pathTracer.RenderEyeSample(intersectionDevice, engine->renderConfig.GetScene(),
								engine->GetFilm(), sampler, sampleResults);

						sampler.NextSample(sampleResults);
					}
				}

#ifdef WIN32
				// Work around Windows bad scheduling
                std::this_thread::yield();
#endif
			}
		}

                if (stop_token.stop_requested())
                        break;

		if (engine->photonGICache) {
			const u_int spp = engine->GetFilm().GetTotalEyeSampleCount() / engine->GetFilm().GetPixelCount();
			engine->photonGICache->Update(engine->renderOCLThreads.size() + threadIndex, spp);
		}
	}

	threadDone = true;

	// This is done to stop threads pending on barrier wait
	// inside engine->photonGICache->Update(). This can happen when an
	// halt condition is satisfied.
	if (engine->photonGICache)
		engine->photonGICache->FinishUpdate(threadIndex);

	//SLG_LOG("[TilePathNativeRenderThread::" << threadIndex << "] Rendering thread halted");
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
