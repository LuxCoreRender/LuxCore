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

#include <vector>

#include <boost/thread.hpp>

#include "luxrays/utils/thread.h"

#include "slg/cameras/camera.h"
#include "slg/samplers/sobol.h"
#include "slg/scene/scene.h"
#include "slg/utils/film2sceneradius.h"
#include "slg/utils/pathdepthinfo.h"
#include "slg/engines/renderengine.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// This function estimates the value of a scene radius starting from film plane
// radius (without material ray differential support)
//------------------------------------------------------------------------------

namespace slg {

static void GenerateEyeRay(const Camera *camera, Ray &eyeRay,
		PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult,
		float &dpdx, float &dpdy, const float imagePlaneRadius,
		const float timeStart, const float timeEnd) {
	// Evaluate the delta x/y over the image plane in pixel

	const float imagePlaneDeltaX = camera->filmWidth * imagePlaneRadius;
	const float imagePlaneDeltaY = camera->filmHeight * imagePlaneRadius;

	// Evaluate the camera ray

	// I intentionally ignore film sub-region to not have the estimated best
	// radius affected by border rendering
	sampleResult.filmX = sampler->GetSample(0) * (camera->filmWidth - 1);
	sampleResult.filmY = sampler->GetSample(1) * (camera->filmHeight - 1);

	const float timeSample = sampler->GetSample(4);
	const float time = (timeStart <= timeEnd) ?
		Lerp(timeSample, timeStart, timeEnd) :
		camera->GenerateRayTime(timeSample);

	const float u0 = sampler->GetSample(2);
	const float u1 = sampler->GetSample(3);

	camera->GenerateRay(time, sampleResult.filmX, sampleResult.filmY,
			&eyeRay, &volInfo, u0, u1);
	
	// I'm lacking the support for true ray differentials in camera object
	// interface so I resort to this simple method 

	if (camera->GetType() == Camera::ORTHOGRAPHIC) {
		dpdx = 1.f / imagePlaneDeltaX;
		dpdy = 1.f / imagePlaneDeltaY;
	} else {
		// Evaluate the camera ray + imagePlaneDeltaX

		Ray eyeRayDeltaX;
		PathVolumeInfo volInfoDeltaX;
		camera->GenerateRay(time, sampleResult.filmX + imagePlaneDeltaX, sampleResult.filmY,
				&eyeRayDeltaX, &volInfoDeltaX, u0, u1);

		// Evaluate the camera ray + imagePlaneDeltaY

		Ray eyeRayDeltaY;
		PathVolumeInfo volInfoDeltaY;
		camera->GenerateRay(time, sampleResult.filmX, sampleResult.filmY + imagePlaneDeltaY,
				&eyeRayDeltaY, &volInfoDeltaY, u0, u1);

		const float t0 = 0.f;
		const float t1 = 1.f;

		// Evaluate dpdx
		dpdx = fabs(Distance(eyeRay(t1), eyeRayDeltaX(t1)) - Distance(eyeRay(t0), eyeRayDeltaX(t0)));
		// Evaluate dpdy
		dpdy = fabs(Distance(eyeRay(t1), eyeRayDeltaY(t1)) - Distance(eyeRay(t0), eyeRayDeltaY(t0)));
	}
}

// To work around boost::thread() 10 arguments limit
typedef struct Film2SceneRadiusThreadParams {
	Film2SceneRadiusThreadParams() : accumulatedRadiusSize(0.f), radiusSizeCount(0) {
	}

	u_int threadIndex;
	u_int workSize;
	const Scene *scene;
	float imagePlaneRadius;
	u_int maxPathDepth;
	float timeStart, timeEnd;
	const Film2SceneRadiusValidator *validator;
	
	float accumulatedRadiusSize;
	u_int radiusSizeCount;
} Film2SceneRadiusThreadParams;

static void Film2SceneRadiusThread(Film2SceneRadiusThreadParams &params) {
	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(params.threadIndex);

	// Hard coded RR parameters
	const u_int rrDepth = 3;
	const float rrImportanceCap = .5f;

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	const Camera *camera = params.scene->camera;

	// Initialize the sampler
	RandomGenerator rnd(1 + params.threadIndex);
	SobolSamplerSharedData sobolSharedData(131, nullptr);
	SobolSampler sampler(&rnd, NULL, NULL, true, 0.f, 0.f,
			16, 16, 1, 1,
			&sobolSharedData);
	
	// Request the samples
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 4;
	const u_int sampleSize = 
		sampleBootSize + // To generate eye ray
		params.maxPathDepth * sampleStepSize; // For each path vertex

	sampler.RequestSamples(PIXEL_NORMALIZED_ONLY, sampleSize);
		// Initialize SampleResult 
	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	sampleResult.Init(Film::RADIANCE_PER_PIXEL_NORMALIZED, 1);

	// Initialize the max. path depth
	PathDepthInfo maxPathDepthInfo;
	maxPathDepthInfo.depth = params.maxPathDepth;
	maxPathDepthInfo.diffuseDepth = params.maxPathDepth;
	maxPathDepthInfo.glossyDepth = params.maxPathDepth;
	maxPathDepthInfo.specularDepth = params.maxPathDepth;

	for (u_int sampleIndex = 0; sampleIndex < params.workSize; ++sampleIndex) {
		sampleResult.radiance[0] = Spectrum();

		Ray eyeRay;
		PathVolumeInfo volInfo;
		float cameraDpDx, cameraDpDy;
		GenerateEyeRay(camera, eyeRay, volInfo, &sampler, sampleResult, cameraDpDx, cameraDpDy,
				params.imagePlaneRadius, params.timeStart, params.timeStart);

		if (!IsValid(cameraDpDx) || !IsValid(cameraDpDy))
			continue;

		BSDFEvent lastBSDFEvent = SPECULAR;
		Spectrum pathThroughput(1.f);
		PathDepthInfo depthInfo;
		BSDF bsdf;
		float pathLength = 0.f;
		for (;;) {
			sampleResult.firstPathVertex = (depthInfo.depth == 0);
			const u_int sampleOffset = sampleBootSize + depthInfo.depth * sampleStepSize;

			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			const bool hit = params.scene->Intersect(nullptr,
					EYE_RAY | (sampleResult.firstPathVertex ? CAMERA_RAY : GENERIC_RAY),
					&volInfo, sampler.GetSample(sampleOffset),
					&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput,
					&pathThroughput, &sampleResult);
			pathThroughput *= connectionThroughput;
			// Note: pass-through check is done inside Scene::Intersect()

			if (!hit) {
				// Nothing was hit, time to stop
				break;
			}

			//------------------------------------------------------------------
			// Something was hit
			//------------------------------------------------------------------
			
			// Update the current path length
			pathLength += eyeRayHit.t;

			if (!params.validator || params.validator->IsValid(bsdf)) {
				// Found a place for a valid cache entry

				params.accumulatedRadiusSize += Max(cameraDpDx * pathLength, cameraDpDy * pathLength);
				++params.radiusSizeCount;
				break;
			}

			// Check if I reached the max. depth
			sampleResult.lastPathVertex = depthInfo.IsLastPathVertex(maxPathDepthInfo, bsdf.GetEventTypes());
			if (sampleResult.lastPathVertex && !sampleResult.firstPathVertex)
				break;

			//------------------------------------------------------------------
			// Build the next vertex path ray
			//------------------------------------------------------------------

			Vector sampledDir;
			float cosSampledDir, lastPdfW;
			const Spectrum bsdfSample = bsdf.Sample(&sampledDir,
						sampler.GetSample(sampleOffset + 1),
						sampler.GetSample(sampleOffset + 2),
						&lastPdfW, &cosSampledDir, &lastBSDFEvent);

			assert (!bsdfSample.IsNaN() && !bsdfSample.IsInf());
			if (bsdfSample.Black())
				break;
			assert (!isnan(lastPdfW) && !isinf(lastPdfW));

			if (sampleResult.firstPathVertex)
				sampleResult.firstPathVertexEvent = lastBSDFEvent;

			// Increment path depth informations
			depthInfo.IncDepths(lastBSDFEvent);

			// Russian Roulette
			if (!(lastBSDFEvent & SPECULAR) && (depthInfo.GetRRDepth() >= rrDepth)) {
				const float rrProb = RenderEngine::RussianRouletteProb(bsdfSample, rrImportanceCap);
				if (rrProb < sampler.GetSample(sampleOffset + 3))
					break;

				// Increase path contribution
				pathThroughput /= rrProb;
			}

			pathThroughput *= bsdfSample;
			assert (!pathThroughput.IsNaN() && !pathThroughput.IsInf());

			// Update volume information
			volInfo.Update(lastBSDFEvent, bsdf);

			eyeRay.Update(bsdf.GetRayOrigin(sampledDir), sampledDir);
		}

		sampler.NextSample(sampleResults);

#ifdef WIN32
		// Work around Windows bad scheduling
		boost::this_thread::yield();
#endif
	}
}

float Film2SceneRadius(const Scene *scene, 
		const float imagePlaneRadius, const float defaultRadius,
		const u_int maxPathDepth, const float timeStart, const float timeEnd,
		const Film2SceneRadiusValidator *validator) {
	const size_t renderThreadCount = GetHardwareThreadCount();

	// Render 16 passes at 256 * 256 resolution
	const u_int workSize = 16 * 256 * 256 / renderThreadCount;

	vector<Film2SceneRadiusThreadParams> params(renderThreadCount);
	vector<boost::thread *> renderThreads(renderThreadCount);

	for (size_t i = 0; i < renderThreadCount; ++i) {
		params[i].threadIndex = i;
		params[i].workSize = workSize;
		params[i].scene = scene;
		params[i].imagePlaneRadius = imagePlaneRadius;
		params[i].maxPathDepth = maxPathDepth;
		params[i].timeStart = timeStart;
		params[i].timeEnd = timeEnd;
		params[i].validator = validator;

		renderThreads[i] = new boost::thread(&Film2SceneRadiusThread, boost::ref(params[i]));
	}

	float totalAccumulatedRadiusSize = 0.f;
	u_int totalRadiusSizeCount = 0;
	for (size_t i = 0; i < renderThreadCount; ++i) {
		renderThreads[i]->join();
		delete renderThreads[i];

		totalAccumulatedRadiusSize += params[i].accumulatedRadiusSize;
		totalRadiusSizeCount += params[i].radiusSizeCount;
	}

	// If I have enough samples, return the estimated best radius
	if (totalRadiusSizeCount > 256)
		return totalAccumulatedRadiusSize / totalRadiusSizeCount;
	else {
		// Otherwise use the default radius
		return defaultRadius;
	}
}

}
