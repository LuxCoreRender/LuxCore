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

#include "slg/engines/bakecpu/bakecpu.h"
#include "slg/volumes/volume.h"
#include "slg/utils/varianceclamping.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BakeCPU RenderThread
//------------------------------------------------------------------------------

BakeCPURenderThread::BakeCPURenderThread(BakeCPURenderEngine *engine,
		const u_int index, IntersectionDevice *device) :
		CPUNoTileRenderThread(engine, index, device) {
}

void BakeCPURenderThread::InitBakeWork(const BakeMapInfo &mapInfo) {
	BakeCPURenderEngine *engine = (BakeCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;

	// Lock the main film
	boost::unique_lock<boost::mutex> lock(*engine->filmMutex);

	// Print some information
	SLG_LOG("Baking map: " << mapInfo.fileName);
	SLG_LOG("Resolution: " << mapInfo.width << "x" << mapInfo.height);

	engine->currentSceneObjsToBake.clear();
	
	if (engine->skipExistingMapFiles) {
		// Check if the file exist
		if (boost::filesystem::exists(mapInfo.fileName)) {
			SLG_LOG("Bake map file already exists: " << mapInfo.fileName);
			// Skip this map
			return;
		}
	}

	// Initialize the map Film
	delete engine->mapFilm;
	engine->mapFilm = nullptr;

	engine->mapFilm = new Film(mapInfo.width, mapInfo.height, nullptr);
	engine->mapFilm->CopyDynamicSettings(*engine->film);
	// Copy the halt conditions too
	engine->mapFilm->CopyHaltSettings(*engine->film);
	engine->mapFilm->Init();

	// Build the list of object to bake and each mesh area
	for (auto const &objName : mapInfo.objectNames) {
		const SceneObject *sceneObj = scene->objDefs.GetSceneObject(objName);
		if (sceneObj)
			engine->currentSceneObjsToBake.push_back(sceneObj);
		else
			SLG_LOG("WARNING: Unknown object to bake ignored (" << objName << ")");
	}

	if (engine->currentSceneObjsToBake.size() == 0)
		return;

	// To sample the each mesh triangle according its area
	engine->currentSceneObjDist.resize(engine->currentSceneObjsToBake.size(), nullptr);
	vector<float> meshesArea(engine->currentSceneObjsToBake.size());

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int sceneObjIndex = 0; sceneObjIndex < engine->currentSceneObjDist.size(); ++sceneObjIndex) {
		const SceneObject *sceneObj = engine->currentSceneObjsToBake[sceneObjIndex];
		const ExtMesh *mesh = sceneObj->GetExtMesh();
		
		Transform localToWorld;
		sceneObj->GetExtMesh()->GetLocal2World(0.f, localToWorld);

		vector<float> trisArea(mesh->GetTotalTriangleCount());
		meshesArea[sceneObjIndex] = 0.f;
		for (u_int triIndex = 0; triIndex < mesh->GetTotalTriangleCount(); ++triIndex) {
			trisArea[triIndex] = mesh->GetTriangleArea(localToWorld, triIndex);
			meshesArea[sceneObjIndex] += trisArea[triIndex];
		}
		
		engine->currentSceneObjDist[sceneObjIndex] = new Distribution1D(&trisArea[0], trisArea.size());
	}
	
	// To sample the meshes according their area
	delete engine->currentSceneObjsDist;
	engine->currentSceneObjsDist = new Distribution1D(&meshesArea[0], meshesArea.size());

	// Reset the main film
	engine->film->Reset();
}

void BakeCPURenderThread::RenderFunc() {
	//SLG_LOG("[BakeCPURenderEngine::" << threadIndex << "] Rendering thread started");

	// Boost barriers (used in PhotonGICache::Update()) are supposed to be not
	// interruptible but they are and seem to be missing a way to reset them. So
	// better to disable interruptions.
	boost::this_thread::disable_interruption di;

	BakeCPURenderEngine *engine = (BakeCPURenderEngine *)renderEngine;
	Scene *scene = engine->renderConfig->scene;
	const PathTracer &pathTracer = engine->pathTracer;

	//--------------------------------------------------------------------------
	// Render the maps
	//--------------------------------------------------------------------------

	Properties samplerAdditionalProps;
	samplerAdditionalProps <<
		// II'm not working in screen space so I can not use adaptive sampling
		Property("sampler.random.adaptive.strength")(0.f) <<
		Property("sampler.sobol.adaptive.strength")(0.f) <<
		// Disable image plane meaning for samples 0 and 1
		Property("sampler.imagesamples.enable")(false);

	double lastPrintTime = WallClockTime();
	for (u_int mapInfoIndex = 0; mapInfoIndex < engine->mapInfos.size(); ++mapInfoIndex) {
		const BakeMapInfo &mapInfo = engine->mapInfos[mapInfoIndex];

		if (threadIndex == 0) {
			SLG_LOG("Baking map index: " << mapInfoIndex << "/" << engine->mapInfos.size());
			InitBakeWork(mapInfo);
		}
		
		// Synchronize
		engine->threadsSyncBarrier->wait();

		if (engine->currentSceneObjsToBake.size() == 0) {
			// Nothing to do
			continue;
		}

		//----------------------------------------------------------------------
		// Rendering initialization
		//----------------------------------------------------------------------

		// (engine->seedBase + 1) seed is used for sharedRndGen
		RandomGenerator *rndGen = new RandomGenerator(engine->seedBase + 1 + threadIndex);

		// Setup the sampler(s)

		Sampler *eyeSampler = engine->renderConfig->AllocSampler(rndGen, engine->mapFilm,
				engine->sampleSplatter, engine->samplerSharedData, samplerAdditionalProps);
		// Below, I need 3 additional samples
		eyeSampler->RequestSamples(PIXEL_NORMALIZED_ONLY, pathTracer.eyeSampleSize + 7);

		// Setup variance clamping
		VarianceClamping varianceClamping(pathTracer.sqrtVarianceClampMaxValue);

		// Setup PathTracer thread state
		PathTracerThreadState pathTracerThreadState(device,
				eyeSampler, nullptr,
				engine->renderConfig->scene, engine->mapFilm,
				&varianceClamping,
				true);

		//----------------------------------------------------------------------
		// Rendering
		//----------------------------------------------------------------------

		const PathVolumeInfo volInfo;
		for (u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
			// Check if we are in pause mode
			if (engine->pauseMode) {
				// Check every 100ms if I have to continue the rendering
				while (!boost::this_thread::interruption_requested() && engine->pauseMode)
					boost::this_thread::sleep(boost::posix_time::millisec(100));

				if (boost::this_thread::interruption_requested())
					break;
			}

			// Pick a scene object to sample
			float sceneObjPickPdf;
			const u_int currentSceneObjIndex = engine->currentSceneObjsDist->SampleDiscrete(eyeSampler->GetSample(0), &sceneObjPickPdf);
			const SceneObject *sceneObj = engine->currentSceneObjsToBake[currentSceneObjIndex];
			const ExtMesh *mesh = sceneObj->GetExtMesh();

			// Pick a triangle to sample
			float triPickPdf;
			const u_int triangleIndex = engine->currentSceneObjDist[currentSceneObjIndex]->SampleDiscrete(eyeSampler->GetSample(1), &triPickPdf);

			const float timeSample = eyeSampler->GetSample(4);
			Transform localToWorld;
			mesh->GetLocal2World(timeSample, localToWorld);

			// Origin
			Point samplePoint;
			float b0, b1, b2;
			mesh->Sample(localToWorld, triangleIndex, eyeSampler->GetSample(2), eyeSampler->GetSample(3),
					&samplePoint, &b0, &b1, &b2);
			
			const u_int sceneObjIndex = scene->objDefs.GetSceneObjectIndex(sceneObj);
			BSDF bsdf(*scene, sceneObjIndex, triangleIndex,
					samplePoint, b1, b2,
					timeSample, eyeSampler->GetSample(pathTracer.eyeSampleSize), &volInfo);

			// Ray direction
			EyePathInfo pathInfo;
			Vector sampledDir;
			float cosSampledDir;
			float bsdfPdfW;
			BSDFEvent bsdfEvent;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
					eyeSampler->GetSample(pathTracer.eyeSampleSize + 1),
					eyeSampler->GetSample(pathTracer.eyeSampleSize + 2),
					&bsdfPdfW, &cosSampledDir, &bsdfEvent);

			pathInfo.lastBSDFPdfW = bsdfPdfW;
			pathInfo.lastBSDFEvent = bsdfEvent;
			
			// Ray origin
			const Point rayOrig = bsdf.GetRayOrigin(sampledDir);

			//------------------------------------------------------------------
			// Set up the ray to trace
			//------------------------------------------------------------------

			Ray eyeRay(rayOrig, sampledDir,
					MachineEpsilon::E(rayOrig), numeric_limits<float>::infinity(),
					timeSample);

			//------------------------------------------------------------------
			// Set up the sample result
			//------------------------------------------------------------------

			PathTracer::ResetEyeSampleResults(pathTracerThreadState.eyeSampleResults);

			SampleResult &eyeSampleResult = pathTracerThreadState.eyeSampleResults[0];

			const UV filmUV(
					bsdf.hitPoint.uv[mapInfo.uvindex].u - floorf(bsdf.hitPoint.uv[mapInfo.uvindex].u),
					1.f - (bsdf.hitPoint.uv[mapInfo.uvindex].v - floorf(bsdf.hitPoint.uv[mapInfo.uvindex].v)));

			const float filmX = filmUV.u * pathTracerThreadState.film->GetWidth() - .5f;
			const float filmY = filmUV.v * pathTracerThreadState.film->GetHeight() - .5f;

			const u_int *subRegion = pathTracerThreadState.film->GetSubRegion();
			eyeSampleResult.pixelX = Clamp(Floor2UInt(filmX), subRegion[0], subRegion[1]);
			eyeSampleResult.pixelY = Clamp(Floor2UInt(filmY), subRegion[2], subRegion[3]);
			eyeSampleResult.filmX = filmX;
			eyeSampleResult.filmY = filmY;

			switch (mapInfo.type) {
				case COMBINED: {
					//----------------------------------------------------------
					// Render the surface point direct light
					//----------------------------------------------------------

					// To keep track of the number of rays traced
					const double deviceRayCount = device->GetTotalRaysCount();

					pathTracer.DirectLightSampling(pathTracerThreadState.device, scene,
							timeSample,
							eyeSampler->GetSample(pathTracer.eyeSampleSize + 3),
							eyeSampler->GetSample(pathTracer.eyeSampleSize + 4),
							eyeSampler->GetSample(pathTracer.eyeSampleSize + 5),
							eyeSampler->GetSample(pathTracer.eyeSampleSize + 6),
							eyeSampler->GetSample(pathTracer.eyeSampleSize + 7),
							pathInfo, 
							Spectrum(1.f), bsdf, &pathTracerThreadState.eyeSampleResults[0]);

					pathTracerThreadState.eyeSampleResults[0].rayCount += (float)(device->GetTotalRaysCount() - deviceRayCount);

					//----------------------------------------------------------
					// Render the received light from the path
					//----------------------------------------------------------

					pathTracer.RenderEyePath(pathTracerThreadState.device, pathTracerThreadState.scene,
							pathTracerThreadState.eyeSampler, pathInfo, eyeRay,
							pathTracerThreadState.eyeSampleResults);

					for(u_int i = 0; i < pathTracerThreadState.eyeSampleResults.size(); ++i) {
						SampleResult &sampleResult = pathTracerThreadState.eyeSampleResults[i];

						for(u_int j = 0; j < sampleResult.radiance.size(); ++j)
							sampleResult.radiance[j] *= bsdfSample;
					}
					break;
				}
				case LIGHTMAP: {
					//----------------------------------------------------------
					// Render the received direct light
					//----------------------------------------------------------

					// To keep track of the number of rays traced
					const double deviceRayCount = device->GetTotalRaysCount();

					pathTracer.DirectLightSampling(pathTracerThreadState.device, scene,
							timeSample,
							eyeSampler->GetSample(pathTracer.eyeSampleSize + 3),
							eyeSampler->GetSample(pathTracer.eyeSampleSize + 4),
							eyeSampler->GetSample(pathTracer.eyeSampleSize + 5),
							eyeSampler->GetSample(pathTracer.eyeSampleSize + 6),
							eyeSampler->GetSample(pathTracer.eyeSampleSize + 7),
							pathInfo, 
							Spectrum(1.f), bsdf, &pathTracerThreadState.eyeSampleResults[0],
							false);

					pathTracerThreadState.eyeSampleResults[0].rayCount += (float)(device->GetTotalRaysCount() - deviceRayCount);

					//----------------------------------------------------------
					// Render the received light from the path
					//----------------------------------------------------------

					pathTracer.RenderEyePath(pathTracerThreadState.device, pathTracerThreadState.scene,
							pathTracerThreadState.eyeSampler, pathInfo, eyeRay,
							pathTracerThreadState.eyeSampleResults);
					break;
				}
				default:
					throw runtime_error("Unknown bake type in BakeCPURenderThread::RenderFunc(): " + ToString(mapInfo.type));
			}

			//------------------------------------------------------------------
			// Variance clamping
			//------------------------------------------------------------------

			if (pathTracerThreadState.varianceClamping->hasClamping()) {
				for(u_int i = 0; i < pathTracerThreadState.eyeSampleResults.size(); ++i) {
					SampleResult &sampleResult = pathTracerThreadState.eyeSampleResults[i];

					// I clamp only eye paths samples (variance clamping would cut
					// SDS path values due to high scale of PSR samples)
					if (sampleResult.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED))
						pathTracerThreadState.varianceClamping->Clamp(*pathTracerThreadState.film, sampleResult);
				}
			}

			//------------------------------------------------------------------
			// Splat the sample results
			//------------------------------------------------------------------

			pathTracerThreadState.eyeSampler->NextSample(pathTracerThreadState.eyeSampleResults);

#ifdef WIN32
			// Work around Windows bad scheduling
			renderThread->yield();
#endif

			if (threadIndex == 0) {
				const double now = WallClockTime();
				if (now - lastPrintTime > 2.0) {
					// Print some information about the rendering progress
					const double elapsedTime = engine->mapFilm->GetTotalTime();
					const u_int pass = static_cast<u_int>(engine->mapFilm->GetTotalSampleCount() /
							(engine->mapFilm->GetWidth() * engine->mapFilm->GetHeight()));
					const float convergence = engine->mapFilm->GetConvergence();
					
					SLG_LOG("Baking map #" << mapInfoIndex << "/" << engine->mapInfos.size() << ": "
							"[Elapsed time " << int(elapsedTime) << " secs]"
							"[Samples " << pass << "]"
							"[Convergence " << (100.f * convergence) << "%]");

					lastPrintTime = now;
				}
			}

			// Check halt conditions
			if (engine->mapFilm->GetConvergence() == 1.f)
				break;

			if (engine->photonGICache) {
				const u_int spp = engine->mapFilm->GetTotalEyeSampleCount() / engine->mapFilm->GetPixelCount();
				engine->photonGICache->Update(threadIndex, spp);
			}
		}

		delete eyeSampler;
		delete rndGen;

		engine->threadsSyncBarrier->wait();

		if ((threadIndex == 0) && !boost::this_thread::interruption_requested()) {
			// Save the rendered map
			Properties props;
			props << Property("index")(mapInfo.imagePipelineIndex);
			engine->mapFilm->Output(mapInfo.fileName, FilmOutputs::RGB_IMAGEPIPELINE,
					&props);
		}

		if (boost::this_thread::interruption_requested())
			break;
	}

	threadDone = true;

	//SLG_LOG("[BakeCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
