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

#include <boost/lexical_cast.hpp>

#include "luxrays/core/color/color.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/pmj02.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PMJ02SamplerSharedData
//------------------------------------------------------------------------------


PMJ02SamplerSharedData::PMJ02SamplerSharedData(const u_int seed, Film *engineFlm) : SamplerSharedData() {
	Init(seed, engineFlm);
}

PMJ02SamplerSharedData::PMJ02SamplerSharedData(RandomGenerator *rndGen, Film *engineFlm) : SamplerSharedData() {
	Init(rndGen->uintValue() % (0xFFFFFFFFu - 1u) + 1u, engineFlm);
}

void PMJ02SamplerSharedData::Init(const u_int seed, Film *engineFlm) {
	engineFilm = engineFlm;
	seedBase = seed;

	if (engineFilm) {
		const u_int *subRegion = engineFilm->GetSubRegion();
		filmRegionPixelCount = (subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1);
		
		// All passes start at 0
		passPerPixel.resize(filmRegionPixelCount, 0);
	} else {
		filmRegionPixelCount = 0;
		passPerPixel.resize(1, 0);
	}

	pixelIndex = 0;
}

void PMJ02SamplerSharedData::GetNewPixelIndex(u_int &index, u_int &seed) {
	SpinLocker spinLocker(spinLock);

	index = pixelIndex;
	seed = (seedBase + pixelIndex) % (0xFFFFFFFFu - 1u) + 1u;
	
	pixelIndex += PMJ02_THREAD_WORK_SIZE;
	if (pixelIndex >= filmRegionPixelCount)
		pixelIndex = 0;
}

u_int PMJ02SamplerSharedData::GetNewPixelPass(const u_int pixelIndex) {
	// Iterate pass of this pixel
	return AtomicInc(&passPerPixel[pixelIndex]);
}

SamplerSharedData *PMJ02SamplerSharedData::FromProperties(const Properties &cfg,
		RandomGenerator *rndGen, Film *film) {
	return new PMJ02SamplerSharedData(rndGen, film);
}

//------------------------------------------------------------------------------
// PMJ02 sampler
//------------------------------------------------------------------------------

PMJ02Sampler::PMJ02Sampler(luxrays::RandomGenerator *rnd, Film *flm,
			const FilmSampleSplatter *flmSplatter, const bool imgSamplesEnable,
			const float adaptiveStr,
			PMJ02SamplerSharedData *samplerSharedData) :
		Sampler(rnd, flm, flmSplatter, imgSamplesEnable), sharedData(samplerSharedData),
		pmj02sequence(rnd), adaptiveStrength(adaptiveStr) {
}

u_int cmj_hash_simple(u_int i, u_int p) {
	i = (i ^ 61) ^ p;
	i += i << 3;
	i ^= i >> 4;
	i *= 0x27d4eb2d;
	return i;
}

void PMJ02Sampler::InitNewSample() {
	for (;;) {
		// Update pixelIndexOffset

		pixelIndexOffset++;
		if (pixelIndexOffset >= PMJ02_THREAD_WORK_SIZE) {
			// Ask for a new base
			u_int seed;
			sharedData->GetNewPixelIndex(pixelIndexBase, seed);
			pixelIndexOffset = 0;
			// rngPass generator
			rngGenerator.init(seed);
		}

		// Initialize sample0 and sample 1

		u_int pixelX, pixelY;
		if (imageSamplesEnable && film) {
			const u_int *subRegion = film->GetSubRegion();

			const u_int pixelIndex = (pixelIndexBase + pixelIndexOffset) % sharedData->filmRegionPixelCount;
			const u_int subRegionWidth = subRegion[1] - subRegion[0] + 1;
			pixelX = subRegion[0] + (pixelIndex % subRegionWidth);
			pixelY = subRegion[2] + (pixelIndex / subRegionWidth);

			// Check if the current pixel is over or under the noise threshold
			const Film *film = sharedData->engineFilm;
			if ((adaptiveStrength > 0.f) && film->HasChannel(Film::NOISE)) {
				// Pixels are sampled in accordance with how far from convergence they are
				// The floor for the pixel importance is given by the adaptiveness strength
				const float noise = Max(*(film->channel_NOISE->GetPixel(pixelX, pixelY)), 1.f - adaptiveStrength);

				if (rndGen->floatValue() > noise) {
					// Skip this pixel and try the next one

					// Workaround for preserving random number distribution behavior
					rngGenerator.uintValue();
					continue;
				}
			}

			pass = sharedData->GetNewPixelPass(pixelIndex);
		} else {
			pixelX = 0;
			pixelY = 0;

			pass = sharedData->GetNewPixelPass();
		}

		const u_int rngPass = rngGenerator.uintValue();;

		std::vector<u_int> dimensionsIndexes;
		const u_int numberTables = requestedSamples/2 + ((requestedSamples % 2) ? 1 : 0);
		dimensionsIndexes.reserve(numberTables);
		for (u_int i = 0; i < numberTables; i++) {
			dimensionsIndexes.push_back(i);
		}

		// Shuffle array using Peter-Yates algorithm
		for (u_int i = numberTables-1; i > 0; i--) { 
			const u_int j = cmj_hash_simple(i, rngPass) % (i+1); 

			const u_int tmp = dimensionsIndexes[i];
			dimensionsIndexes[i] = dimensionsIndexes[j];
			dimensionsIndexes[j] = tmp;
		} 

		pmj02sequence.dimensionsIndexes = dimensionsIndexes;

		sample0 = pixelX + pmj02sequence.GetSample(pass, 0);
		sample1 = pixelY + pmj02sequence.GetSample(pass, 1);
		break;
	}
}

void PMJ02Sampler::RequestSamples(const SampleType smplType, const u_int size) {
	Sampler::RequestSamples(smplType, size);
	pmj02sequence.RequestSamples(size);
	pixelIndexOffset = PMJ02_THREAD_WORK_SIZE;
	InitNewSample();
}

float PMJ02Sampler::GetSample(const u_int index) {
	assert (index < requestedSamples);

	switch (index) {
		case 0:
			return sample0;
		case 1:
			return sample1;
		default:
			return pmj02sequence.GetSample(pass, index);
	}
}

void PMJ02Sampler::RequestSamples(const SampleType smplType, const std::vector<SampleSize> smplSizes) {
	const u_int size = CalculateSampleIndexes(smplSizes);
	Sampler::RequestSamples(smplType, size);
	
	pmj02sequence.RequestSamples(size);
	
	pixelIndexOffset = PMJ02_THREAD_WORK_SIZE;
	InitNewSample();
}

float PMJ02Sampler::GetSample1D(const u_int index) {
	return pmj02sequence.GetSample(pass, sampleIndexes1D[index]);
}
void PMJ02Sampler::GetSample2D(const u_int index, float &u0, float &u1) {
	switch (index) {
		case 0: {
			u0 = sample0;
			u1 = sample1;
			break;
		}
		default: {
			u0 = pmj02sequence.GetSample(pass, sampleIndexes2D[index]);
			u1 = pmj02sequence.GetSample(pass, sampleIndexes2D[index] + 1);
		}
	}
}

void PMJ02Sampler::NextSample(const vector<SampleResult> &sampleResults) {
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
				throw runtime_error("Unknown sample type in PMJ02Sampler::NextSample(): " + ToString(sampleType));
		}
		film->AddSampleCount(pixelNormalizedCount, screenNormalizedCount);

		AtomicAddSamplesToFilm(sampleResults);
	}

	InitNewSample();
}

Properties PMJ02Sampler::ToProperties() const {
	return Sampler::ToProperties() <<
			Property("sampler.pmj02.adaptive.strength")(adaptiveStrength);
}

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

Properties PMJ02Sampler::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("sampler.type")) <<
			cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")) <<
			cfg.Get(GetDefaultProps().Get("sampler.pmj02.adaptive.strength"));
}

Sampler *PMJ02Sampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")).Get<bool>();

	const float str = Clamp(cfg.Get(GetDefaultProps().Get("sampler.pmj02.adaptive.strength")).Get<float>(), 0.f, .95f);

	return new PMJ02Sampler(rndGen, film, flmSplatter, imageSamplesEnable,
			str, (PMJ02SamplerSharedData *)sharedData);
}

slg::ocl::Sampler *PMJ02Sampler::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Sampler *oclSampler = new slg::ocl::Sampler();

	oclSampler->type = slg::ocl::PMJ02;
	oclSampler->pmj02.adaptiveStrength = Clamp(cfg.Get(GetDefaultProps().Get("sampler.pmj02.adaptive.strength")).Get<float>(), 0.f, .95f);

	return oclSampler;
}

Film::FilmChannelType PMJ02Sampler::GetRequiredChannels(const luxrays::Properties &cfg) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")).Get<bool>();

	const float str = cfg.Get(GetDefaultProps().Get("sampler.pmj02.adaptive.strength")).Get<float>();

	if (imageSamplesEnable && (str > 0.f))
		return Film::NOISE;
	else
		return Film::NONE;
}

const Properties &PMJ02Sampler::GetDefaultProps() {
	static Properties props = Properties() <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag()) <<
			Property("sampler.pmj02.adaptive.strength")(.95f);

	return props;
}
