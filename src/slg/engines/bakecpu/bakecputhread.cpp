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

#include "luxrays/utils/thread.h"

#include "slg/engines/bakecpu/bakecpu.h"
#include "slg/volumes/volume.h"
#include "slg/utils/varianceclamping.h"
#include "slg/film/imagepipeline/plugins/bakemapmargin.h"

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
	engine->currentSceneObjsToBakeArea.clear();
	
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
	engine->mapFilm->SetThreadCount(engine->renderThreads.size());
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
	engine->currentSceneObjsToBakeArea.resize(engine->currentSceneObjsToBake.size());

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
		engine->currentSceneObjsToBakeArea[sceneObjIndex] = 0.f;
		for (u_int triIndex = 0; triIndex < mesh->GetTotalTriangleCount(); ++triIndex) {
			trisArea[triIndex] = mesh->GetTriangleArea(localToWorld, triIndex);
			engine->currentSceneObjsToBakeArea[sceneObjIndex] += trisArea[triIndex];
		}
		
		engine->currentSceneObjDist[sceneObjIndex] = new Distribution1D(&trisArea[0], trisArea.size());
	}
	
	// To sample the meshes according their area
	delete engine->currentSceneObjsDist;
	engine->currentSceneObjsDist = new Distribution1D(&engine->currentSceneObjsToBakeArea[0], engine->currentSceneObjsToBakeArea.size());

	// Reset the main film
	engine->film->Reset();
}

void BakeCPURenderThread::SetSampleResultXY(const BakeMapInfo &mapInfo,
		const HitPoint &hitPoint, const Film &film, SampleResult &sampleResult) const {
	const UV uv = hitPoint.GetUV(mapInfo.uvindex);
	const UV filmUV(
			uv.u - floorf(uv.u),
			1.f - (uv.v - floorf(uv.v)));

	const float filmX = filmUV.u * film.GetWidth() - .5f;
	const float filmY = filmUV.v * film.GetHeight() - .5f;

	const u_int *subRegion = film.GetSubRegion();
	sampleResult.pixelX = Clamp(Floor2UInt(filmX), subRegion[0], subRegion[1]);
	sampleResult.pixelY = Clamp(Floor2UInt(filmY), subRegion[2], subRegion[3]);
	sampleResult.filmX = filmX;
	sampleResult.filmY = filmY;
}

void BakeCPURenderThread::RenderEyeSample(const BakeMapInfo &mapInfo, PathTracerThreadState &state) const {
	BakeCPURenderEngine *engine = (BakeCPURenderEngine *)renderEngine;
	const PathTracer &pathTracer = engine->pathTracer;
	vector<SampleResult> &sampleResults = state.eyeSampleResults;
	SampleResult &sampleResult = sampleResults[0];

	// Pick a scene object to sample
	float sceneObjPickPdf;
	const u_int currentSceneObjIndex = engine->currentSceneObjsDist->SampleDiscrete(state.eyeSampler->GetSample(0), &sceneObjPickPdf);
	const SceneObject *sceneObj = engine->currentSceneObjsToBake[currentSceneObjIndex];
	const ExtMesh *mesh = sceneObj->GetExtMesh();

	// Pick a triangle to sample
	float triPickPdf;
	const u_int triangleIndex = engine->currentSceneObjDist[currentSceneObjIndex]->SampleDiscrete(state.eyeSampler->GetSample(1), &triPickPdf);

	const float timeSample = state.eyeSampler->GetSample(4);
	Transform localToWorld;
	mesh->GetLocal2World(timeSample, localToWorld);

	// Origin
	Point samplePoint;
	float b0, b1, b2;
	mesh->Sample(localToWorld, triangleIndex, state.eyeSampler->GetSample(2), state.eyeSampler->GetSample(3),
			&samplePoint, &b0, &b1, &b2);

	const u_int sceneObjIndex = state.scene->objDefs.GetSceneObjectIndex(sceneObj);
	const PathVolumeInfo volInfo;
	BSDF bsdf(*state.scene, sceneObjIndex, triangleIndex,
			samplePoint, b1, b2,
			timeSample, state.eyeSampler->GetSample(pathTracer.eyeSampleSize), &volInfo);

	//--------------------------------------------------------------------------
	// Set up the sample result
	//--------------------------------------------------------------------------

	PathTracer::ResetEyeSampleResults(sampleResults);
	SetSampleResultXY(mapInfo, bsdf.hitPoint, *state.film, sampleResult);

	switch (mapInfo.type) {
		case COMBINED: {
			//------------------------------------------------------------------
			// Render the surface point direct light
			//------------------------------------------------------------------

			// To keep track of the number of rays traced
			const double deviceRayCount = device->GetTotalRaysCount();

			EyePathInfo pathInfo;
			// I have to set isPassThroughPath to false to avoid problems with the
			// force black background option (by default, it is set to true)
			pathInfo.isPassThroughPath = false;

			const PathTracer::DirectLightResult directLightResult = pathTracer.DirectLightSampling(state.device, state.scene,
					timeSample,
					state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 3),
					state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 4),
					state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 5),
					state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 6),
					state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 7),
					pathInfo, 
					Spectrum(1.f), bsdf, &sampleResult);

			sampleResult.rayCount += (float)(device->GetTotalRaysCount() - deviceRayCount);

			if (bsdf.IsShadowCatcher() && (directLightResult != PathTracer::SHADOWED))
				sampleResult.alpha = 0.f;
			else {
				sampleResult.alpha = 1.f - Clamp(bsdf.GetPassThroughTransparency(false).Y(), 0.f, 1.f);

				//--------------------------------------------------------------
				// Set up the next ray to trace
				//--------------------------------------------------------------

				// Ray direction
				Vector sampledDir;
				float cosSampledDir;
				float bsdfPdfW;
				BSDFEvent bsdfEvent;
				const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
						state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 1),
						state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 2),
						&bsdfPdfW, &cosSampledDir, &bsdfEvent);

				pathInfo.AddVertex(bsdf, bsdfEvent, bsdfPdfW, pathTracer.hybridBackForwardGlossinessThreshold);

				// Ray origin
				const Point rayOrig = bsdf.GetRayOrigin(sampledDir);

				Ray eyeRay(rayOrig, sampledDir,
						MachineEpsilon::E(rayOrig), numeric_limits<float>::infinity(),
						timeSample);

				//--------------------------------------------------------------
				// Render the received light from the path
				//--------------------------------------------------------------

				pathTracer.RenderEyePath(state.device, state.scene,
						state.eyeSampler, pathInfo, eyeRay, bsdfSample,
						state.eyeSampleResults);
			}
			break;
		}
		case LIGHTMAP: {
			//--------------------------------------------------------------
			// Render the received direct light
			//--------------------------------------------------------------

			// To keep track of the number of rays traced
			const double deviceRayCount = device->GetTotalRaysCount();

			EyePathInfo pathInfo;
			// I have to set isPassThroughPath to false to avoid problems with the
			// force black background option (by default, it is set to true)
			pathInfo.isPassThroughPath = false;

			const PathTracer::DirectLightResult directLightResult = pathTracer.DirectLightSampling(state.device, state.scene,
					timeSample,
					state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 3),
					state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 4),
					state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 5),
					state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 6),
					state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 7),
					pathInfo, 
					Spectrum(1.f), bsdf, &sampleResult,
					false);

			sampleResult.rayCount += (float)(device->GetTotalRaysCount() - deviceRayCount);

			if (bsdf.IsShadowCatcher() && (directLightResult != PathTracer::SHADOWED))
				sampleResult.alpha = 0.f;
			else {
				sampleResult.alpha = 1.f - Clamp(bsdf.GetPassThroughTransparency(false).Y(), 0.f, 1.f);

				//--------------------------------------------------------------
				// Set up the next ray to trace
				//--------------------------------------------------------------

				// Ray direction
				const float u1 = state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 1);
				const float u2 = state.eyeSampler->GetSample(pathTracer.eyeSampleSize + 2);
				const Vector localSampledDir = UniformSampleHemisphere(u1, u2);
				const Vector sampledDir = bsdf.GetFrame().ToWorld(localSampledDir);

				const float samplePdf = UniformHemispherePdf(u1, u2);
				pathInfo.AddVertex(bsdf, DIFFUSE | REFLECT, samplePdf, pathTracer.hybridBackForwardGlossinessThreshold);

				// Ray origin
				const Point rayOrig = bsdf.GetRayOrigin(sampledDir);

				Ray eyeRay(rayOrig, sampledDir,
						MachineEpsilon::E(rayOrig), numeric_limits<float>::infinity(),
						timeSample);

				//--------------------------------------------------------------
				// Render the received light from the path
				//--------------------------------------------------------------

				const float NdotL = Dot(bsdf.hitPoint.shadeN, sampledDir);
				pathTracer.RenderEyePath(state.device, state.scene,
						state.eyeSampler, pathInfo, eyeRay, Spectrum(NdotL * INV_PI / samplePdf),
						state.eyeSampleResults);
			}
			break;
		}
		default:
			throw runtime_error("Unknown bake type in BakeCPURenderThread::RenderFunc(): " + ToString(mapInfo.type));
	}

	//--------------------------------------------------------------------------
	// AOV support
	//--------------------------------------------------------------------------

	if (bsdf.IsAlbedoEndPoint())
		sampleResult.albedo = bsdf.Albedo();

	sampleResult.depth = 0.f;
	sampleResult.position = bsdf.hitPoint.p;
	sampleResult.geometryNormal = bsdf.hitPoint.geometryN;
	sampleResult.shadingNormal = bsdf.hitPoint.shadeN;
	sampleResult.materialID = bsdf.GetMaterialID();
	sampleResult.objectID = bsdf.GetObjectID();
	sampleResult.uv = bsdf.hitPoint.GetUV(0);
}

void BakeCPURenderThread::RenderConnectToEyeCallBack(const BakeMapInfo &mapInfo,
		const LightPathInfo &pathInfo,
		const BSDF &bsdf, const u_int lightID,
		const Spectrum &lightPathFlux, vector<SampleResult> &sampleResults) const {
	BakeCPURenderEngine *engine = (BakeCPURenderEngine *)renderEngine;

	// Bake only caustics
	if (pathInfo.IsSDPath() && (!engine->pathTracer.hybridBackForwardEnable || (pathInfo.depth.depth > 0))) {
		// Check if the hit point is on one of the objects I'm baking
		for (u_int i = 0; i < engine->currentSceneObjsToBake.size(); ++i) {
			if (engine->currentSceneObjsToBake[i] == bsdf.GetSceneObject()) {
				SampleResult &sampleResult = PathTracer::AddLightSampleResult(sampleResults, engine->mapFilm);

				SetSampleResultXY(mapInfo, bsdf.hitPoint, *engine->mapFilm, sampleResult);

				const float fluxToRadianceFactor = 1.f / engine->currentSceneObjsToBakeArea[i];

				BSDFEvent event;
				const Spectrum bsdfEval = bsdf.Evaluate(Vector(bsdf.hitPoint.shadeN), &event);

				sampleResult.radiance[lightID] = lightPathFlux * fluxToRadianceFactor * bsdfEval;

				return;
			}
		}
	}
}

void BakeCPURenderThread::RenderLightSample(const BakeMapInfo &mapInfo, PathTracerThreadState &state) const {
	BakeCPURenderEngine *engine = (BakeCPURenderEngine *)renderEngine;
	const PathTracer &pathTracer = engine->pathTracer;
	
	const PathTracer::ConnectToEyeCallBackType connectToEyeCallBack = boost::bind(
			&BakeCPURenderThread::RenderConnectToEyeCallBack, this, mapInfo, _1, _2, _3, _4, _5);

	pathTracer.RenderLightSample(state.device, state.scene, state.film, state.lightSampler,
			state.lightSampleResults, connectToEyeCallBack);
}

void BakeCPURenderThread::RenderSample(const BakeMapInfo &mapInfo, PathTracerThreadState &state) const {
	BakeCPURenderEngine *engine = (BakeCPURenderEngine *)renderEngine;
	const PathTracer &pathTracer = engine->pathTracer;

	// Check if I have to trace an eye or light path
	Sampler *sampler;
	vector<SampleResult> *sampleResults;
	if (pathTracer.HasToRenderEyeSample(state)) {
		// Trace an eye path
		sampler = state.eyeSampler;
		sampleResults = &state.eyeSampleResults;
	} else {
		// Trace a light path
		sampler = state.lightSampler;
		sampleResults = &state.lightSampleResults;
	}
	if (sampler == state.eyeSampler)
		RenderEyeSample(mapInfo, state);
	else
		RenderLightSample(mapInfo, state);

	// Variance clamping
	pathTracer.ApplyVarianceClamp(state, *sampleResults);

	sampler->NextSample(*sampleResults);
}

void BakeCPURenderThread::RenderFunc() {
	//SLG_LOG("[BakeCPURenderEngine::" << threadIndex << "] Rendering thread started");

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);

	BakeCPURenderEngine *engine = (BakeCPURenderEngine *)renderEngine;
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

		Sampler *eyeSampler = nullptr;
		Sampler *lightSampler = nullptr;

		eyeSampler = engine->renderConfig->AllocSampler(rndGen, engine->mapFilm,
				engine->sampleSplatter, engine->samplerSharedData, samplerAdditionalProps);
		eyeSampler->SetThreadIndex(threadIndex);
		// Below, I need 7 additional samples
		eyeSampler->RequestSamples(PIXEL_NORMALIZED_ONLY, pathTracer.eyeSampleSize + 8);

		if (pathTracer.hybridBackForwardEnable) {
			// Light path sampler is always Metropolis
			Properties props;
			props <<
				Property("sampler.type")("METROPOLIS") <<
				// Disable image plane meaning for samples 0 and 1
				Property("sampler.imagesamples.enable")(false) <<
				Property("sampler.metropolis.addonlycaustics")(true);

			lightSampler = Sampler::FromProperties(props, rndGen, engine->mapFilm, nullptr,
					engine->lightSamplerSharedData);
			lightSampler->SetThreadIndex(threadIndex);

			lightSampler->RequestSamples(SCREEN_NORMALIZED_ONLY, pathTracer.lightSampleSize);
		}

		// Setup variance clamping
		VarianceClamping varianceClamping(pathTracer.sqrtVarianceClampMaxValue);

		// Setup PathTracer thread state
		PathTracerThreadState pathTracerThreadState(device,
				eyeSampler, lightSampler,
				engine->renderConfig->scene, engine->mapFilm,
				&varianceClamping,
				true);

		//----------------------------------------------------------------------
		// Rendering
		//----------------------------------------------------------------------

		for (u_int steps = 0; !boost::this_thread::interruption_requested(); ++steps) {
			// Check if we are in pause mode
			if (engine->pauseMode) {
				// Check every 100ms if I have to continue the rendering
				while (!boost::this_thread::interruption_requested() && engine->pauseMode)
					boost::this_thread::sleep(boost::posix_time::millisec(100));

				if (boost::this_thread::interruption_requested())
					break;
			}

			RenderSample(mapInfo, pathTracerThreadState);

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
				try {
					const u_int spp = engine->mapFilm->GetTotalEyeSampleCount() / engine->mapFilm->GetPixelCount();
					engine->photonGICache->Update(threadIndex, spp);
				} catch (boost::thread_interrupted &ti) {
					// I have been interrupted, I must stop
					break;
				}
			}
		}

		delete eyeSampler;
		delete rndGen;

		engine->threadsSyncBarrier->wait();

		if ((threadIndex == 0) && !boost::this_thread::interruption_requested()) {
			// Execute the image pipeline
			engine->mapFilm->ExecuteImagePipeline(mapInfo.imagePipelineIndex);

			// Apply margin options
			if (engine->marginPixels > 0)
				BakeMapMarginPlugin::Apply(*engine->mapFilm, mapInfo.imagePipelineIndex,
						engine->marginPixels, engine->marginSamplesThreshold, true);

			// Save the rendered map
			Properties props;
			props << Property("index")(mapInfo.imagePipelineIndex);
			engine->mapFilm->Output(mapInfo.fileName,
					engine->mapFilm->HasChannel(Film::ALPHA) ? FilmOutputs::RGBA_IMAGEPIPELINE : FilmOutputs::RGB_IMAGEPIPELINE,
					&props, false);			
		}

		engine->threadsSyncBarrier->wait();

		if (boost::this_thread::interruption_requested())
			break;
	}

	threadDone = true;

	// This is done to interrupt thread pending on barrier wait
	// inside engine->photonGICache->Update(). This can happen when an
	// halt condition is satisfied.
	for (u_int i = 0; i < engine->renderThreads.size(); ++i)
		engine->renderThreads[i]->Interrupt();

	//SLG_LOG("[BakeCPURenderEngine::" << threadIndex << "] Rendering thread halted");
}
