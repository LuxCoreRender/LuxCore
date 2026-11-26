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
#include <memory>

#include "luxrays/core/color/color.h"
#include "luxrays/usings.h"
#include "slg/usings.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/sobol.h"
#include "slg/utils/mortoncurve.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SobolSamplerSharedData
//------------------------------------------------------------------------------

SobolSamplerSharedData::SobolSamplerSharedData(
	const u_int seed,
	FilmPtr engineFlm
) :
	SamplerSharedData(),
	engineFilm(engineFlm),
	seedBase(std::make_shared<u_int>(seed))
{
	Reset();
}

SobolSamplerSharedData::SobolSamplerSharedData(
	const RandomGeneratorUPtr & rndGen,
	FilmPtr engineFlm
) :
	SamplerSharedData(),
	engineFilm(engineFlm),
	seedBase(std::make_shared<u_int>(rndGen->uintValue() % (0xFFFFFFFFu - 1u) + 1u))
{
	Reset();
}

void SobolSamplerSharedData::Reset() {
	if (HasEngineFilm()) {
		const u_int *subRegion = GetEngineFilm().GetSubRegion();
		const u_int filmRegionPixelCount = (subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1);

		// Initialize with SOBOL_STARTOFFSET the vector holding the passes per pixel
		passPerPixel.resize(filmRegionPixelCount, SOBOL_STARTOFFSET);
	} else
		passPerPixel.resize(1, SOBOL_STARTOFFSET);

	bucketIndex = std::make_shared<u_int>(0);
}

std::tuple<u_int, u_int> SobolSamplerSharedData::GetNewBucket(u_int bucketCount) {
	u_int newBucketIndex = AtomicInc(bucketIndex.get()) % bucketCount;

	u_int seed = (*seedBase + newBucketIndex) % (0xFFFFFFFFu - 1u) + 1u;

	return std::make_tuple(newBucketIndex, seed);
}

u_int SobolSamplerSharedData::GetNewPixelPass(const u_int pixelIndex) {
	// Iterate pass of this pixel
	return AtomicInc(&passPerPixel[pixelIndex]);
}

u_int SobolSamplerSharedData::GetPassCount(const u_int bucketCount) const {
	return *bucketIndex / bucketCount;
}

std::unique_ptr<SamplerSharedData> SobolSamplerSharedData::FromProperties(const Properties &cfg,
		const RandomGeneratorUPtr& rndGen, FilmPtr film) {
	return std::make_unique<SobolSamplerSharedData>(rndGen, film);
}

//------------------------------------------------------------------------------
// Sobol sampler
//
// This sampler is based on Blender Cycles Sobol implementation.
//------------------------------------------------------------------------------

SobolSampler::SobolSampler(
	const RandomGeneratorUPtr & rnd,
	FilmPtr flm,  // Film is optional!
	const FilmSampleSplatterUPtr& flmSplatter,
	const bool imgSamplesEnable,
	const float adaptiveStr,
	const float adaptiveUserImpWeight,
	const u_int bucketSz,
	const u_int tileSz,
	const u_int superSmpl,
	const u_int overlap,
	SamplerSharedDataSPtr samplerSharedData
) :
	Sampler(rnd, flm, flmSplatter, imgSamplesEnable),
	sharedData(static_pointer_cast<SobolSamplerSharedData>(samplerSharedData)),
	sobolSequence(),
	adaptiveStrength(adaptiveStr),
	adaptiveUserImportanceWeight(adaptiveUserImpWeight),
	bucketSize(bucketSz),
	tileSize(tileSz),
	superSampling(superSmpl),
	overlapping(overlap),
	bucketIndex(std::make_shared<u_int>(0))
{}
SobolSampler::SobolSampler(
	const RandomGeneratorUPtr & rnd,
	FilmRef flm,
	const FilmSampleSplatterUPtr& flmSplatter,
	const bool imgSamplesEnable,
	const float adaptiveStr,
	const float adaptiveUserImpWeight,
	const u_int bucketSz,
	const u_int tileSz,
	const u_int superSmpl,
	const u_int overlap,
	SamplerSharedDataSPtr samplerSharedData
) :
	Sampler(rnd, FilmPtr(std::addressof(flm)), flmSplatter, imgSamplesEnable),
	sharedData(static_pointer_cast<SobolSamplerSharedData>(samplerSharedData)),
	sobolSequence(),
	adaptiveStrength(adaptiveStr),
	adaptiveUserImportanceWeight(adaptiveUserImpWeight),
	bucketSize(bucketSz),
	tileSize(tileSz),
	superSampling(superSmpl),
	overlapping(overlap),
	bucketIndex(std::make_shared<u_int>(0))
{}

SobolSampler::~SobolSampler() {
}

void SobolSampler::InitNewSample() {
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
				auto [newBucketIndex, newBucketSeed] = sharedData->GetNewBucket(bucketCount);
				*bucketIndex = newBucketIndex;

				pixelOffset = 0;
				passOffset = 0;

				// Initialize the rng0, rng1 and rngPass generator
				rngGenerator.init(newBucketSeed);
			}
		}

		// Initialize sample0 and sample 1

		u_int pixelX, pixelY;
		if (doImageSamples) {
			// Transform the bucket index in a pixel coordinate

			const u_int pixelBucketIndex = (*bucketIndex / overlapping) * bucketSize + pixelOffset;
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
			auto& film = sharedData->GetEngineFilm();
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

					// Workaround for preserving random number distribution behavior
					rngGenerator.floatValue();
					rngGenerator.floatValue();
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

		sobolSequence.rng0 = rngGenerator.floatValue();
		sobolSequence.rng1 = rngGenerator.floatValue();
		sobolSequence.rngPass = rngGenerator.uintValue();

		sample0 = pixelX +  sobolSequence.GetSample(pass, 0);
		sample1 = pixelY +  sobolSequence.GetSample(pass, 1);
		break;
	}
}

void SobolSampler::RequestSamples(const SampleType smplType, const u_int size) {
	Sampler::RequestSamples(smplType, size);

	sobolSequence.RequestSamples(size);

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
			return sobolSequence.GetSample(pass, index);
	}
}

void SobolSampler::NextSample(const vector<SampleResult> &sampleResults) {
	if (film) {
		switch (sampleType) {
			case PIXEL_NORMALIZED_ONLY:
				GetFilm().AddSampleCount(threadIndex, 1.0, 0.0);
				break;
			case SCREEN_NORMALIZED_ONLY:
				GetFilm().AddSampleCount(threadIndex, 0.0, 1.0);
				break;
			case PIXEL_NORMALIZED_AND_SCREEN_NORMALIZED:
				GetFilm().AddSampleCount(threadIndex, 1.0, 1.0);
				break;
			case ONLY_AOV_SAMPLE:
				break;
			default:
				throw runtime_error("Unknown sample type in SobolSampler::NextSample(): " + ToString(sampleType));
		}

		AtomicAddSamplesToFilm(sampleResults);
	}

	InitNewSample();
}

u_int SobolSampler::GetPassCount() const {
	const bool doImageSamples = (imageSamplesEnable && film);
	if (!doImageSamples)
		throw runtime_error("Called SobolSampler::GetPassCount() without sampling an image");
	
	const u_int *filmSubRegion = GetFilm().GetSubRegion();

	const u_int subRegionWidth = filmSubRegion[1] - filmSubRegion[0] + 1;
	const u_int subRegionHeight = filmSubRegion[3] - filmSubRegion[2] + 1;

	const u_int tiletWidthCount = (subRegionWidth + tileSize - 1) / tileSize;
	const u_int tileHeightCount = (subRegionHeight + tileSize - 1) / tileSize;

	const u_int bucketCount = overlapping * (tiletWidthCount * tileSize * tileHeightCount * tileSize + bucketSize - 1) / bucketSize;

	return sharedData->GetPassCount(bucketCount);
}

PropertiesUPtr SobolSampler::ToProperties() const {
	auto props_ptr = std::make_unique<Properties>();
	auto& props = *props_ptr;
	props << Sampler::ToProperties() <<
			Property("sampler.sobol.adaptive.strength")(adaptiveStrength) <<
			Property("sampler.sobol.adaptive.userimportanceweight")(adaptiveUserImportanceWeight) <<
			Property("sampler.sobol.bucketsize")(bucketSize) <<
			Property("sampler.sobol.tilesize")(tileSize) <<
			Property("sampler.sobol.supersampling")(superSampling) <<
			Property("sampler.sobol.overlapping")(overlapping);
	return props_ptr;
}

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

PropertiesUPtr SobolSampler::ToProperties(const Properties &cfg) {
	PropertiesUPtr props = std::make_unique<Properties>();
	*props <<
				cfg.Get(GetDefaultProps()->Get("sampler.type")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.imagesamples.enable")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.sobol.adaptive.strength")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.sobol.adaptive.userimportanceweight")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.sobol.bucketsize")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.sobol.tilesize")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.sobol.supersampling")) <<
			cfg.Get(GetDefaultProps()->Get("sampler.sobol.overlapping"));
	return props;
}

SamplerUPtr SobolSampler::FromProperties(const Properties &cfg, const RandomGeneratorUPtr & rndGen,
		FilmPtr film, const FilmSampleSplatterUPtr& flmSplatter,
		SamplerSharedDataSPtr sharedData
) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps()->Get("sampler.imagesamples.enable")).Get<bool>();

	const float adaptiveStrength = Clamp(cfg.Get(GetDefaultProps()->Get("sampler.sobol.adaptive.strength")).Get<double>(), 0.0, .95);
	const float adaptiveUserImportanceWeight = cfg.Get(GetDefaultProps()->Get("sampler.sobol.adaptive.userimportanceweight")).Get<double>();
	const float bucketSize = RoundUpPow2(cfg.Get(GetDefaultProps()->Get("sampler.sobol.bucketsize")).Get<u_int>());
	const float tileSize = RoundUpPow2(cfg.Get(GetDefaultProps()->Get("sampler.sobol.tilesize")).Get<u_int>());
	const float superSampling = cfg.Get(GetDefaultProps()->Get("sampler.sobol.supersampling")).Get<u_int>();
	const float overlapping = cfg.Get(GetDefaultProps()->Get("sampler.sobol.overlapping")).Get<u_int>();

	return std::make_unique<SobolSampler>(rndGen, film, flmSplatter, imageSamplesEnable,
			adaptiveStrength, adaptiveUserImportanceWeight,
			bucketSize, tileSize, superSampling, overlapping,
			dynamic_pointer_cast<SobolSamplerSharedData>(sharedData)
	);
}

slg::ocl::Sampler *SobolSampler::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Sampler *oclSampler = new slg::ocl::Sampler();

	oclSampler->type = slg::ocl::SOBOL;
	oclSampler->sobol.adaptiveStrength = Clamp(cfg.Get(GetDefaultProps()->Get("sampler.sobol.adaptive.strength")).Get<double>(), 0.0, .95);
	oclSampler->sobol.adaptiveUserImportanceWeight = cfg.Get(GetDefaultProps()->Get("sampler.sobol.adaptive.userimportanceweight")).Get<double>();
	oclSampler->sobol.bucketSize = RoundUpPow2(cfg.Get(GetDefaultProps()->Get("sampler.sobol.bucketsize")).Get<u_int>());
	oclSampler->sobol.tileSize = RoundUpPow2(cfg.Get(GetDefaultProps()->Get("sampler.sobol.tilesize")).Get<u_int>());
	oclSampler->sobol.superSampling = cfg.Get(GetDefaultProps()->Get("sampler.sobol.supersampling")).Get<u_int>();
	oclSampler->sobol.overlapping = cfg.Get(GetDefaultProps()->Get("sampler.sobol.overlapping")).Get<u_int>();

	return oclSampler;
}

void SobolSampler::AddRequiredChannels(Film::FilmChannels &channels, const luxrays::Properties &cfg) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps()->Get("sampler.imagesamples.enable")).Get<bool>();

	const float str = cfg.Get(GetDefaultProps()->Get("sampler.sobol.adaptive.strength")).Get<double>();

	if (imageSamplesEnable && (str > 0.f))
		channels.insert(Film::NOISE);
}

PropertiesUPtr SobolSampler::GetDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
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
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
