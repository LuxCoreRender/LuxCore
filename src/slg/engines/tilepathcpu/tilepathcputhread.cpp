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

#include "slg/samplers/tilepathsampler.h"
#include "slg/engines/tilepathcpu/tilepathcpu.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TilePathCPU RenderThread
//------------------------------------------------------------------------------

TilePathCPURenderThread::TilePathCPURenderThread(TilePathCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPUTileRenderThread(engine, index, device) {
}

void TilePathCPURenderThread::SampleGrid(RandomGenerator *rndGen, const u_int size,
		const u_int ix, const u_int iy, float *u0, float *u1) const {
	*u0 = rndGen->floatValue();
	*u1 = rndGen->floatValue();

	if (size > 1) {
		const float idim = 1.f / size;
		*u0 = (ix + *u0) * idim;
		*u1 = (iy + *u1) * idim;
	}
}

void TilePathCPURenderThread::RenderFunc() {
	//SLG_LOG("[TilePathCPURenderEngine::" << threadIndex << "] Rendering thread started");

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	TilePathCPURenderEngine *engine = (TilePathCPURenderEngine *)renderEngine;
	const PathTracer &pathTracer = engine->pathTracer;
	RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + threadIndex);

	// Setup the sampler
	Sampler *genericSampler = engine->renderConfig->AllocSampler(rndGen,
			engine->film, NULL, NULL);
	genericSampler->RequestSamples(pathTracer.sampleSize);

	TilePathSampler *sampler = dynamic_cast<TilePathSampler *>(genericSampler);
	sampler->SetAASamples(engine->aaSamples);

	// Initialize SampleResult
	vector<SampleResult> sampleResults(1);
	pathTracer.InitSampleResults(engine->film, sampleResults);

	//--------------------------------------------------------------------------
	// Extract the tile to render
	//--------------------------------------------------------------------------

	TileRepository::Tile *tile = NULL;
	bool interruptionRequested = boost::this_thread::interruption_requested();
	while (engine->tileRepository->NextTile(engine->film, engine->filmMutex, &tile, tileFilm) && !interruptionRequested) {
		// Check if we are in pause mode
		if (engine->pauseMode) {
			// Check every 100ms if I have to continue the rendering
			while (!boost::this_thread::interruption_requested() && engine->pauseMode)
				boost::this_thread::sleep(boost::posix_time::millisec(100));

			if (boost::this_thread::interruption_requested())
				break;
		}

		// Render the tile
		tileFilm->Reset();
		//SLG_LOG("[TilePathCPURenderEngine::" << threadIndex << "] Tile: "
		//		"(" << tile->xStart << ", " << tile->yStart << ") => " <<
		//		"(" << tile->tileWidth << ", " << tile->tileHeight << ")");

		//----------------------------------------------------------------------
		// Render the tile
		//----------------------------------------------------------------------

		sampler->Init(tile, tileFilm);

		for (u_int y = 0; y < tile->tileHeight && !interruptionRequested; ++y) {
			for (u_int x = 0; x < tile->tileWidth && !interruptionRequested; ++x) {
				for (u_int sampleY = 0; sampleY < engine->aaSamples; ++sampleY) {
					for (u_int sampleX = 0; sampleX < engine->aaSamples; ++sampleX) {
						pathTracer.RenderSample(device, engine->renderConfig->scene, engine->film, sampler, sampleResults);

						sampler->NextSample(sampleResults);
					}
				}

				interruptionRequested = boost::this_thread::interruption_requested();
#ifdef WIN32
				// Work around Windows bad scheduling
				renderThread->yield();
#endif
			}
		}
	}

	delete rndGen;

	//SLG_LOG("[TilePathCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
