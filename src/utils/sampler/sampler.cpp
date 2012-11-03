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

/*MetropolisSampler::MetropolisSampler(RandomGenerator *rnd, Film *flm, const unsigned int maxRej,
		const float pLarge, const float imgRange) : Sampler(rnd, flm),
		maxRejects(maxRej),	largeMutationProbability(pLarge), imageRange(imgRange),
		samples(NULL), sampleStamps(NULL), currentSamples(NULL), currentSampleStamps(NULL) {
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
		float x1 = x + dx;
		return (x1 < 1.f) ? x1 : x1 - 1.f;
	} else {
		float x1 = x - dx;
		return (x1 < 0.f) ? x1 + 1.f : x1;
	}
}

// Mutate a value in the range [min-max]
//static float MutateScaled(const float x, const float randomValue, const float mini, const float maxi, const float range) {
//	static const float s1 = 32.f;
//	const float dx = range / (s1 / (1.f + s1) + (s1 * s1) / (1.f + s1) *
//			fabsf(2.f * randomValue - 1.f)) - range / s1;
//	if (randomValue < 0.5f) {
//		float x1 = x + dx;
//		return (x1 < maxi) ? x1 : x1 - maxi + mini;
//	} else {
//		float x1 = x - dx;
//		return (x1 < mini) ? x1 - mini + maxi : x1;
//	}
//}

void MetropolisSampler::RequestSamples(const unsigned int size) {
	sampleSize = size;
	samples = new float[sampleSize];
	sampleStamps = new unsigned int[sampleSize];
	currentSamples = new float[sampleSize];
	currentSampleStamps = new unsigned int[sampleSize];

	stamp = 0;
	totalLuminance = 0.;
	sampleCount = 0.;
	
	ResetData();
}

float MetropolisSampler::GetSample(const unsigned int index) {
	if (stamp == 0) {
		const float s = rndGen->floatValue();
		samples[index] = s;

		return s;
	} else {
		const float sampleStamp = sampleStamps[index];

		float s;
		if (sampleStamp == 0)
			s = rndGen->floatValue();
		else
			s = samples[index];

		// Mutate the sample up to the currentStamp
		for (unsigned int i = sampleStamp; i < stamp; ++i)
			s = Mutate(s, rndGen->floatValue());

		samples[index] = s;

		return s;
	}
}

void MetropolisSampler::NextSample(const SampleResult *sampleResults, const unsigned int size) {
	// Calculate the sample result luminance
	float newLuminance = 0.f;
	for (unsigned int i = 0; i < size; ++i) {
		SampleResult *sampleResult = &sampleResults[i];
		const float luminance = sampleResult

		if 
	}
	
	if (sampleResult2)
		newLuminance = sampleResult1->radiance.Y() + sampleResult2->radiance.Y();
	else
		newLuminance = sampleResult1->radiance.Y();
	newLuminance = (!isnan(newLuminance) && !isinf(newLuminance)) ?
		newLuminance : 0.f;
	assert (!isnan(newLuminance) && !isinf(newLuminance));

	if (isLargeMuattion) {
		totalLuminance += newLuminance;
		sampleCount += 1.;
	}

	const float meanIntensity = (totalLuminance > 0.) ?
		static_cast<float>(totalLuminance / sampleCount) : 1.f;

	// Calculate accept probability from old and new image sample
	float accProb;
	if ((currentLuminance > 0.f) && (consecRejects < maxRejects))
		accProb = Min<float>(1.f, newLuminance / currentLuminance);
	else
		accProb = 1.f;
	const float newWeight = accProb + (isLargeMuattion ? 1.f : 0.f);
	weight += 1.f - accProb;

	// Try or force accepting of the new sample
	if ((accProb == 1.f) || (rndGen->floatValue() < accProb)) {
		// Add accumulated contribution of previous reference sample
		const float norm = weight / (currentLuminance / meanIntensity + largeMutationProbability);
		if (norm > 0.f) {
			film->AddSampleCount(currentSampleResult1->type, 1.f);
			film->SplatFiltered(currentSampleResult1->type, currentSampleResult1->screenX, currentSampleResult1->screenY,
					norm * currentSampleResult1->radiance);
			if (currentSampleResult2) {
				film->AddSampleCount(currentSampleResult2->type, 1.f);
				film->SplatFiltered(currentSampleResult2->type, currentSampleResult2->screenX, currentSampleResult2->screenY,
						norm * currentSampleResult2->radiance);
			}
		}

		// Save new contributions for reference
		weight = newWeight;
		currentStamp = stamp;
		currentLuminance = newLuminance;
		Swap(currentSamples, samples);
		Swap(currentSampleStamps, sampleStamps);
		currentSampleResult1 = sampleResult1;
		currentSampleResult2 = sampleResult2;

		consecRejects = 0;
	} else {
		// Add contribution of new sample before rejecting it
		const float norm = newWeight / (newLuminance / meanIntensity + largeMutationProbability);
		if (norm > 0.f) {
			film->AddSampleCount(sampleResult1->type, 1.f);
			film->SplatFiltered(sampleResult1->type, sampleResult1->screenX, sampleResult1->screenY,
					norm * sampleResult1->radiance);
			if (sampleResult2) {
				film->AddSampleCount(sampleResult2->type, 1.f);
				film->SplatFiltered(sampleResult2->type, sampleResult2->screenX, sampleResult2->screenY,
						norm * sampleResult2->radiance);
			}
		}

		// Restart from previous reference
		stamp = currentStamp;

		++consecRejects;
	}

	isLargeMuattion = (rndGen->floatValue() < largeMutationProbability);
	if (isLargeMuattion) {
		ResetData();
	} else
		++stamp;
}*/

} }
