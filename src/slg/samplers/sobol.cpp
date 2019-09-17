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
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SobolSamplerSharedData
//------------------------------------------------------------------------------

SobolSamplerSharedData::SobolSamplerSharedData(const u_int seed, Film *engineFlm) : SamplerSharedData() {
	Init(seed, engineFlm);
}

SobolSamplerSharedData::SobolSamplerSharedData(RandomGenerator *rndGen, Film *engineFlm) : SamplerSharedData() {
	Init(rndGen->uintValue() % (0xFFFFFFFFu - 1u) + 1u, engineFlm);
}

void SobolSamplerSharedData::Init(const u_int seed, Film *engineFlm) {
	engineFilm = engineFlm;
	seedBase = seed;

	if (engineFilm) {
		const u_int *subRegion = engineFilm->GetSubRegion();
		filmRegionPixelCount = (subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1);
		
		// Initialize with SOBOL_STARTOFFSET the vector holding the passes per pixel
		passPerPixel.resize(filmRegionPixelCount, SOBOL_STARTOFFSET);
	} else {
		filmRegionPixelCount = 0;
		passPerPixel.resize(1, SOBOL_STARTOFFSET);
	}

	pixelIndex = 0;
}

void SobolSamplerSharedData::GetNewPixelIndex(u_int &index, u_int &seed) {
	SpinLocker spinLocker(spinLock);
	
	index = pixelIndex;
	seed = (seedBase + pixelIndex) % (0xFFFFFFFFu - 1u) + 1u;
	
	pixelIndex += SOBOL_THREAD_WORK_SIZE;
	if (pixelIndex >= filmRegionPixelCount)
		pixelIndex = 0;
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
		const float adaptiveStr,
		SobolSamplerSharedData *samplerSharedData) : Sampler(rnd, flm, flmSplatter, imgSamplesEnable),
		sharedData(samplerSharedData), sobolSequence(), adaptiveStrength(adaptiveStr),
		rngGenerator() {
}

SobolSampler::~SobolSampler() {
}

void SobolSampler::InitNewSample() {
	for (;;) {
		// Update pixelIndexOffset

		pixelIndexOffset++;
		if ((pixelIndexOffset >= SOBOL_THREAD_WORK_SIZE) ||
				(pixelIndexBase + pixelIndexOffset >= sharedData->filmRegionPixelCount)) {
			// Ask for a new base
			u_int seed;
			sharedData->GetNewPixelIndex(pixelIndexBase, seed);
			pixelIndexOffset = 0;

			// Initialize the rng0, rng1 and rngPass generator
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
			
			// Check if the current pixel is over or under the convergence threshold
			const Film *film = sharedData->engineFilm;
			if ((adaptiveStrength > 0.f) && film->HasChannel(Film::NOISE)) {
				// Pixels are sampled in accordance with how far from convergence they are
				// The floor for the pixel importance is given by the adaptiveness strength
				const float noise = Max(*(film->channel_NOISE->GetPixel(pixelX, pixelY)), 1.f - adaptiveStrength);

				if (rndGen->floatValue() > noise) {

					// Workaround for preserving random number distribution behavior
					rngGenerator.floatValue();
					rngGenerator.floatValue();
					rngGenerator.uintValue();

					// Skip this pixel and try the next one
					continue;
				}
			}

			pass = sharedData->GetNewPixelPass(pixelIndex);
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

	pixelIndexOffset = SOBOL_THREAD_WORK_SIZE;
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
		double pixelNormalizedCount, screenNormalizedCount;
		switch (sampleType) {
			case PIXEL_NORMALIZED_ONLY:
				pixelNormalizedCount = 1.f;
				screenNormalizedCount = 0.f;
				break;
			case SCREEN_NORMALIZED_ONLY:
				pixelNormalizedCount = 0.f;
				screenNormalizedCount = 1.f;
				break;
			case PIXEL_NORMALIZED_AND_SCREEN_NORMALIZED:
				pixelNormalizedCount = 1.f;
				screenNormalizedCount = 1.f;
				break;
			default:
				throw runtime_error("Unknown sample type in SobolSampler::NextSample(): " + ToString(sampleType));
		}
		film->AddSampleCount(pixelNormalizedCount, screenNormalizedCount);

		AtomicAddSamplesToFilm(sampleResults);
	}

	InitNewSample();
}

Properties SobolSampler::ToProperties() const {
	return Sampler::ToProperties() <<
			Property("sampler.sobol.adaptive.strength")(adaptiveStrength);
}

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

Properties SobolSampler::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("sampler.type")) <<
			cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")) <<
			cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength"));
}

Sampler *SobolSampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")).Get<bool>();

	const float str = Clamp(cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength")).Get<float>(), 0.f, .95f);

	return new SobolSampler(rndGen, film, flmSplatter, imageSamplesEnable,
			str, (SobolSamplerSharedData *)sharedData);
}

slg::ocl::Sampler *SobolSampler::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Sampler *oclSampler = new slg::ocl::Sampler();

	oclSampler->type = slg::ocl::SOBOL;
	oclSampler->sobol.adaptiveStrength = Clamp(cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength")).Get<float>(), 0.f, .95f);

	return oclSampler;
}

Film::FilmChannelType SobolSampler::GetRequiredChannels(const luxrays::Properties &cfg) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")).Get<bool>();

	const float str = cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength")).Get<float>();

	if (imageSamplesEnable && (str > 0.f))
		return Film::NOISE;
	else
		return Film::NONE;
}

const Properties &SobolSampler::GetDefaultProps() {
	static Properties props = Properties() <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag()) <<
			Property("sampler.sobol.adaptive.strength")(.95f);

	return props;
}
