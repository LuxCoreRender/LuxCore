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

#include <boost/format.hpp>

#include "luxrays/utils/thread.h"

#include "slg/samplers/sobol.h"
#include "slg/scene/scene.h"
#include "slg/imagemap/imagemapcache.h"
#include "slg/utils/pathdepthinfo.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Optimal resize preprocess rendering thread
//------------------------------------------------------------------------------

static void GenerateEyeRay(const Camera *camera, const UV &sampleOffestUV, Ray &eyeRay,
		PathVolumeInfo &volInfo, Sampler *sampler, SampleResult &sampleResult) {
	const u_int *subRegion = camera->filmSubRegion;
	// Removed the +1 from region width to have room for sampleOffestUV
	sampleResult.filmX = subRegion[0] + sampler->GetSample(0) * (subRegion[1] - subRegion[0]) +
			 sampleOffestUV.u;
	// Removed the +1 from region height to have room for sampleOffestUV
	sampleResult.filmY = subRegion[2] + sampler->GetSample(1) * (subRegion[3] - subRegion[2]) +
			 sampleOffestUV.v;
	
	//cout << "Sample: " << sampleResult.filmX << ", " << sampleResult.filmY << endl;

	const float timeSample = sampler->GetSample(4);
	const float time = camera->GenerateRayTime(timeSample);

	camera->GenerateRay(time, sampleResult.filmX, sampleResult.filmY, &eyeRay, &volInfo,
		sampler->GetSample(2), sampler->GetSample(3));
}

void ImageMapResizePolicy::RenderFunc(const u_int threadIndex,
		ImageMapCache *imc, const vector<u_int> *imgMapsIndices, u_int *workCounter,
		const Scene *scene, SobolSamplerSharedData *sobolSharedData,
		boost::barrier *threadsSyncBarrier) {
	// Hard coded parameters
	const u_int passesCount = 1;
	const u_int workSize = 4096;
	const float maxPathDepth = 4;
	const float glossinessThreshold = .05f;

	//--------------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------------

	// This is really used only by Windows for 64+ threads support
	SetThreadGroupAffinity(threadIndex);

	// Setup thread image maps instrumentation
	for (auto i : *imgMapsIndices)
		imc->maps[i]->instrumentationInfo->ThreadSetUp();
	
	threadsSyncBarrier->wait();

	const Camera *camera = scene->camera;

	// Initialize the sampler
	RandomGenerator rnd(1 + threadIndex);
	SobolSampler sampler(&rnd, NULL, NULL, true, 0.f, 0.f,
			16, 16, 1, 1,
			sobolSharedData);
	
	// Request the samples
	const u_int sampleBootSize = 5;
	const u_int sampleStepSize = 3;
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

	const u_int *subRegion = camera->filmSubRegion;
	const u_int totalWorkUnit = u_longlong(subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1) * passesCount / workSize;
	//cout << "totalWorkUnit: " << totalWorkUnit << endl;

	//GenericFrameBuffer<4, 1, float> frameBuffer(camera->filmWidth, camera->filmHeight);
	while (!boost::this_thread::interruption_requested() && (*workCounter < totalWorkUnit)) {
		AtomicInc(workCounter);
		
		for (u_int workUnitIndex = 0; workUnitIndex < workSize; ++workUnitIndex) {
			for (u_int i = 0; i < 3; ++i) {
				ImageMap::InstrumentationInfo::InstrumentationSampleIndex sampleIndex;
				UV sampleOffestUV;
				switch (i) {
					case 0:
						sampleIndex = ImageMap::InstrumentationInfo::BASE_INDEX;
						break;
					case 1:
						sampleIndex = ImageMap::InstrumentationInfo::OFFSET_U_INDEX;
						sampleOffestUV = UV (1.f, 0.f);
						break;
					case 2:
						sampleIndex = ImageMap::InstrumentationInfo::OFFSET_V_INDEX;
						sampleOffestUV = UV (0.f, 1.f);
						break;
				}

				for (auto j : *imgMapsIndices)
					imc->maps[j]->instrumentationInfo->ThreadSetSampleIndex(sampleIndex);

				sampleResult.radiance[0] = Spectrum();

				Ray eyeRay;
				PathVolumeInfo volInfo;
				GenerateEyeRay(camera, sampleOffestUV, eyeRay, volInfo, &sampler, sampleResult);

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
					} /*else {
						if (sampleResult.firstPathVertex)
							sampleResult.radiance[0] = bsdf.Albedo();
					}*/

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
					// Check if I have to continue with this path or not
					if (bsdfSample.Black() ||
							(!(lastBSDFEvent & SPECULAR) &&
							!((lastBSDFEvent & GLOSSY) && (bsdf.GetGlossiness() < glossinessThreshold)))
							)
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

				/*const u_int frameBufferX = floorf(sampleResult.filmX);
				const u_int frameBufferY = floorf(sampleResult.filmY);
				if ((frameBufferX >= 0) && (frameBufferX < frameBuffer.GetWidth()) &&
						(frameBufferY >= 0) && (frameBufferY < frameBuffer.GetHeight()))
					frameBuffer.AddWeightedPixel(frameBufferX, frameBufferY, sampleResult.radiance[0].c, 1.f);*/

#ifdef WIN32
				// Work around Windows bad scheduling
				boost::this_thread::yield();
#endif
			}

			// Accumulate image maps instrumentation data
			for (auto j : *imgMapsIndices)
				imc->maps[j]->instrumentationInfo->ThreadAccumulateSamples();

			sampler.NextSample(sampleResults);
		}
	}

	//frameBuffer.SaveHDR("thread" + ToString(threadIndex) + ".exr");
	
	threadsSyncBarrier->wait();

	// Delete thread image maps instrumentation
	for (auto i : *imgMapsIndices)
		imc->maps[i]->instrumentationInfo->ThreadFinalize();
}

//------------------------------------------------------------------------------
// ImageMapResizeMinMemPolicy::CalcOptimalImageMapSizes()
//------------------------------------------------------------------------------

void ImageMapResizePolicy::CalcOptimalImageMapSizes(ImageMapCache &imc, const Scene *scene,
		const vector<u_int> &imgMapsIndices) {
	// Do a test render to establish the optimal image maps sizes	
	const size_t renderThreadCount = GetHardwareThreadCount();
	vector<boost::thread *> renderThreads(renderThreadCount, nullptr);
	SLG_LOG("Optimal image map size preprocess thread count: " << renderThreadCount);

	boost::barrier threadsSyncBarrier(renderThreadCount);
	
	SobolSamplerSharedData sobolSharedData(131, nullptr);
	
	// Start the preprocessing threads
	u_int workCounter = 0;
	for (size_t i = 0; i < renderThreadCount; ++i)
		renderThreads[i] = new boost::thread(&RenderFunc, i, &imc, &imgMapsIndices,
				&workCounter, scene, &sobolSharedData, &threadsSyncBarrier);

	// Wait for the end of threads
	for (size_t i = 0; i < renderThreadCount; ++i) {
		renderThreads[i]->join();

		delete renderThreads[i];
	}

	for (auto i : imgMapsIndices) {
		const u_int originalWidth = imc.maps[i]->instrumentationInfo->originalWidth;
		const u_int originalHeigth = imc.maps[i]->instrumentationInfo->originalHeigth;

		u_int optimalWidth = imc.maps[i]->instrumentationInfo->optimalWidth;
		u_int optimalHeigth = imc.maps[i]->instrumentationInfo->optimalHeigth;

		SDL_LOG("Image maps \"" << imc.maps[i]->GetName() << "\" optimal resize: " <<
				originalWidth << "x" << originalHeigth << " => " <<
				optimalWidth << "x" << optimalHeigth);
	}
}
