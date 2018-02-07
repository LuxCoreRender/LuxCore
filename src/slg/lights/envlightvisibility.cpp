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

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>

#include <memory>

#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>

#include "luxrays/utils/atomic.h"
#include "slg/samplers/metropolis.h"
#include "slg/lights/envlightvisibility.h"
#include "slg/film/imagepipeline/plugins/gaussianblur3x3.h"

using namespace std;
using namespace luxrays;
using namespace slg;
OIIO_NAMESPACE_USING

//------------------------------------------------------------------------------
// Env. light visibility map builder
//------------------------------------------------------------------------------

EnvLightVisibility::EnvLightVisibility(const Scene *scn, const EnvLightSource *envl,
		ImageMap *li,
		const bool upperHemisphereOnly,
		const u_int w, const u_int h,
		const u_int sc, const u_int md) :
		scene(scn), envLight(envl), luminanceMapImage(li),
		sampleUpperHemisphereOnly(upperHemisphereOnly),
		width(w), height(h), sampleCount(sc), maxDepth(md) {
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

void EnvLightVisibility::ComputeVisibilityThread(const u_int threadIndex,
		MetropolisSamplerSharedData *sharedData, float *map) const {
	const u_int threadCount = boost::thread::hardware_concurrency();

	// Initialize the sampler
	RandomGenerator rnd(threadIndex + 1);
	MetropolisSampler sampler(&rnd, NULL, NULL, 512, .4f, .1f, sharedData);
	
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

	const u_int threadSampleCount = sampleCount / threadCount;
	for (u_int i = 0; i < threadSampleCount; ++i) {
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
				// Nothing was hit, I can see the env. light

				// I'm not interested in direct eye rays
				if (sampleResult.firstPathVertex)
					break;

				// The value is used by Metropolis
				sampleResult.radiance[0] += 1.f;

				// Update the map
				const UV envUV = envLight->GetEnvUV(-eyeRay.d);
				const u_int envX = Floor2UInt(envUV.u * width + .5f) % width;
				const u_int envY = Floor2UInt(envUV.v * height + .5f) % height;

				AtomicAdd(&map[envX + envY * width], 1.f);
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
		
		if (i % 8192 == 8191)
			samplesDone += 8192;
	}
}

void EnvLightVisibility::ComputeVisibility(float *map) const {
	SLG_LOG("Building visibility map of light source: " << envLight->GetName());

	const double t1 = WallClockTime();
	
	fill(&map[0], &map[width * height], 0.f);
	MetropolisSamplerSharedData sharedData;
	samplesDone = 0;

	const u_int threadCount = boost::thread::hardware_concurrency();
	vector<boost::thread *> threads(12, NULL);
	for (u_int i = 0; i < threadCount; ++i) {
		threads[i] = new boost::thread(&EnvLightVisibility::ComputeVisibilityThread,
				this, i, &sharedData, map);
	}
	
	for (u_int i = 0; i < threadCount; ++i) {
		for (;;) {
			if (threads[i]->try_join_for(boost::chrono::milliseconds(2000)))
				break;
		
			// Using double to avoid u_int overflow
			SLG_LOG("Visibility samples: " << samplesDone << "/" << sampleCount <<" (" << (u_int)((100.0 * samplesDone) / sampleCount) << "%)");
		}
	}

	const double t2 = WallClockTime();
	const double dt = t2 - t1;
	SLG_LOG(boost::str(boost::format("Visibility map done in %.2f secs with %d samples (%.2fM samples/sec)") %
				dt % sampleCount % (sampleCount / (dt * 1000000.0))));
}

Distribution2D *EnvLightVisibility::Build() const {
	// The initial visibility map is built at the same size of the infinite
	// light image map

	// Allocate the map storage
	ImageMap *visibilityMapImage = ImageMap::AllocImageMap<float>(1.f, 1,
			width, height, ImageMapStorage::REPEAT);
	float *visibilityMap = (float *)visibilityMapImage->GetStorage()->GetPixelsData();

	// Compute the visibility map
	ComputeVisibility(visibilityMap);

	// Filter the map
	const u_int mapPixelCount = width * height;
	vector<float> tmpBuffer(mapPixelCount);
	GaussianBlur3x3FilterPlugin::ApplyBlurFilter(width, height,
				&visibilityMap[0], &tmpBuffer[0],
				.5f, 1.f, .5f);

	// Check if I have set the lower hemisphere to 0.0
	if (sampleUpperHemisphereOnly) {
		for (u_int y = height / 2 + 1; y < height; ++y)
			for (u_int x = 0; x < width; ++x)
				visibilityMap[x + y * width] = 0.f;
	}

	// Normalize and multiply for normalized image luminance
	float visibilityMaxVal = 0.f;
	for (u_int i = 0; i < mapPixelCount; ++i)
		visibilityMaxVal = Max(visibilityMaxVal, visibilityMap[i]);

	if (visibilityMaxVal == 0.f) {
		// This is quite strange. In this case I will use the normal map
		SLG_LOG("WARNING: Visibility map is all black, reverting to importance sampling");

		delete visibilityMapImage;
		return NULL;
	}

	const ImageMapStorage *luminanceMapStorage = luminanceMapImage->GetStorage();

	// For some debug, save the map to a file
	/*{
		ImageSpec spec(width, height, 3, TypeDesc::FLOAT);
		ImageBuf buffer(spec);
		for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
			u_int x = it.x();
			u_int y = it.y();
			float *pixel = (float *)buffer.pixeladdr(x, y, 0);
			const float v = luminanceMapStorage->GetFloat(x + y * width);
			pixel[0] = v;
			pixel[1] = v;
			pixel[2] = v;
		}
		buffer.write("luminance.exr");
	}*/

	float luminanceMaxVal = 0.f;
	for (u_int i = 0; i < mapPixelCount; ++i)
		luminanceMaxVal = Max(visibilityMaxVal, luminanceMapStorage->GetFloat(i));

	const float invVisibilityMaxVal = 1.f / visibilityMaxVal;
	const float invLuminanceMaxVal = 1.f / luminanceMaxVal;
	for (u_int i = 0; i < mapPixelCount; ++i) {
		const float normalizedVisVal = visibilityMap[i] * invVisibilityMaxVal;
		const float normalizedLumiVal = luminanceMapStorage->GetFloat(i) * invLuminanceMaxVal;

		visibilityMap[i] = normalizedVisVal * normalizedLumiVal;
	}

	// For some debug, save the map to a file
	/*{
		ImageSpec spec(width, height, 3, TypeDesc::FLOAT);
		ImageBuf buffer(spec);
		for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
			u_int x = it.x();
			u_int y = it.y();
			float *pixel = (float *)buffer.pixeladdr(x, y, 0);
			const float v = visibilityMap[x + y * width];
			pixel[0] = v;
			pixel[1] = v;
			pixel[2] = v;
		}
		buffer.write("visibiliy.exr");
	}*/

	Distribution2D *dist = new Distribution2D(&visibilityMap[0], width, height);
	delete visibilityMapImage;

	return dist;
}
