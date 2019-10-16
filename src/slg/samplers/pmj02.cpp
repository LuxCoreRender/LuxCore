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

PMJ02SamplerSharedData::PMJ02SamplerSharedData(Film *engineFlm) {
	engineFilm = engineFlm;

	if (engineFilm) {
		const u_int *subRegion = engineFilm->GetSubRegion();
		filmRegionPixelCount = (subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1);
	} else
		filmRegionPixelCount = 0;

	pixelIndex = 0;
}

u_int PMJ02SamplerSharedData::GetNewPixelIndex() {
	SpinLocker spinLocker(spinLock);

	const u_int result = pixelIndex;
	pixelIndex = (pixelIndex + PMJ02_THREAD_WORK_SIZE) % filmRegionPixelCount;

	return result;
}

SamplerSharedData *PMJ02SamplerSharedData::FromProperties(const Properties &cfg,
		RandomGenerator *rndGen, Film *film) {
	return new PMJ02SamplerSharedData(film);
}

//------------------------------------------------------------------------------
// PMJ02 sampler
//------------------------------------------------------------------------------

PMJ02Sampler::PMJ02Sampler(luxrays::RandomGenerator *rnd, Film *flm,
			const FilmSampleSplatter *flmSplatter, const bool imgSamplesEnable,
			const float adaptiveStr,
			PMJ02SamplerSharedData *samplerSharedData) :
		Sampler(rnd, flm, flmSplatter, imgSamplesEnable), sharedData(samplerSharedData),
		adaptiveStrength(adaptiveStr) {
}

void PMJ02Sampler::InitNewSample() {
	for (;;) {
		// Update pixelIndexOffset

		pixelIndexOffset++;
		if (pixelIndexOffset >= PMJ02_THREAD_WORK_SIZE) {
			// Ask for a new base
			pixelIndexBase = sharedData->GetNewPixelIndex();
			pixelIndexOffset = 0;
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

void PMJ02Sampler::RequestSamples(const SampleType smplType, const u_int size) {
	Sampler::RequestSamples(smplType, size);

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
			return rndGen->floatValue();
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

void PMJ02Sampler::generate_2D(float2 points[], u_int size, luxrays::RandomGenerator *rndGen) {
	points[0].x = rndGen->floatValue();
	points[0].y = rndGen->floatValue();
	for (u_int N = 1; N < size; N = 4 *N) {
		extend_sequence_even(points, N);
		extend_sequence_odd(points, 2 * N);
	}
}
void PMJ02Sampler::mark_occupied_strata(float2 points[], u_int N) {
	u_int NN = 2 * N;
	u_int num_shapes = (int)log2f(NN) + 1;
	occupiedStrata.resize(num_shapes);
	for (u_int shape = 0; shape < num_shapes; ++shape) {
		occupiedStrata[shape].resize(NN);
		for (int n = 0; n < NN; ++n) {
			occupiedStrata[shape][n] = false;
		}
	}
	for (int s = 0; s < N; ++s) {
		mark_occupied_strata1(points[s], NN);
	}
}

void PMJ02Sampler::mark_occupied_strata1(float2 pt, u_int NN) {
	u_int shape = 0;
	u_int xdivs = NN;
	u_int ydivs = 1;
	do {
		u_int xstratum = (u_int)(xdivs * pt.x);
		u_int ystratum = (u_int)(ydivs * pt.y);
		size_t index = ystratum * xdivs + xstratum;

		occupiedStrata[shape][index] = true;
		shape = shape + 1;
		xdivs = xdivs / 2;
		ydivs = ydivs * 2;
	} while (xdivs > 0);
}

void PMJ02Sampler::generate_sample_point(float2 points[], float i, float j, float xhalf, float yhalf, u_int n, u_int N) {
	u_int NN = 2 * N;
	float2 pt;
	do {
		pt.x = (i + 0.5f * (xhalf + rndGen->floatValue())) / n;
		pt.y = (j + 0.5f * (yhalf + rndGen->floatValue())) / n;
	} while (is_occupied(pt, NN));
	mark_occupied_strata1(pt, NN);
	points[num_samples] = pt;
	++num_samples;
}
void PMJ02Sampler::extend_sequence_even(float2 points[], u_int N) {
	u_int n = (int)sqrtf(N);
	occupied1Dx.resize(2 * N);
	occupied1Dy.resize(2 * N);
	mark_occupied_strata(points, N);
	for (int s = 0; s < N; ++s) {
		float2 oldpt = points[s];
		float i = floorf(n * oldpt.x);
		float j = floorf(n * oldpt.y);
		float xhalf = floorf(2.0f * (n * oldpt.x - i));
		float yhalf = floorf(2.0f * (n * oldpt.y - j));
		xhalf = 1.0f - xhalf;
		yhalf = 1.0f - yhalf;
		generate_sample_point(points, i, j, xhalf, yhalf, n, N);
	}
}

void PMJ02Sampler::extend_sequence_odd(float2 points[], u_int N) {
	u_int n = (int)sqrtf(N / 2);
	occupied1Dx.resize(2 * N);
	occupied1Dy.resize(2 * N);
	mark_occupied_strata(points, N);
	std::vector<float> xhalves(N / 2);
	std::vector<float> yhalves(N / 2);
	for (int s = 0; s < N / 2; ++s) {
		float2 oldpt = points[s];
		float i = floorf(n * oldpt.x);
		float j = floorf(n * oldpt.y);
		float xhalf = floorf(2.0f * (n * oldpt.x - i));
		float yhalf = floorf(2.0f * (n * oldpt.y - j));
		if (rndGen->floatValue() > 0.5f) {
			xhalf = 1.0f - xhalf;
		}
		else {
			yhalf = 1.0f - yhalf;
		}
		xhalves[s] = xhalf;
		yhalves[s] = yhalf;
		generate_sample_point(points, i, j, xhalf, yhalf, n, N);
	}
	for (int s = 0; s < N / 2; ++s) {
		float2 oldpt = points[s];
		float i = floorf(n * oldpt.x);
		float j = floorf(n * oldpt.y);
		float xhalf = 1.0f - xhalves[s];
		float yhalf = 1.0f - yhalves[s];
		generate_sample_point(points, i, j, xhalf, yhalf, n, N);
	}
}

bool PMJ02Sampler::is_occupied(float2 pt, u_int NN) {
	u_int shape = 0;
	u_int xdivs = NN;
	u_int ydivs = 1;
	do {
		u_int xstratum = (u_int)(xdivs * pt.x);
		u_int ystratum = (u_int)(ydivs * pt.y);
		size_t index = ystratum * xdivs + xstratum;
		if (occupiedStrata[shape][index]) {
			return true;
		}
		shape = shape + 1;
		xdivs = xdivs / 2;
		ydivs = ydivs * 2;
	} while (xdivs > 0);
	return false;
}

void PMJ02Sampler::shuffle(float2 points[], u_int size) {

	constexpr u_int odd[8] = { 0, 1, 4, 5, 10, 11, 14, 15 };
	constexpr u_int even[8] = { 2, 3, 6, 7, 8, 9, 12, 13 };

	u_int rng_index = 0;
	for (u_int yy = 0; yy < size / 16; ++yy) {
		for (u_int xx = 0; xx < 8; ++xx) {
			u_int other = (u_int)(rndGen->floatValue() * (8.0f - xx) + xx);
			float2 tmp = points[odd[other] + yy * 16];
			points[odd[other] + yy * 16] = points[odd[xx] + yy * 16];
			points[odd[xx] + yy * 16] = tmp;
		}
		for (u_int xx = 0; xx < 8; ++xx) {
			u_int other = (u_int)(rndGen->floatValue() * (8.0f - xx) + xx);
			float2 tmp = points[even[other] + yy * 16];
			points[even[other] + yy * 16] = points[even[xx] + yy * 16];
			points[even[xx] + yy * 16] = tmp;
		}
	}
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
