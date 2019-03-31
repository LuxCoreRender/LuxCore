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
		
		// Initialize with zeros the vector holding the passes per pixel
		passPerPixel.resize(filmRegionPixelCount, 0);
	} else
		filmRegionPixelCount = 0;

	pixelIndex = 0;

	pass = SOBOL_STARTOFFSET;
}

void SobolSamplerSharedData::GetNewPixelIndex(u_int &index, u_int &sobolPass, u_int &seed) {
	SpinLocker spinLocker(spinLock);
	
	index = pixelIndex;
	sobolPass = pass;
	seed = (seedBase + pixelIndex) % (0xFFFFFFFFu - 1u) + 1u;
	
	pixelIndex += SOBOL_THREAD_WORK_SIZE;
	if (pixelIndex >= filmRegionPixelCount) {
		pixelIndex = 0;
		pass++;
	}
}

u_int SobolSamplerSharedData::GetPixelPass(const u_int index) {
	// Don't know if a spinLocker is the right choice or if just using AtomicAdd below would suffice
	SpinLocker spinLocker(spinLock);

	// Iterate pass of this pixel
	passPerPixel[index] = passPerPixel[index] + 1;
	return passPerPixel[index];	
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
		const FilmSampleSplatter *flmSplatter,
		const float adaptiveStr,
		SobolSamplerSharedData *samplerSharedData) : Sampler(rnd, flm, flmSplatter),
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
			sharedData->GetNewPixelIndex(pixelIndexBase, pass, seed);
			pixelIndexOffset = 0;

			// Initialize the rng0, rng1 and rngPass generator
			rngGenerator.init(seed);
		}

		// Initialize sample0 and sample 1

		u_int pixelX, pixelY;
		if (film) {
			const u_int *subRegion = film->GetSubRegion();

			const u_int pixelIndex = (pixelIndexBase + pixelIndexOffset) % sharedData->filmRegionPixelCount;
			const u_int subRegionWidth = subRegion[1] - subRegion[0] + 1;
			pixelX = subRegion[0] + (pixelIndex % subRegionWidth);
			pixelY = subRegion[2] + (pixelIndex / subRegionWidth);
			
			
			// Check if the current pixel is over or under the convergence threshold
			const Film *film = sharedData->engineFilm;
			if ((adaptiveStrength > 0.f) && film->HasChannel(Film::CONVERGENCE)) {
				// Pixels are sampled in accordance with how far from convergence they are
				const float convergence = *(film->channel_CONVERGENCE->GetPixel(pixelX, pixelY));
				if (rndGen->floatValue() > ((convergence + (1 - adaptiveStrength)) * adaptiveStrength)) {
					// Skip this pixel and try the next one
					continue;
				} else {
					currentPixelPass = sharedData->GetPixelPass(pixelIndex);
				}
			} else {
				currentPixelPass = sharedData->GetPixelPass(pixelIndex);
			}
		} else {
			pixelX = 0;
			pixelY = 0;
		}

		// Initialize rng0, rng1 and rngPass

		sobolSequence.rng0 = rngGenerator.floatValue();
		sobolSequence.rng1 = rngGenerator.floatValue();
		sobolSequence.rngPass = rngGenerator.uintValue();

		sample0 = pixelX +  sobolSequence.GetSample(currentPixelPass, 0);
		sample1 = pixelY +  sobolSequence.GetSample(currentPixelPass, 1);
		break;
	}
}

void SobolSampler::RequestSamples(const u_int size) {
	sobolSequence.RequestSamples(size);

	pixelIndexOffset = SOBOL_THREAD_WORK_SIZE;
	InitNewSample();
}

float SobolSampler::GetSample(const u_int index) {
	switch (index) {
		case 0:
			return sample0;
		case 1:
			return sample1;
		default:
			return sobolSequence.GetSample(currentPixelPass, index);
	}
}

void SobolSampler::NextSample(const vector<SampleResult> &sampleResults) {
	if (film) {
		film->AddSampleCount(1.0);
		AtomicAddSamplesToFilm(sampleResults);
	}

	InitNewSample();
}

Properties SobolSampler::ToProperties() const {
	return Sampler::ToProperties() <<
			Property("sampler.random.adaptive.strength")(adaptiveStrength);
}

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

Properties SobolSampler::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("sampler.type")) <<
			cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength"));
}

Sampler *SobolSampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	const float str = Clamp(cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength")).Get<float>(), 0.f, .95f);

	return new SobolSampler(rndGen, film, flmSplatter, str, (SobolSamplerSharedData *)sharedData);
}

slg::ocl::Sampler *SobolSampler::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Sampler *oclSampler = new slg::ocl::Sampler();

	oclSampler->type = slg::ocl::SOBOL;
	oclSampler->sobol.adaptiveStrength = Clamp(cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength")).Get<float>(), 0.f, .95f);

	return oclSampler;
}

Film::FilmChannelType SobolSampler::GetRequiredChannels(const luxrays::Properties &cfg) {
	const float str = cfg.Get(GetDefaultProps().Get("sampler.sobol.adaptive.strength")).Get<float>();

	if (str > 0.f)
		return Film::CONVERGENCE;
	else
		return Film::NONE;
}

const Properties &SobolSampler::GetDefaultProps() {
	static Properties props = Properties() <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag()) <<
			Property("sampler.sobol.adaptive.strength")(.7f);

	return props;
}
