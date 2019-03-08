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

#include <vector>

#include "slg/engines/renderengine.h"
#include "slg/engines/caches/photongi/photongicache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// EvaluateBestRadius
//------------------------------------------------------------------------------

// The percentage of image plane to cover with the radius
const float imagePlaneDelta = .02f;

static void GenerateEyeRay(const Camera *camera, Ray &eyeRay,
		PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult,
		float &dpdx, float &dpdy) {
	const u_int *subRegion = camera->filmSubRegion;

	// Evaluate the delta x/y over the image plane in pixel

	const float imagePlaneDeltaX = camera->filmWidth * imagePlaneDelta;
	const float imagePlaneDeltaY = camera->filmHeight * imagePlaneDelta;

	// Evaluate the camera ray

	sampleResult.filmX = subRegion[0] + sampler->GetSample(0) * (subRegion[1] - subRegion[0] + 1);
	sampleResult.filmY = subRegion[2] + sampler->GetSample(1) * (subRegion[3] - subRegion[2] + 1);

	camera->GenerateRay(sampleResult.filmX, sampleResult.filmY, &eyeRay, &volInfo,
		sampler->GetSample(2), sampler->GetSample(3), sampler->GetSample(4));
	
	// Evaluate the camera ray + imagePlaneDeltaX

	Ray eyeRayDeltaX;
	PathVolumeInfo volInfoDeltaX;
	camera->GenerateRay(sampleResult.filmX + imagePlaneDeltaX, sampleResult.filmY, &eyeRayDeltaX, &volInfoDeltaX,
			sampler->GetSample(2), sampler->GetSample(3), sampler->GetSample(4));
	
	// Evaluate the camera ray + imagePlaneDeltaY

	Ray eyeRayDeltaY;
	PathVolumeInfo volInfoDeltaY;
	camera->GenerateRay(sampleResult.filmX, sampleResult.filmY + imagePlaneDeltaY, &eyeRayDeltaY, &volInfoDeltaY,
			sampler->GetSample(2), sampler->GetSample(3), sampler->GetSample(4));

	// I'm lacking the support for true ray differentials in camera object
	// interface so I resort to this simple method 
	
	const float t0 = 0.f;
	const float t1 = 1.f;

	// Evaluate dpdx
	dpdx = fabs(Distance(eyeRay(t1), eyeRayDeltaX(t1)) - Distance(eyeRay(t0), eyeRayDeltaX(t0)));
	// Evaluate dpdy
	dpdy = fabs(Distance(eyeRay(t1), eyeRayDeltaY(t1)) - Distance(eyeRay(t0), eyeRayDeltaY(t0)));
}

void PhotonGICache::EvaluateBestRadiusImpl(const u_int threadIndex, const u_int workSize,
		float &accumulatedRadiusSize, u_int &radiusSizeCount) const {
	// Hard coded RR parameters
	const u_int rrDepth = 3;
	const float rrImportanceCap = .5f;

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	const Camera *camera = scene->camera;

	// Initialize the sampler
	RandomGenerator rnd(1 + threadIndex);
	SobolSamplerSharedData sobolSharedData(131, nullptr);
	SobolSampler sampler(&rnd, NULL, NULL, 0.f, &sobolSharedData);
	
	// Request the samples
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 1;
	const u_int sampleSize = 
		sampleBootSize + // To generate eye ray
		params.photon.maxPathDepth * sampleStepSize; // For each path vertex

	sampler.RequestSamples(sampleSize);
		// Initialize SampleResult 
	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	sampleResult.Init(Film::RADIANCE_PER_PIXEL_NORMALIZED, 1);

	// Initialize the max. path depth
	PathDepthInfo maxPathDepthInfo;
	maxPathDepthInfo.depth = params.photon.maxPathDepth;
	maxPathDepthInfo.diffuseDepth = params.photon.maxPathDepth;
	maxPathDepthInfo.glossyDepth = params.photon.maxPathDepth;
	maxPathDepthInfo.specularDepth = params.photon.maxPathDepth;

	for (u_int sampleIndex = 0; sampleIndex < workSize; ++sampleIndex) {
		sampleResult.radiance[0] = Spectrum();

		Ray eyeRay;
		PathVolumeInfo volInfo;
		float cameraDpDx, cameraDpDy;
		GenerateEyeRay(camera, eyeRay, volInfo, &sampler, sampleResult, cameraDpDx, cameraDpDy);

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
			const bool hit = scene->Intersect(NULL, false, sampleResult.firstPathVertex,
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
			
			// Check if I have to flip the normal
			const Normal surfaceNormal = ((Dot(bsdf.hitPoint.geometryN, -eyeRay.d) > 0.f) ?
				1.f : -1.f) * bsdf.hitPoint.geometryN;

			// Update the current path length
			pathLength += eyeRayHit.t;

			if (IsPhotonGIEnabled(bsdf)) {
				// Found a place for a valid cache entry

				accumulatedRadiusSize += Max(cameraDpDx * pathLength, cameraDpDy * pathLength);
				++radiusSizeCount;
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
			sampleResult.passThroughPath = false;

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

			eyeRay.Update(bsdf.hitPoint.p, surfaceNormal, sampledDir);
		}

		sampler.NextSample(sampleResults);

#ifdef WIN32
		// Work around Windows bad scheduling
		boost::this_thread::yield();
#endif
	}
}

float PhotonGICache::EvaluateBestRadius() {
	SLG_LOG("PhotonGI evaluating best radius");

	const size_t renderThreadCount = boost::thread::hardware_concurrency();

	// Render 16 passes at 256 * 256 resolution
	const u_int workSize = 16 * 256 * 256 / renderThreadCount;

	vector<float> accumulatedRadiusSize(renderThreadCount, 0.f);
	vector<u_int> radiusSizeCount(renderThreadCount, 0.f);
	vector<boost::thread *> renderThreads(renderThreadCount);

	for (size_t i = 0; i < renderThreadCount; ++i) {
		renderThreads[i] = new boost::thread(&PhotonGICache::EvaluateBestRadiusImpl,
				this, i, workSize, boost::ref(accumulatedRadiusSize[i]), boost::ref(radiusSizeCount[i]));
	}

	float totalAccumulatedRadiusSize = 0.f;
	u_int totalRadiusSizeCount = 0;
	for (size_t i = 0; i < renderThreadCount; ++i) {
		renderThreads[i]->join();
		delete renderThreads[i];

		totalAccumulatedRadiusSize += accumulatedRadiusSize[i];
		totalRadiusSizeCount += radiusSizeCount[i];
	}

	SLG_LOG("PhotonGI evaluated values: " << totalRadiusSizeCount);

	// If I have enough samples, return the estimated best radius
	if (totalRadiusSizeCount > 256)
		return totalAccumulatedRadiusSize / totalRadiusSizeCount;
	else
		return GetDefaultProps().Get("path.photongi.indirect.lookup.radius").Get<float>();
}
