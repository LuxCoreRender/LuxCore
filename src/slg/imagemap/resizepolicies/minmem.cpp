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

#include<boost/format.hpp>

#include "luxrays/utils/thread.h"

#include "slg/samplers/sobol.h"
#include "slg/scene/scene.h"
#include "slg/imagemap/imagemapcache.h"
#include "slg/utils/pathdepthinfo.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMapResizeMinMemPolicy preprocess rendering thread
//------------------------------------------------------------------------------

//static void GenerateEyeRay(const Camera *camera, Ray &eyeRay,
//		PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) {
//	const u_int *subRegion = camera->filmSubRegion;
//	sampleResult.filmX = subRegion[0] + sampler->GetSample(0) * (subRegion[1] - subRegion[0] + 1);
//	sampleResult.filmY = subRegion[2] + sampler->GetSample(1) * (subRegion[3] - subRegion[2] + 1);
//
//	const float timeSample = sampler->GetSample(4);
//	const float time = camera->GenerateRayTime(timeSample);
//
//	camera->GenerateRay(time, sampleResult.filmX, sampleResult.filmY, &eyeRay, &volInfo,
//		sampler->GetSample(2), sampler->GetSample(3));
//}

static void RenderFunc(const u_int threadIndex, ImageMapResizeMinMemPolicy *mmp,
		const Scene *scene, SobolSamplerSharedData &sobolSharedData) {
	/*// Hard coded parameters
	const float maxPathDepth = 12;

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);

	const Camera *camera = scene->camera;

	// Initialize the sampler
	RandomGenerator rnd(1 + threadIndex);
	SobolSampler sampler(&rnd, NULL, NULL, true, 0.f, 0.f,
			16, 16, 1, 1,
			&sobolSharedData);
	
	// Request the samples
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 4;
	const u_int sampleSize = 
		sampleBootSize + // To generate eye ray
		maxPathDepth * sampleStepSize; // For each path vertex
	sampler.RequestSamples(PIXEL_NORMALIZED_ONLY, sampleSize);
	
	// Initialize SampleResult 
	vector<SampleResult> sampleResults(1);
	SampleResult &sampleResult = sampleResults[0];
	const Film::FilmChannels sampleResultsChannels({
		Film::RADIANCE_PER_PIXEL_NORMALIZED
	});
	sampleResult.Init(&sampleResultsChannels, 1);

	// Initialize the max. path depth
	PathDepthInfo maxPathDepthInfo;
	maxPathDepthInfo.depth = maxPathDepth;
	maxPathDepthInfo.diffuseDepth = maxPathDepth;
	maxPathDepthInfo.glossyDepth = maxPathDepth;
	maxPathDepthInfo.specularDepth = maxPathDepth;

	//--------------------------------------------------------------------------
	// Rendering
	//--------------------------------------------------------------------------
	
	while(!boost::this_thread::interruption_requested()) {
		sampleResult.radiance[0] = Spectrum();

		Ray eyeRay;
		PathVolumeInfo volInfo;
		GenerateEyeRay(camera, eyeRay, volInfo, &sampler, sampleResult);

		BSDFEvent lastBSDFEvent = SPECULAR;
		Spectrum pathThroughput(1.f);
		PathDepthInfo depthInfo;
		BSDF bsdf;
		for (;;) {
			sampleResult.firstPathVertex = (depthInfo.depth == 0);
			const u_int sampleOffset = sampleBootSize + depthInfo.depth * sampleStepSize;

			RayHit eyeRayHit;
			Spectrum connectionThroughput;
			const bool hit = scene->Intersect(NULL,
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

			//--------------------------------------------------------------
			// Something was hit
			//--------------------------------------------------------------

			// Check if I reached the max. depth
			sampleResult.lastPathVertex = depthInfo.IsLastPathVertex(maxPathDepthInfo, bsdf.GetEventTypes());
			if (sampleResult.lastPathVertex && !sampleResult.firstPathVertex)
				break;

			//--------------------------------------------------------------
			// Build the next vertex path ray
			//--------------------------------------------------------------

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
		break;
	}*/
}

//------------------------------------------------------------------------------
// ImageMapResizeMinMemPolicy::Preprocess()
//------------------------------------------------------------------------------

void ImageMapResizeMinMemPolicy::Preprocess(ImageMapCache &imc, const Scene *scene, const bool useRTMode) {
	if (useRTMode)
		return;

	SDL_LOG("Applying resize policy " << ImageMapResizePolicyType2String(GetType()) << "...");
	
	const double startTime = WallClockTime();
	
	// Build the list of image maps to check
	vector<u_int> imgMapsIndices;
	for (u_int i = 0; i < imc.resizePolicyToApply.size(); ++i) {
		if (imc.resizePolicyToApply[i]) {
			//SDL_LOG("Image map to process:  " << imc.maps[i]->GetName());
			imgMapsIndices.push_back(i);
		}
	}
	
	SDL_LOG("Image maps to process:  " << imgMapsIndices.size());

	// Enable instrumentation for image maps to check
	for (auto i : imgMapsIndices)
		imc.maps[i]->EnableInstrumentation();

	// Do a test render to establish the optimal image maps sizes	
	const size_t renderThreadCount = 1;//GetHardwareThreadCount();
	vector<boost::thread *> renderThreads(renderThreadCount, nullptr);
	SLG_LOG("Resize policy preprocess thread count: " << renderThreadCount);

	SobolSamplerSharedData sobolSharedData(131, nullptr);
	
	// Start the preprocessing threads
	for (size_t i = 0; i < renderThreadCount; ++i)
		renderThreads[i] = new boost::thread(&RenderFunc, i, this, scene, sobolSharedData);

	// Wait for the end of visibility particles tracing threads
	for (size_t i = 0; i < renderThreadCount; ++i) {
		renderThreads[i]->join();

		delete renderThreads[i];
	}
	
	// Delete instrumentation for image maps checked
	for (auto i : imgMapsIndices)
		imc.maps[i]->DeleteInstrumentation();

	SDL_LOG("Applying resize policy " << ImageMapResizePolicyType2String(GetType()) << " time: " <<
			(boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");
}
