/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/usings.h"
#include "luxrays/core/color/color.h"
#include "slg/usings.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/random.h"
#include "slg/utils/mortoncurve.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RandomSamplerSharedData
//------------------------------------------------------------------------------

RandomSamplerSharedData::RandomSamplerSharedData(FilmPtr engineFlm) :
	engineFilm(engineFlm)
{
	Reset();
}

void RandomSamplerSharedData::Reset() {
	bucketIndex = 0;
}

void RandomSamplerSharedData::GetNewBucket(const u_int bucketCount,
		u_int *newBucketIndex) {
	*newBucketIndex = AtomicInc(&bucketIndex) % bucketCount;
}

std::unique_ptr<SamplerSharedData> RandomSamplerSharedData::FromProperties(
	const Properties &cfg, const RandomGeneratorUPtr & rndGen, FilmPtr film
) {
	return std::make_unique<RandomSamplerSharedData>(film);
}

//------------------------------------------------------------------------------
// Random sampler
//------------------------------------------------------------------------------

RandomSampler::RandomSampler(const RandomGeneratorUPtr & rnd, FilmPtr flm,
		const FilmSampleSplatterUPtr& flmSplatter, const bool imgSamplesEnable,
		const float adaptiveStr, const float adaptiveUserImpWeight,
		const u_int bucketSz, const u_int tileSz, const u_int superSmpl,
		const u_int overlap,
		SamplerSharedDataSPtr samplerSharedData
) :
		Sampler(rnd, flm, flmSplatter, imgSamplesEnable),
		sharedData(static_pointer_cast<RandomSamplerSharedData>(samplerSharedData)),
		adaptiveStrength(adaptiveStr), adaptiveUserImportanceWeight(adaptiveUserImpWeight),
		bucketSize(bucketSz), tileSize(tileSz), superSampling(superSmpl), overlapping(overlap) {
}

void RandomSampler::InitNewSample() {
	const bool doImageSamples = (imageSamplesEnable && film);

	const u_int *filmSubRegion;
	u_int subRegionWidth, subRegionHeight, tiletWidthCount, tileHeightCount, bucketCount;

	if (doImageSamples) {
		filmSubRegion = GetFilm().GetSubRegion();

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
				sharedData->GetNewBucket(bucketCount,
						&bucketIndex);

				pixelOffset = 0;
				passOffset = 0;
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
			FilmConstRef film = sharedData->GetEngineFilm();
			if ((adaptiveStrength > 0.f) && GetFilm().HasChannel(Film::NOISE)) {
				// Pixels are sampled in accordance with how far from convergence they are
				const float noise = *(GetFilm().channel_NOISE->GetPixel(pixelX, pixelY));

				// Factor user driven importance sampling too
				float threshold;
				if (GetFilm().HasChannel(Film::USER_IMPORTANCE)) {
					const float userImportance = *(GetFilm().channel_USER_IMPORTANCE->GetPixel(pixelX, pixelY));

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
					// Skip this pixel and try the next one
					continue;
				}
			}
		} else {
			pixelX = 0;
			pixelY = 0;
		}

		sample0 = pixelX + rndGen->floatValue();
		sample1 = pixelY + rndGen->floatValue();
		break;
	}
}

void RandomSampler::RequestSamples(const SampleType smplType, const u_int size) {
	Sampler::RequestSamples(smplType, size);

	pixelOffset = bucketSize * bucketSize;
	passOffset = superSampling;

	InitNewSample();
}

float RandomSampler::GetSample(const u_int index) {
	assert (index < requestedSamples);

	switch (index) {
		case 0:
			return sample0;
		case 1:
			return sample1;
		default:
			return rndGen->floatValue();
	}
}

void RandomSampler::NextSample(const vector<SampleResult> &sampleResults) {
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
				throw runtime_error("Unknown sample type in RandomSampler::NextSample(): " + ToString(sampleType));
		}
		GetFilm().AddSampleCount(threadIndex, pixelNormalizedCount, screenNormalizedCount);

		AtomicAddSamplesToFilm(sampleResults);
	}

	InitNewSample();
}

PropertiesUPtr RandomSampler::ToProperties() const {
	auto props = std::make_unique<Properties>();
	*props << Sampler::ToProperties() <<
			Property("sampler.random.adaptive.strength")(adaptiveStrength) <<
			Property("sampler.random.adaptive.userimportanceweight")(adaptiveUserImportanceWeight) <<
			Property("sampler.random.bucketsize")(bucketSize) <<
			Property("sampler.random.tilesize")(tileSize) <<
			Property("sampler.random.supersampling")(superSampling) <<
			Property("sampler.random.overlapping")(overlapping);
	return props;
}

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

PropertiesUPtr RandomSampler::ToProperties(const Properties &cfg) {
	PropertiesUPtr props = std::make_unique<Properties>();
	*props <<
				cfg.Get(GetDefaultProps()->Get("sampler.type")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.imagesamples.enable")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.random.adaptive.strength")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.random.adaptive.userimportanceweight")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.random.bucketsize")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.random.tilesize")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.random.supersampling")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.random.overlapping"));
	return props;
}

SamplerUPtr RandomSampler::FromProperties(const Properties &cfg, const RandomGeneratorUPtr &  rndGen,
		FilmPtr film, const FilmSampleSplatterUPtr& flmSplatter,
		SamplerSharedDataSPtr sharedData) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps()->Get("sampler.imagesamples.enable")).Get<bool>();

	const float adaptiveStrength = Clamp(cfg.Get(GetDefaultProps()->Get("sampler.random.adaptive.strength")).Get<double>(), 0.0, .95);
	const float adaptiveUserImportanceWeight = cfg.Get(GetDefaultProps()->Get("sampler.random.adaptive.userimportanceweight")).Get<double>();
	const float bucketSize = RoundUpPow2(cfg.Get(GetDefaultProps()->Get("sampler.random.bucketsize")).Get<u_int>());
	const float tileSize = RoundUpPow2(cfg.Get(GetDefaultProps()->Get("sampler.random.tilesize")).Get<u_int>());
	const float superSampling = cfg.Get(GetDefaultProps()->Get("sampler.random.supersampling")).Get<u_int>();
	const float overlapping = cfg.Get(GetDefaultProps()->Get("sampler.random.overlapping")).Get<u_int>();

	return std::make_unique<RandomSampler>(rndGen, film, flmSplatter, imageSamplesEnable,
			adaptiveStrength, adaptiveUserImportanceWeight,
			bucketSize, tileSize, superSampling, overlapping,
			static_pointer_cast<RandomSamplerSharedData>(sharedData));
}

slg::ocl::Sampler *RandomSampler::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Sampler *oclSampler = new slg::ocl::Sampler();

	oclSampler->type = slg::ocl::RANDOM;
	oclSampler->random.adaptiveStrength = Clamp(cfg.Get(GetDefaultProps()->Get("sampler.random.adaptive.strength")).Get<double>(), 0.0, .95);
	oclSampler->random.adaptiveUserImportanceWeight = cfg.Get(GetDefaultProps()->Get("sampler.random.adaptive.userimportanceweight")).Get<double>();
	oclSampler->random.bucketSize = RoundUpPow2(cfg.Get(GetDefaultProps()->Get("sampler.random.bucketsize")).Get<u_int>());
	oclSampler->random.tileSize = RoundUpPow2(cfg.Get(GetDefaultProps()->Get("sampler.random.tilesize")).Get<u_int>());
	oclSampler->random.superSampling = cfg.Get(GetDefaultProps()->Get("sampler.random.supersampling")).Get<u_int>();
	oclSampler->random.overlapping = cfg.Get(GetDefaultProps()->Get("sampler.random.overlapping")).Get<u_int>();

	return oclSampler;
}

void RandomSampler::AddRequiredChannels(Film::FilmChannels &channels, const luxrays::Properties &cfg) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps()->Get("sampler.imagesamples.enable")).Get<bool>();

	const float str = cfg.Get(GetDefaultProps()->Get("sampler.random.adaptive.strength")).Get<double>();

	if (imageSamplesEnable && (str > 0.f))
		channels.insert(Film::NOISE);
}

PropertiesUPtr RandomSampler::GetDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag()) <<
			Property("sampler.random.adaptive.strength")(.95f) <<
			Property("sampler.random.adaptive.userimportanceweight")(.75f) <<
			Property("sampler.random.bucketsize")(16) <<
			Property("sampler.random.tilesize")(16) <<
			Property("sampler.random.supersampling")(1) <<
			Property("sampler.random.overlapping")(1);

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
