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
#include "slg/samplers/random.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RandomSamplerSharedData
//------------------------------------------------------------------------------

RandomSamplerSharedData::RandomSamplerSharedData(Film *engineFlm) {
	engineFilm = engineFlm;

	const u_int *subRegion = engineFilm->GetSubRegion();
	filmRegionPixelCount = (subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1);
	pixelIndex = 0;
}

u_int RandomSamplerSharedData::GetNewPixelIndex() {
	SpinLocker spinLocker(spinLock);

	const u_int result = pixelIndex;
	pixelIndex = (pixelIndex + RANDOM_THREAD_WORK_SIZE) % filmRegionPixelCount;

	return result;
}

SamplerSharedData *RandomSamplerSharedData::FromProperties(const Properties &cfg,
		RandomGenerator *rndGen, Film *film) {
	return new RandomSamplerSharedData(film);
}

//------------------------------------------------------------------------------
// Random sampler
//------------------------------------------------------------------------------

RandomSampler::RandomSampler(luxrays::RandomGenerator *rnd, Film *flm,
			const FilmSampleSplatter *flmSplatter,
			const float adaptiveStr,
			RandomSamplerSharedData *samplerSharedData) :
		Sampler(rnd, flm, flmSplatter), sharedData(samplerSharedData),
		adaptiveStrength(adaptiveStr) {
}

void RandomSampler::InitNewSample() {
	for (;;) {
		// Update pixelIndexOffset

		pixelIndexOffset++;
		if (pixelIndexOffset >= RANDOM_THREAD_WORK_SIZE) {
			// Ask for a new base
			pixelIndexBase = sharedData->GetNewPixelIndex();
			pixelIndexOffset = 0;
		}

		// Initialize sample0 and sample 1

		const u_int *subRegion = film->GetSubRegion();

		const u_int pixelIndex = (pixelIndexBase + pixelIndexOffset) % sharedData->filmRegionPixelCount;
		const u_int subRegionWidth = subRegion[1] - subRegion[0] + 1;
		const u_int pixelX = subRegion[0] + (pixelIndex % subRegionWidth);
		const u_int pixelY = subRegion[2] + (pixelIndex / subRegionWidth);

		// Check if the current pixel is over or hunter the convergence threshold
		const Film *film = sharedData->engineFilm;
		if ((adaptiveStrength > 0.f) && film->HasChannel(Film::CONVERGENCE) &&
				(*(film->channel_CONVERGENCE->GetPixel(pixelX, pixelY)) == 0.f)) {
			// This pixel is already under the convergence threshold. Check if to
			// render or not
			if (rndGen->floatValue() < adaptiveStrength) {
				// Skip this pixel and try the next one
				continue;
			}
		}

		sample0 = pixelX + rndGen->floatValue();
		sample1 = pixelY + rndGen->floatValue();
		break;
	}
}

void RandomSampler::RequestSamples(const u_int size) {
	pixelIndexOffset = RANDOM_THREAD_WORK_SIZE;
	InitNewSample();
}

float RandomSampler::GetSample(const u_int index) {
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
	film->AddSampleCount(1.0);
	AddSamplesToFilm(sampleResults);
	InitNewSample();
}

Properties RandomSampler::ToProperties() const {
	return Sampler::ToProperties() <<
			Property("sampler.random.adaptive.strength")(adaptiveStrength);
}

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

Properties RandomSampler::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("sampler.type")) <<
			cfg.Get(GetDefaultProps().Get("sampler.random.adaptive.strength"));
}

Sampler *RandomSampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	const float str = Clamp(cfg.Get(GetDefaultProps().Get("sampler.random.adaptive.strength")).Get<float>(), 0.f, .95f);

	return new RandomSampler(rndGen, film, flmSplatter, str, (RandomSamplerSharedData *)sharedData);
}

slg::ocl::Sampler *RandomSampler::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Sampler *oclSampler = new slg::ocl::Sampler();

	oclSampler->type = slg::ocl::RANDOM;
	oclSampler->random.adaptiveStrength = Clamp(cfg.Get(GetDefaultProps().Get("sampler.random.adaptive.strength")).Get<float>(), 0.f, .95f);

	return oclSampler;
}

Film::FilmChannelType RandomSampler::GetRequiredChannels(const luxrays::Properties &cfg) {
	const float str = cfg.Get(GetDefaultProps().Get("sampler.random.adaptive.strength")).Get<float>();

	if (str > 0.f)
		return Film::CONVERGENCE;
	else
		return Film::NONE;
}

const Properties &RandomSampler::GetDefaultProps() {
	static Properties props = Properties() <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag()) <<
			Property("sampler.random.adaptive.strength")(.7f);

	return props;
}
