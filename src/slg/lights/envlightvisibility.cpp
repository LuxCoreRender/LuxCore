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

#include <memory>
#include <boost/format.hpp>

#include "slg/samplers/metropolis.h"
#include "slg/lights/envlightvisibility.h"
#include "luxrays/utils/atomic.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Env. light visibility preprocessor
//------------------------------------------------------------------------------

EnvLightVisibility::EnvLightVisibility(const Scene *scn, const EnvLightSource *envl) :
		scene(scn), envLight(envl) {
}

EnvLightVisibility::~EnvLightVisibility() {
}

void EnvLightVisibility::GenerateEyeRay(const Camera *camera, Ray &eyeRay,
		PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) const {
	const u_int *subRegion = camera->filmSubRegion;
	sampleResult.filmX = subRegion[0] + sampler->GetSample(0) * (subRegion[1] - subRegion[0] + 1);
	sampleResult.filmY = subRegion[2] + sampler->GetSample(1) * (subRegion[3] - subRegion[2] + 1);

	camera->GenerateRay(sampleResult.filmX, sampleResult.filmY, &eyeRay, &volInfo,
		sampler->GetSample(2), sampler->GetSample(3), sampler->GetSample(4));
}

float *EnvLightVisibility::ComputeVisibility(const u_int width, const u_int height,
		const u_int sampleCount, const u_int maxDepth) const {
	SLG_LOG("Building visibility map of light source: " << envLight->GetName());

	const double t1 = WallClockTime();
	
	auto_ptr<float> map(new float[width * height]);
	fill(map.get(), map.get() + width * height, 0.f);

	// Initialize the sampler
	RandomGenerator rnd(131);
	MetropolisSamplerSharedData sharedData;
	MetropolisSampler sampler(&rnd, NULL, NULL, 512, .4f, .1f, &sharedData);
	
	// Request the samples
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 3;
	const u_int sampleSize = 
		sampleBootSize + // To generate eye ray
		maxDepth * sampleStepSize; // For each path vertex
	sampler.RequestSamples(sampleSize);

	// Initialize SampleResult 
	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	sampleResult.Init(Film::RADIANCE_PER_PIXEL_NORMALIZED, 1);

	// Initialize the max. path depth
	PathDepthInfo maxPathDepth;
	maxPathDepth.depth = maxDepth;
	maxPathDepth.diffuseDepth = maxDepth;
	maxPathDepth.glossyDepth = maxDepth;
	maxPathDepth.specularDepth = maxDepth;

	for (u_int i = 0; i < sampleCount; ++i) {
		sampleResult.radiance[0] = Spectrum();
		
		Ray eyeRay;
		PathVolumeInfo volInfo;
		GenerateEyeRay(scene->camera, eyeRay, volInfo, &sampler, sampleResult);

		BSDFEvent lastBSDFEvent = SPECULAR;
		Spectrum pathThroughput(1.f);
		PathDepthInfo depthInfo;
		BSDF bsdf;
		for (;;) {
			sampleResult.firstPathVertex = (depthInfo.depth == 0);
			const u_int sampleOffset = sampleBootSize + depthInfo.depth * sampleStepSize;

			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			const bool hit = scene->Intersect(NULL, false,
					&volInfo, sampler.GetSample(sampleOffset),
					&eyeRay, &eyeRayHit, &bsdf, &connectionThroughput,
					&pathThroughput, &sampleResult);
			pathThroughput *= connectionThroughput;
			// Note: pass-through check is done inside Scene::Intersect()

			if (!hit) {
				if (sampleResult.firstPathVertex)
					break;

				// Nothing was hit, I can see the env. light
				const Spectrum envRadiance = envLight->GetRadiance(*scene, -eyeRay.d, NULL);
				const float visibleLuminance = Spectrum(pathThroughput * envRadiance).Y();

				// The value is used by Metropolis
				sampleResult.radiance[0] += visibleLuminance;

				// Update the map
				const UV envUV = envLight->GetEnvUV(-eyeRay.d);
				const u_int envX = Floor2UInt(envUV.u * width + .5f) % width;
				const u_int envY = Floor2UInt(envUV.v * height + .5f) % height;

				float *ptr = map.get() + envX + envY * width;
				*ptr += visibleLuminance;
				break;
			}

			//------------------------------------------------------------------
			// Something was hit
			//------------------------------------------------------------------

			// Check if I reached the max. depth
			sampleResult.lastPathVertex = depthInfo.IsLastPathVertex(maxPathDepth, bsdf.GetEventTypes());
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

			pathThroughput *= bsdfSample;
			assert (!pathThroughput.IsNaN() && !pathThroughput.IsInf());

			// Update volume information
			volInfo.Update(lastBSDFEvent, bsdf);

			eyeRay.Update(bsdf.hitPoint.p, sampledDir);
		}
		
		sampler.NextSample(sampleResults);
	}

	const double t2 = WallClockTime();
	const double dt = t2 - t1;
	SLG_LOG(boost::str(boost::format("Visibility map done in %.2f secs with %d samples (%.2fM samples/sec)") %
				dt % sampleCount % (sampleCount / (dt * 1000000.0))));

	return map.release();
}
