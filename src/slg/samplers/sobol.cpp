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

#include <boost/lexical_cast.hpp>

#include "luxrays/core/color/color.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/sobol.h"
#include "slg/utils/mortoncurve.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SobolSamplerSharedData
//------------------------------------------------------------------------------

SobolSamplerSharedData::SobolSamplerSharedData(const u_int seed, Film *engineFlm) : SamplerSharedData() {
	engineFilm = engineFlm;
	seedBase = seed;
	
	Reset();
}

SobolSamplerSharedData::SobolSamplerSharedData(RandomGenerator *rndGen, Film *engineFlm) : SamplerSharedData() {
	engineFilm = engineFlm;
	seedBase = rndGen->uintValue() % (0xFFFFFFFFu - 1u) + 1u;

	Reset();
}

void SobolSamplerSharedData::Reset() {
	if (engineFilm) {
		const u_int *subRegion = engineFilm->GetSubRegion();
		const u_int filmRegionPixelCount = (subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1);
		
		// Initialize with SOBOL_STARTOFFSET the vector holding the passes per pixel
		passPerPixel.resize(filmRegionPixelCount, SOBOL_STARTOFFSET);
	} else
		passPerPixel.resize(1, SOBOL_STARTOFFSET);

	bucketIndex = 0;
}

void SobolSamplerSharedData::GetNewBucket(const u_int bucketCount,
		u_int *newBucketIndex, u_int *seed) {
	*newBucketIndex = AtomicInc(&bucketIndex) % bucketCount;

	*seed = (seedBase + *newBucketIndex) % (0xFFFFFFFFu - 1u) + 1u;
}

u_int SobolSamplerSharedData::GetNewPixelPass(const u_int pixelIndex) {
	// Iterate pass of this pixel
	return AtomicInc(&passPerPixel[pixelIndex]);
}

SamplerSharedData *SobolSamplerSharedData::FromProperties(const Properties &cfg,
		RandomGenerator *rndGen, Film *film) {
	return new SobolSamplerSharedData(rndGen, film);
}

//------------------------------------------------------------------------------
// Sobol sampler
//
// This sampler is based on Blender Cycles Sobol implementation.
//------------------------------------------------------------------------------

SobolSampler::SobolSampler(RandomGenerator *rnd, Film *flm,
		const FilmSampleSplatter *flmSplatter, const bool imgSamplesEnable,
		const float adaptiveStr, const float adaptiveUserImpWeight,
		const u_int bucketSz, const u_int tileSz, const u_int superSmpl,
		const u_int overlap, SobolSamplerSharedData *samplerSharedData) :
		Sampler(rnd, flm, flmSplatter, imgSamplesEnable),
		sharedData(samplerSharedData), sobolSequence(), adaptiveStrength(adaptiveStr),
		adaptiveUserImportanceWeight(adaptiveUserImpWeight), bucketSize(bucketSz),
		tileSize(tileSz), superSampling(superSmpl), overlapping(overlap) {
}

SobolSampler::~SobolSampler() {
}

void SobolSampler::InitNewSample() {
	const bool doImageSamples = (imageSamplesEnable && film);

	const u_int *filmSubRegion;
	u_int subRegionWidth, subRegionHeight, tiletWidthCount, tileHeightCount, bucketCount;

	if (doImageSamples) {
		filmSubRegion = film->GetSubRegion();

		subRegionWidth = filmSubRegion[1] - filmSubRegion[0] + 1;
		subRegionHeight = filmSubRegion[3] - filmSubRegion[2] + 1;

		tiletWidthCount = (subRegionWidth + tileSize - 1) / tileSize;
		tileHeightCount = (subRegionHeight + tileSize - 1) / tileSize;

		bucketCount = overlapping * (tiletWidthCount * tileSize * tileHeightCount * tileSize + bucketSize - 1) / bucketSize;
	} else
		bucketCount = 0xffffffffu;

	// Update pixelIndexOffset

	for (;;) {
		passOffset++;
		if (passOffset >= superSampling) {
			pixelOffset++;
			passOffset = 0;

			if (pixelOffset >= bucketSize) {
				// Ask for a new bucket
				u_int bucketSeed;
				sharedData->GetNewBucket(bucketCount,
						&bucketIndex, &bucketSeed);

				pixelOffset = 0;
				passOffset = 0;

				// Initialize the rng0, rng1 and rngPass generator
				rngGenerator.init(bucketSeed);
			}
		}

		// Initialize sample0 and sample 1

		u_int pixelX, pixelY;
		if (doImageSamples) {
			// Transform the bucket index in a pixel coordinate

			const u_int pixelBucketIndex = (bucketIndex / overlapping) * bucketSize + pixelOffset;
			const u_int mortonCurveOffset = pixelBucketIndex % (tileSize * tileSize);
			const u_int pixelTileIndex = pixelBucketIndex / (tileSize * tileSize);

			const u_int subRegionPixelX = (pixelTileIndex % tiletWidthCount) * tileSize + DecodeMorton2X(mortonCurveOffset);
			const u_int subRegionPixelY = (pixelTileIndex / tiletWidthCount) * tileSize + DecodeMorton2Y(mortonCurveOffset);
			if ((subRegionPixelX >= subRegionWidth) || (subRegionPixelY >= subRegionHeight)) {
				// Skip the pixels out of the film sub region
				continue;
			}

			pixelX = filmSubRegion[0] + subRegionPixelX;
			pixelY = filmSubRegion[2] + subRegionPixelY;

			// Check if the current pixel is over or under the convergence threshold
			const Film *film = sharedData->engineFilm;
			if ((adaptiveStrength > 0.f) && film->HasChannel(Film::NOISE)) {
				// Pixels are sampled in accordance with how far from convergence they are
				const float noise = *(film->channel_NOISE->GetPixel(pixelX, pixelY));

				// Factor user driven importance sampling too
				float threshold;
				if (film->HasChannel(Film::USER_IMPORTANCE)) {
					const float userImportance = *(film->channel_USER_IMPORTANCE->GetPixel(pixelX, pixelY));

					// Noise is initialized to INFINITY at start
					if (isinf(noise))
						threshold = userImportance;
					else
						threshold = (userImportance > 0.f) ? Lerp(adaptiveUserImportanceWeight, noise, userImportance) : 0.f;
				} else
					threshold = noise;

				// The floor for the pixel importance is given by the adaptiveness strength
				threshold = Max(threshold, 1.f - adaptiveStrength);

				if (rndGen->floatValue() > threshold) {

					// Workaround for preserving random number distribution behavior
					// rngGenerator.floatValue();
					// rngGenerator.floatValue();
					rngGenerator.uintValue();

					// Skip this pixel and try the next one
					continue;
				}
			}

			pass = sharedData->GetNewPixelPass(subRegionPixelX + subRegionPixelY * subRegionWidth);
		} else {
			pixelX = 0;
			pixelY = 0;

			pass = sharedData->GetNewPixelPass();
		}

		// Initialize rng0, rng1 and rngPass

		// sobolSequence.rng0 = rngGenerator.floatValue();
		// sobolSequence.rng1 = rngGenerator.floatValue();
		sobolSequence.rngPass = rngGenerator.uintValue();

		sample0 = pixelX + sobolSequence.GetSampleOwen(pass, 0);
		sample1 = pixelY + sobolSequence.GetSampleOwen(pass, 1);
		// sample0 = pixelX +  sobolSequence.GetSample(pass, 0);
		// sample1 = pixelY +  sobolSequence.GetSample(pass, 1);
		break;
	}
}


void SobolSampler::RequestSamples(const SampleType smplType, const u_int size) {
	Sampler::RequestSamples(smplType, size);

	// sobolSequence.RequestSamples(size);
	// sobolSequence.seeds.reserve(requestedSamples/4 + 1);
	// SLG_LOG(requestedSamples << " samples requested");
	pixelOffset = bucketSize * bucketSize;
	passOffset = superSampling;

	InitNewSample();
}

float SobolSampler::GetSample(const u_int index) {
	assert (index < requestedSamples);

	switch (index) {
		case 0:
			return sample0;
		case 1:
			return sample1;
		default:
			// return sobolSequence.GetSample(pass, index);
			return sobolSequence.GetSampleOwen(pass, index);
	}
}

void SobolSampler::NextSample(const vector<SampleResult> &sampleResults) {
	if (film) {
		double pixelNormalizedCount, screenNormalizedCount;
		switch (sampleType) {
			case PIXEL_NORMALIZED_ONLY:
				pixelNormalizedCount = 1.0;
				screenNormalizedCount = 0.0;
				break;
			case SCREEN_NORMALIZED_ONLY:
				pixelNormalizedCount = 0.0;
				screenNormalizedCount = 1.0;
				break;
			case PIXEL_NORMALIZED_AND_SCREEN_NORMALIZED:
				pixelNormalizedCount = 1.0;
				screenNormalizedCount = 1.0;
				break;
			default:
				throw runtime_error("Unknown sample type in SobolSampler::NextSample(): " + ToString(sampleType));
		}
		film->AddSampleCount(threadIndex, pixelNormalizedCount, screenNormalizedCount);

		AtomicAddSamplesToFilm(sampleResults);
	}

	InitNewSample();
}

Properties SobolSampler::ToProperties() const {
	return Sampler::ToProperties() <<
			Property("sampler.sobol.adaptive.strength")(adaptiveStrength) <<
			Property("sampler.sobol.adaptive.userimportanceweight")(adaptiveUserImportanceWeight) <<
			Property("sampler.sobol.bucketsize")(bucketSize) <<
			Property("sampler.sobol.tilesize")(tileSize) <<
			Property("sampler.sobol.supersampling")(superSampling) <<
			Property("sampler.sobol.overlapping")(overlapping);
}

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

Properties SobolSampler::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("sampler.type")) <<
			cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")) <<
			cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength")) <<
			cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.userimportanceweight")) <<
			cfg.Get(GetDefaultProps().Get("sampler.sobol.bucketsize")) <<
			cfg.Get(GetDefaultProps().Get("sampler.sobol.tilesize")) <<
			cfg.Get(GetDefaultProps().Get("sampler.sobol.supersampling")) <<
			cfg.Get(GetDefaultProps().Get("sampler.sobol.overlapping"));
}

Sampler *SobolSampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")).Get<bool>();

	const float adaptiveStrength = Clamp(cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength")).Get<float>(), 0.f, .95f);
	const float adaptiveUserImportanceWeight = cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.userimportanceweight")).Get<float>();
	const float bucketSize = RoundUpPow2(cfg.Get(GetDefaultProps().Get("sampler.sobol.bucketsize")).Get<u_int>());
	const float tileSize = RoundUpPow2(cfg.Get(GetDefaultProps().Get("sampler.sobol.tilesize")).Get<u_int>());
	const float superSampling = cfg.Get(GetDefaultProps().Get("sampler.sobol.supersampling")).Get<u_int>();
	const float overlapping = cfg.Get(GetDefaultProps().Get("sampler.sobol.overlapping")).Get<u_int>();

	return new SobolSampler(rndGen, film, flmSplatter, imageSamplesEnable,
			adaptiveStrength, adaptiveUserImportanceWeight,
			bucketSize, tileSize, superSampling, overlapping,
			(SobolSamplerSharedData *)sharedData);
}

slg::ocl::Sampler *SobolSampler::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Sampler *oclSampler = new slg::ocl::Sampler();

	oclSampler->type = slg::ocl::SOBOL;
	oclSampler->sobol.adaptiveStrength = Clamp(cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength")).Get<float>(), 0.f, .95f);
	oclSampler->sobol.adaptiveUserImportanceWeight = cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.userimportanceweight")).Get<float>();
	oclSampler->sobol.bucketSize = RoundUpPow2(cfg.Get(GetDefaultProps().Get("sampler.sobol.bucketsize")).Get<u_int>());
	oclSampler->sobol.tileSize = RoundUpPow2(cfg.Get(GetDefaultProps().Get("sampler.sobol.tilesize")).Get<u_int>());
	oclSampler->sobol.superSampling = cfg.Get(GetDefaultProps().Get("sampler.sobol.supersampling")).Get<u_int>();
	oclSampler->sobol.overlapping = cfg.Get(GetDefaultProps().Get("sampler.sobol.overlapping")).Get<u_int>();

	return oclSampler;
}

void SobolSampler::AddRequiredChannels(Film::FilmChannels &channels, const luxrays::Properties &cfg) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")).Get<bool>();

	const float str = cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength")).Get<float>();

	if (imageSamplesEnable && (str > 0.f))
		channels.insert(Film::NOISE);
}

const Properties &SobolSampler::GetDefaultProps() {
	static Properties props = Properties() <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag()) <<
			Property("sampler.sobol.adaptive.strength")(.95f) <<
			Property("sampler.sobol.adaptive.userimportanceweight")(.75f) <<
			Property("sampler.sobol.bucketsize")(16) <<
			Property("sampler.sobol.tilesize")(16) <<
			Property("sampler.sobol.supersampling")(1) <<
			Property("sampler.sobol.overlapping")(1);

	return props;
}
