/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include "luxrays/utils/sampler/sampler.h"
#include "luxrays/utils/core/spectrum.h"

namespace luxrays { namespace utils {

//------------------------------------------------------------------------------
// Metropolis sampler
//------------------------------------------------------------------------------

MetropolisSampler::MetropolisSampler(RandomGenerator *rnd, Film *flm, const unsigned int maxRej,
		const float pLarge, const float imgRange) : Sampler(rnd, flm),
		maxRejects(maxRej),	largeMutationProbability(pLarge), imageRange(imgRange),
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

	totalLuminance = 0.;
	sampleCount = 0.;

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
			s = MutateScaled(s, imageRange, rndGen->floatValue());
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
		const float luminance = sr->radiance.Y();
		assert (!isnan(luminance) && !isinf(luminance));

		if ((luminance >= 0.f) &&!isnan(luminance) && !isinf(luminance)) {
			if (sr->type == PER_SCREEN_NORMALIZED) {
				// This is required because the way camera pixel area is expressed (i.e. normalized
				// coordinates Vs pixels). I should probably fix the source of the problem
				// instead of this workaround.
				newLuminance += luminance / pixelCount;
			} else
				newLuminance += luminance;
		}
	}

	if (isLargeMutation) {
		totalLuminance += newLuminance;
		sampleCount += 1.;
	}

	const float meanIntensity = (totalLuminance > 0.) ?
		static_cast<float>(totalLuminance / sampleCount) : 1.f;

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
		const float norm = weight / (currentLuminance / meanIntensity + currentLargeMutationProbability);
		if (norm > 0.f) {
			for (std::vector<SampleResult>::const_iterator sr = currentSampleResult.begin(); sr < currentSampleResult.end(); ++sr) {
				film->SplatFiltered(sr->type, sr->screenX, sr->screenY, sr->radiance, norm);
				film->SplatFilteredAlpha(sr->screenX, sr->screenY, sr->alpha, norm);
			}
		}

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
		const float norm = newWeight / (newLuminance / meanIntensity + currentLargeMutationProbability);
		if (norm > 0.f) {
			for (std::vector<SampleResult>::const_iterator sr = sampleResults.begin(); sr < sampleResults.end(); ++sr) {
				film->SplatFiltered(sr->type, sr->screenX, sr->screenY, norm * sr->radiance, norm);
				film->SplatFilteredAlpha(sr->screenX, sr->screenY, sr->alpha, norm);
			}
		}

		// Restart from previous reference
		stamp = currentStamp;
		std::copy(currentSamples, currentSamples + sampleSize, samples);
		std::copy(currentSampleStamps, currentSampleStamps + sampleSize, sampleStamps);

		++consecRejects;
	}

	// Cooldown is used in order to not have problems in the estimation of meanIntensity
	// when 
	if (cooldown) {
		// Check if it is time to end the cooldown
		if (sampleCount > pixelCount) {
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

} }
