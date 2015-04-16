/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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
#include "slg/samplers/metropolis.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Metropolis sampler
//------------------------------------------------------------------------------

MetropolisSampler::MetropolisSampler(RandomGenerator *rnd, Film *flm, const unsigned int maxRej,
		const float pLarge, const float imgRange,
		double *sharedTotLum, double *sharedSplCount) : Sampler(rnd, flm),
		maxRejects(maxRej),	largeMutationProbability(pLarge), imageMutationRange(imgRange),
		sharedTotalLuminance(sharedTotLum), sharedSampleCount(sharedSplCount),
		samples(NULL), sampleStamps(NULL), currentSamples(NULL), currentSampleStamps(NULL),
		cooldown(true) {
}

MetropolisSampler::~MetropolisSampler() {
	delete samples;
	delete sampleStamps;
	delete currentSamples;
	delete currentSampleStamps;
}

// Mutate a value in the range [0-1]
static float Mutate(const float x, const float randomValue) {
	static const float s1 = 1.f / 512.f, s2 = 1.f / 16.f;

	const float dx = s1 / (s1 / s2 + fabsf(2.f * randomValue - 1.f)) -
			s1 / (s1 / s2 + 1.f);

	if (randomValue < 0.5f) {
		float mutatedX = x + dx;
		return (mutatedX < 1.f) ? mutatedX : mutatedX - 1.f;
	} else {
		float mutatedX = x - dx;
		return (mutatedX < 0.f) ? mutatedX + 1.f : mutatedX;
	}
}

// Mutate a value max. by a range value
float MutateScaled(const float x, const float range, const float randomValue) {
	static const float s1 = 32.f;
	
	const float dx = range / (s1 / (1.f + s1) + (s1 * s1) / (1.f + s1) *
		fabs(2.f * randomValue - 1.f)) - range / s1;

	float mutatedX = x;
	if (randomValue < 0.5f) {
		mutatedX += dx;
		return (mutatedX < 1.f) ? mutatedX : (mutatedX - 1.f);
	} else {
		mutatedX -= dx;
		return (mutatedX < 0.f) ? (mutatedX + 1.f) : mutatedX;
	}
}

void MetropolisSampler::RequestSamples(const unsigned int size) {
	sampleSize = size;
	samples = new float[sampleSize];
	sampleStamps = new unsigned int[sampleSize];
	currentSamples = new float[sampleSize];
	currentSampleStamps = new unsigned int[sampleSize];

	*sharedTotalLuminance = 0.;
	*sharedSampleCount = 0.;

	isLargeMutation = true;
	weight = 0.f;
	consecRejects = 0;
	currentLuminance = 0.;
	std::fill(sampleStamps, sampleStamps + sampleSize, 0);
	stamp = 1;
	currentStamp = 1;
	currentSampleResult.resize(0);
}

float MetropolisSampler::GetSample(const unsigned int index) {
	assert (index < sampleSize);

	unsigned int sampleStamp = sampleStamps[index];

	float s;
	if (sampleStamp == 0) {
		s = rndGen->floatValue();
		sampleStamp = 1;
	} else
		s = samples[index];

	// Mutate the sample up to the currentStamp
	if ((index == 0) || (index == 1)) {
		// 0 and 1 are used for image X/Y
		for (unsigned int i = sampleStamp; i < stamp; ++i)
			s = MutateScaled(s, imageMutationRange, rndGen->floatValue());
	} else {
		for (unsigned int i = sampleStamp; i < stamp; ++i)
			s = Mutate(s, rndGen->floatValue());
	}

	samples[index] = s;
	sampleStamps[index] = stamp;

	return s;
}

void MetropolisSampler::NextSample(const std::vector<SampleResult> &sampleResults) {
	film->AddSampleCount(1.0);

	// Calculate the sample result luminance
	const unsigned int pixelCount = film->GetWidth() * film->GetHeight();
	float newLuminance = 0.f;
	for (std::vector<SampleResult>::const_iterator sr = sampleResults.begin(); sr != sampleResults.end(); ++sr) {
		if (sr->HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED)) {
			for (u_int i = 0; i < sr->radiancePerPixelNormalized.size(); ++i) {
				const float luminance = sr->radiancePerPixelNormalized[i].Y();
				assert (!isnan(luminance) && !isinf(luminance));

				if ((luminance > 0.f) && !isnan(luminance) && !isinf(luminance))
					newLuminance += luminance;
			}
		}

		if (sr->HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED)) {
			for (u_int i = 0; i < sr->radiancePerScreenNormalized.size(); ++i) {
				const float luminance = sr->radiancePerScreenNormalized[i].Y();
				assert (!isnan(luminance) && !isinf(luminance));

				if ((luminance > 0.f) && !isnan(luminance) && !isinf(luminance))
					newLuminance += luminance;
			}
		}
	}

	if (isLargeMutation) {
		*sharedTotalLuminance += newLuminance;
		*sharedSampleCount += 1.;
	}

	const float invMeanIntensity = (*sharedTotalLuminance > 0.) ?
		static_cast<float>(*sharedSampleCount / *sharedTotalLuminance) : 1.f;

	// Define the probability of large mutations. It is 50% if we are still
	// inside the cooldown phase.
	const float currentLargeMutationProbability = (cooldown) ? .5f : largeMutationProbability;

	// Calculate accept probability from old and new image sample
	float accProb;
	if ((currentLuminance > 0.f) && (consecRejects < maxRejects))
		accProb = Min<float>(1.f, newLuminance / currentLuminance);
	else
		accProb = 1.f;
	const float newWeight = accProb + (isLargeMutation ? 1.f : 0.f);
	weight += 1.f - accProb;

	// Try or force accepting of the new sample
	if ((accProb == 1.f) || (rndGen->floatValue() < accProb)) {
		// Add accumulated SampleResult of previous reference sample
		const float norm = weight / (currentLuminance * invMeanIntensity + currentLargeMutationProbability);
		if (norm > 0.f)
			AddSamplesToFilm(currentSampleResult, norm);

		// Save new contributions for reference
		weight = newWeight;
		currentStamp = stamp;
		currentLuminance = newLuminance;
		std::copy(samples, samples + sampleSize, currentSamples);
		std::copy(sampleStamps, sampleStamps + sampleSize, currentSampleStamps);
		currentSampleResult = sampleResults;

		consecRejects = 0;
	} else {
		// Add contribution of new sample before rejecting it
		const float norm = newWeight / (newLuminance * invMeanIntensity + currentLargeMutationProbability);
		if (norm > 0.f)
			AddSamplesToFilm(sampleResults, norm);

		// Restart from previous reference
		stamp = currentStamp;
		std::copy(currentSamples, currentSamples + sampleSize, samples);
		std::copy(currentSampleStamps, currentSampleStamps + sampleSize, sampleStamps);

		++consecRejects;
	}

	// Cooldown is used in order to not have problems in the estimation of meanIntensity
	// when large mutation probability is very small
	if (cooldown) {
		// Check if it is time to end the cooldown
		if (*sharedSampleCount > pixelCount) {
			cooldown = false;
			isLargeMutation = (rndGen->floatValue() < currentLargeMutationProbability);
		} else
			isLargeMutation = (rndGen->floatValue() < .5f);
	} else
		isLargeMutation = (rndGen->floatValue() < currentLargeMutationProbability);

	if (isLargeMutation) {
		stamp = 1;
		std::fill(sampleStamps, sampleStamps + sampleSize, 0);
	} else
		++stamp;
}
