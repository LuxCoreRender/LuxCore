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

#include <thread>
#include <mutex>
#include "luxrays/utils/thread.h"

#include "slg/engines/bakecpu/bakecpu.h"
#include "slg/volumes/volume.h"
#include "slg/utils/varianceclamping.h"
#include "slg/film/imagepipeline/plugins/bakemapmargin.h"
#include "luxrays/core/randomgen.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace std::literals::chrono_literals;

//------------------------------------------------------------------------------
// BakeCPU RenderThread
//------------------------------------------------------------------------------

BakeCPURenderThread::BakeCPURenderThread(BakeCPURenderEngine *engine,
		const u_int index, IntersectionDeviceRef device) :
		CPUNoTileRenderThread(engine, index, device) {
}

void BakeCPURenderThread::InitBakeWork(const BakeMapInfo &mapInfo) {
	BakeCPURenderEngine *engine = (BakeCPURenderEngine *)renderEngine;
	const auto& scene = engine->renderConfig.GetScene();

	// Lock the main film
	std::unique_lock<std::mutex> lock(*engine->filmMutex);

	// Print some information
	SLG_LOG("Baking map: " << mapInfo.fileName);
	SLG_LOG("Resolution: " << mapInfo.width << "x" << mapInfo.height);

	engine->currentSceneObjsToBake.clear();
	engine->currentSceneObjsToBakeArea.clear();
	
	if (engine->skipExistingMapFiles) {
		// Check if the file exist
		if (std::filesystem::exists(mapInfo.fileName)) {
			SLG_LOG("Bake map file already exists: " << mapInfo.fileName);
			// Skip this map
			return;
		}
	}

	// Initialize the map Film
	engine->mapFilm = Film::Create(mapInfo.width, mapInfo.height, NULL);
	engine->GetMapFilm().CopyDynamicSettings(engine->GetFilm());
	// Copy the halt conditions too
	engine->GetMapFilm().CopyHaltSettings(engine->GetFilm());
	engine->GetMapFilm().SetThreadCount(engine->renderThreads.size());
	engine->GetMapFilm().Init();

	// Build the list of object to bake and each mesh area
	for (auto const &objName : mapInfo.objectNames) {
		auto& sceneObj = scene.GetObjects().GetSceneObject(objName);
		engine->currentSceneObjsToBake.push_back(&sceneObj);
		//if (sceneObj)
			//engine->currentSceneObjsToBake.push_back(sceneObj);
		//else
			//SLG_LOG("WARNING: Unknown object to bake ignored (" << objName << ")");
	}

	if (engine->currentSceneObjsToBake.size() == 0)
		return;

	// To sample the each mesh triangle according its area
	engine->currentSceneObjDist.resize(engine->currentSceneObjsToBake.size());
	engine->currentSceneObjsToBakeArea.resize(engine->currentSceneObjsToBake.size());

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int sceneObjIndex = 0; sceneObjIndex < engine->currentSceneObjDist.size(); ++sceneObjIndex) {
		auto& sceneObj = *engine->currentSceneObjsToBake[sceneObjIndex];
		auto& mesh = sceneObj.GetExtMesh();

		Transform localToWorld;
localToWorld = 		sceneObj.GetExtMesh().GetLocal2World(0.f);

		std::vector<float> trisArea(mesh.GetTotalTriangleCount());
		engine->currentSceneObjsToBakeArea[sceneObjIndex] = 0.f;
		for (u_int triIndex = 0; triIndex < mesh.GetTotalTriangleCount(); ++triIndex) {
			trisArea[triIndex] = mesh.GetTriangleArea(localToWorld, triIndex);
			engine->currentSceneObjsToBakeArea[sceneObjIndex] += trisArea[triIndex];
		}

		engine->currentSceneObjDist[sceneObjIndex] = std::make_unique<Distribution1D>(trisArea);
	}

	// To sample the meshes according their area
	engine->currentSceneObjsDist = std::make_unique<Distribution1D>(
		engine->currentSceneObjsToBakeArea
	);

	// Reset the main film
	engine->GetFilm().Reset();
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
	vector<SampleResult> &sampleResults = state.GetEyeSampleResults();
	SampleResult &sampleResult = sampleResults[0];

	// Pick a scene object to sample
	float sceneObjPickPdf;
	const u_int currentSceneObjIndex = engine->currentSceneObjsDist->SampleDiscrete(state.GetEyeSampler().GetSample(0), &sceneObjPickPdf);
	auto& sceneObj = *engine->currentSceneObjsToBake[currentSceneObjIndex];
	auto& mesh = sceneObj.GetExtMesh();

	// Pick a triangle to sample
	float triPickPdf;
	const u_int triangleIndex = engine->currentSceneObjDist[currentSceneObjIndex]->SampleDiscrete(state.GetEyeSampler().GetSample(1), &triPickPdf);

	const float timeSample = state.GetEyeSampler().GetSample(4);
	Transform localToWorld;
localToWorld = 	mesh.GetLocal2World(timeSample);

	// Origin
	Point samplePoint;
	float b0, b1, b2;
	std::tie(samplePoint, b0, b1, b2) = mesh.Sample(localToWorld, triangleIndex,
			state.GetEyeSampler().GetSample(2), state.GetEyeSampler().GetSample(3));

	const u_int sceneObjIndex = state.scene.GetObjects().GetSceneObjectIndex(sceneObj);
	const PathVolumeInfo volInfo;
	BSDF bsdf(state.scene, sceneObjIndex, triangleIndex,
			samplePoint, b1, b2,
			timeSample, state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize), &volInfo);

	//--------------------------------------------------------------------------
	// Set up the sample result
	//--------------------------------------------------------------------------

	PathTracer::ResetEyeSampleResults(sampleResults);
	SetSampleResultXY(mapInfo, bsdf.hitPoint, state.GetFilm(), sampleResult);

	switch (mapInfo.type) {
		case COMBINED: {
			//------------------------------------------------------------------
			// Render the surface point direct light
			//------------------------------------------------------------------

			// To keep track of the number of rays traced
			const double deviceRayCount = device.GetTotalRaysCount();

			EyePathInfo pathInfo;
			// I have to set isPassThroughPath to false to avoid problems with the
			// force black background option (by default, it is set to true)
			pathInfo.isPassThroughPath = false;

			const PathTracer::DirectLightResult directLightResult = pathTracer.DirectLightSampling(state.device, state.scene,
					timeSample,
					state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 3),
					state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 4),
					state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 5),
					state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 6),
					state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 7),
					pathInfo, 
					Spectrum(1.f), bsdf, &sampleResult);

			sampleResult.rayCount += (float)(device.GetTotalRaysCount() - deviceRayCount);

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
						state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 1),
						state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 2),
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

				pathTracer.RenderEyePath(
					state.device, state.scene,
					state.GetEyeSampler(),
					pathInfo,
					eyeRay,
					bsdfSample,
					state.GetEyeSampleResults()
				);
			}
			break;
		}
		case LIGHTMAP: {
			//--------------------------------------------------------------
			// Render the received direct light
			//--------------------------------------------------------------

			// To keep track of the number of rays traced
			const double deviceRayCount = device.GetTotalRaysCount();

			EyePathInfo pathInfo;
			// I have to set isPassThroughPath to false to avoid problems with the
			// force black background option (by default, it is set to true)
			pathInfo.isPassThroughPath = false;

			const PathTracer::DirectLightResult directLightResult = pathTracer.DirectLightSampling(state.device, state.scene,
					timeSample,
					state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 3),
					state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 4),
					state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 5),
					state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 6),
					state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 7),
					pathInfo, 
					Spectrum(1.f), bsdf, &sampleResult,
					false);

			sampleResult.rayCount += (float)(device.GetTotalRaysCount() - deviceRayCount);

			if (bsdf.IsShadowCatcher() && (directLightResult != PathTracer::SHADOWED))
				sampleResult.alpha = 0.f;
			else {
				sampleResult.alpha = 1.f - Clamp(bsdf.GetPassThroughTransparency(false).Y(), 0.f, 1.f);

				//--------------------------------------------------------------
				// Set up the next ray to trace
				//--------------------------------------------------------------

				// Ray direction
				const float u1 = state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 1);
				const float u2 = state.GetEyeSampler().GetSample(pathTracer.eyeSampleSize + 2);
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
						state.GetEyeSampler(), pathInfo, eyeRay, Spectrum(NdotL * INV_PI / samplePdf),
						state.GetEyeSampleResults());
			}
			break;
		}
		default:
			throw runtime_error("Unknown bake type in BakeCPURenderThread::RenderFunc(std::stop_token stop_token): " + ToString(mapInfo.type));
	}

	//--------------------------------------------------------------------------
	// AOV support
	//--------------------------------------------------------------------------

	if (bsdf.IsAlbedoEndPoint(pathTracer.albedoSpecularSetting, pathTracer.albedoSpecularGlossinessThreshold))
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

	// Bake only caustics (note: pathInfo is not updated with the current hit point):
	// - the path must be specular;
	// - the current hit point must be diffuse/glossy (aka not specular)
	if (pathInfo.IsSpecularPath() &&
			!pathInfo.IsNearlySpecular(bsdf.GetEventTypes(), bsdf.GetGlossiness(), engine->pathTracer.hybridBackForwardGlossinessThreshold) &&
			(!engine->pathTracer.hybridBackForwardEnable || (pathInfo.depth.depth > 0))) {
		// Check if the hit point is on one of the objects I'm baking
		for (u_int i = 0; i < engine->currentSceneObjsToBake.size(); ++i) {
			if (engine->currentSceneObjsToBake[i] == &bsdf.GetSceneObject()) {
				SampleResult &sampleResult = PathTracer::AddLightSampleResult(sampleResults, engine->GetMapFilm());

				SetSampleResultXY(mapInfo, bsdf.hitPoint, engine->GetMapFilm(), sampleResult);

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
	
	const PathTracer::ConnectToEyeCallBackType connectToEyeCallBack = std::bind(
			&BakeCPURenderThread::RenderConnectToEyeCallBack, this, mapInfo, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);

	pathTracer.RenderLightSample(state.device, state.scene, state.GetFilm(), state.GetLightSampler(),
			state.GetLightSampleResults(), connectToEyeCallBack);
}

void BakeCPURenderThread::RenderSample(const BakeMapInfo &mapInfo, PathTracerThreadState &state) const {
	BakeCPURenderEngine *engine = (BakeCPURenderEngine *)renderEngine;
	const PathTracer &pathTracer = engine->pathTracer;

	// Check if I have to trace an eye or light path
	bool eyeSampling = pathTracer.HasToRenderEyeSample(state);
	SamplerRef sampler =
		 eyeSampling ? state.GetEyeSampler() : state.GetLightSampler();

	std::vector<SampleResult>& sampleResults = eyeSampling ?
		state.GetEyeSampleResults() : state.GetLightSampleResults();

	if (eyeSampling)
		RenderEyeSample(mapInfo, state);
	else
		RenderLightSample(mapInfo, state);

	// Variance clamping
	pathTracer.ApplyVarianceClamp(state, sampleResults);

	sampler.NextSample(sampleResults);
}

void BakeCPURenderThread::RenderFunc(std::stop_token stop_token) {
#ifndef NDEBUG
	SLG_LOG("[BakeCPURenderEngine::" << threadIndex << "] Rendering thread started");
#endif

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
		engine->threadsSyncBarrier->arrive_and_wait();

		if (engine->currentSceneObjsToBake.size() == 0) {
			// Nothing to do
			continue;
		}

		//----------------------------------------------------------------------
		// Rendering initialization
		//----------------------------------------------------------------------

		// (engine->seedBase + 1) seed is used for sharedRndGen
		const std::unique_ptr<RandomGenerator> rndGen = std::make_unique<RandomGenerator>(engine->seedBase + 1 + threadIndex);

		// Setup the sampler(s)

		SamplerUPtr lightSampler;

		auto eyeSampler = engine->renderConfig.AllocSampler(
			rndGen,
			engine->GetMapFilm(),
			engine->GetSampleSplatter(),
			engine->samplerSharedData,
			samplerAdditionalProps
		);
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

			lightSampler = Sampler::FromProperties(
				props,
				std::cref(rndGen),
				FilmPtr(&engine->GetMapFilm()),
				FilmSampleSplatter::Null,
				engine->lightSamplerSharedData);
			lightSampler->SetThreadIndex(threadIndex);

			lightSampler->RequestSamples(SCREEN_NORMALIZED_ONLY, pathTracer.lightSampleSize);
		}

		// Setup variance clamping
		VarianceClamping varianceClamping(pathTracer.sqrtVarianceClampMaxValue);

		// Setup PathTracer thread state
		PathTracerThreadState pathTracerThreadState(device,
				eyeSampler, lightSampler,
				engine->renderConfig.GetScene(),
				engine->GetMapFilm(),
				&varianceClamping,
				true);

		//----------------------------------------------------------------------
		// Rendering
		//----------------------------------------------------------------------

		for (u_int steps = 0; !stop_token.stop_requested(); ++steps) {
			// Check if we are in pause mode
			if (engine->pauseMode) {
				// Check every 100ms if I have to continue the rendering
				while (!stop_token.stop_requested() && engine->pauseMode)
					std::this_thread::sleep_for(100ms);

				if (stop_token.stop_requested())
					break;
			}

			RenderSample(mapInfo, pathTracerThreadState);

#ifdef WIN32
			// Work around Windows bad scheduling
            std::this_thread::yield();
#endif

			if (threadIndex == 0) {
				const double now = WallClockTime();
				if (now - lastPrintTime > 2.0) {
					// Print some information about the rendering progress
					const double elapsedTime = engine->GetMapFilm().GetTotalTime();
					const u_int pass = static_cast<u_int>(engine->GetMapFilm().GetTotalSampleCount() /
							(engine->GetMapFilm().GetWidth() * engine->GetMapFilm().GetHeight()));
					const float convergence = engine->GetMapFilm().GetConvergence();
					
					SLG_LOG("Baking map #" << mapInfoIndex << "/" << engine->mapInfos.size() << ": "
							"[Elapsed time " << int(elapsedTime) << " secs]"
							"[Samples " << pass << "]"
							"[Convergence " << (100.f * convergence) << "%]");

					lastPrintTime = now;
				}
			}

			// Check halt conditions
			if (engine->GetMapFilm().GetConvergence() == 1.f)
				break;
                        if (stop_token.stop_requested())
                                break;

			if (engine->photonGICache) {
                                const u_int spp = engine->GetMapFilm().GetTotalEyeSampleCount() / engine->GetMapFilm().GetPixelCount();
                                engine->photonGICache->Update(threadIndex, spp);
                        }

		}

		//rndGen.reset();

		engine->threadsSyncBarrier->arrive_and_wait();

		if ((threadIndex == 0) && !stop_token.stop_requested()) {
			// Execute the image pipeline
			engine->GetMapFilm().ExecuteImagePipeline(mapInfo.imagePipelineIndex);

			// Apply margin options
			if (engine->marginPixels > 0)
				BakeMapMarginPlugin::Apply(engine->GetMapFilm(), mapInfo.imagePipelineIndex,
						engine->marginPixels, engine->marginSamplesThreshold, true);

			// Save the rendered map
			auto props = std::make_unique<Properties>();
			*props << Property("index")(mapInfo.imagePipelineIndex);
				engine->GetMapFilm().Output(
				mapInfo.fileName,
				engine->GetMapFilm().HasChannel(Film::ALPHA) ?
					FilmOutputs::RGBA_IMAGEPIPELINE :
					FilmOutputs::RGB_IMAGEPIPELINE,
				props,
				false
			);
		}

		engine->threadsSyncBarrier->arrive_and_wait();

		if (stop_token.stop_requested())
			break;
	}

	threadDone = true;

	// This is done to stop threads pending on barrier wait
	// inside engine->photonGICache->Update(). This can happen when an
	// halt condition is satisfied.
	if (engine->photonGICache)
		engine->photonGICache->FinishUpdate(threadIndex);

#ifndef NDEBUG
	SLG_LOG("[BakeCPURenderEngine::" << threadIndex << "] Rendering thread halted");
#endif
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
