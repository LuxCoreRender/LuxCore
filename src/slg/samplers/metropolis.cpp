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
#include "slg/samplers/metropolis.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// MetropolisSamplerSharedData
//------------------------------------------------------------------------------

MetropolisSamplerSharedData::MetropolisSamplerSharedData() : SamplerSharedData() {
	totalLuminance = 0.;
	sampleCount = 0.;
}

SamplerSharedData *MetropolisSamplerSharedData::FromProperties(const Properties &cfg,
		RandomGenerator *rndGen, Film *film) {
	return new MetropolisSamplerSharedData();
}

//------------------------------------------------------------------------------
// Metropolis sampler
//------------------------------------------------------------------------------

MetropolisSampler::MetropolisSampler(RandomGenerator *rnd, Film *flm,
		const FilmSampleSplatter *flmSplatter, const bool imgSamplesEnable,
		const u_int maxRej, const float pLarge, const float imgRange,
		MetropolisSamplerSharedData *samplerSharedData) : Sampler(rnd, flm, flmSplatter, imgSamplesEnable),
		sharedData(samplerSharedData),
		maxRejects(maxRej),	largeMutationProbability(pLarge), imageMutationRange(imgRange),
		samples(NULL), sampleStamps(NULL), currentSamples(NULL), currentSampleStamps(NULL),
		cooldown(true) {
}

MetropolisSampler::~MetropolisSampler() {
	delete[] samples;
	delete[] sampleStamps;
	delete[] currentSamples;
	delete[] currentSampleStamps;
}

// Mutate a value in the range [0-1]
static float Mutate(const float x, const float randomValue) {
	static const float s1 = 1.f / 512.f;
	static const float s2 = 1.f / 16.f;

	const float dx = s1 / (s1 / s2 + fabsf(2.f * randomValue - 1.f)) -
			s1 / (s1 / s2 + 1.f);

	float mutatedX = x;
	if (randomValue < .5f) {
		mutatedX += dx;
		mutatedX = (mutatedX < 1.f) ? mutatedX : (mutatedX - 1.f);
	} else {
		mutatedX -= dx;
		mutatedX = (mutatedX < 0.f) ? (mutatedX + 1.f) : mutatedX;
	}

	// mutatedX can still be 1.f due to numerical precision problems
	if (mutatedX == 1.f)
		mutatedX = 0.f;

	return mutatedX;
}

// Mutate a value max. by a range value
float MutateScaled(const float x, const float range, const float randomValue) {
	static const float s1 = 32.f;
	
	const float dx = range / (s1 / (1.f + s1) + (s1 * s1) / (1.f + s1) *
		fabs(2.f * randomValue - 1.f)) - range / s1;

	float mutatedX = x;
	if (randomValue < .5f) {
		mutatedX += dx;
		mutatedX = (mutatedX < 1.f) ? mutatedX : (mutatedX - 1.f);
	} else {
		mutatedX -= dx;
		mutatedX = (mutatedX < 0.f) ? (mutatedX + 1.f) : mutatedX;
	}

	// mutatedX can still be 1.f due to numerical precision problems
	if (mutatedX == 1.f)
		mutatedX = 0.f;

	return mutatedX;
}

void MetropolisSampler::RequestSamples(const SampleType smplType, const u_int size) {
	Sampler::RequestSamples(smplType, size);

	samples = new float[requestedSamples];
	sampleStamps = new u_int[requestedSamples];
	currentSamples = new float[requestedSamples];
	currentSampleStamps = new u_int[requestedSamples];

	isLargeMutation = true;
	weight = 0.f;
	consecRejects = 0;
	currentLuminance = 0.f;
	fill(sampleStamps, sampleStamps + requestedSamples, 0);
	stamp = 1;
	currentStamp = 1;
	currentSampleResult.resize(0);
}

float MetropolisSampler::GetSample(const u_int index) {
	assert (index < requestedSamples);

	u_int sampleStamp = sampleStamps[index];

	float s;
	if (sampleStamp == 0) {
		s = rndGen->floatValue();
		sampleStamp = 1;
		
		assert (s != 1.f);
	} else
		s = samples[index];

	// Mutate the sample up to the currentStamp
	if (imageSamplesEnable && film && ((index == 0) || (index == 1))) {
		// 0 and 1 are used for image X/Y
		for (u_int i = sampleStamp; i < stamp; ++i)
			s = MutateScaled(s, imageMutationRange, rndGen->floatValue());
		
		samples[index] = s;
		sampleStamps[index] = stamp;

		const u_int *subRegion = film->GetSubRegion();
		const u_int subRegionIndex = (index == 0) ? 0 : 2;
		s = subRegion[subRegionIndex] + s * (subRegion[subRegionIndex + 1] - subRegion[subRegionIndex] + 1);

		return s;
	} else {
		for (u_int i = sampleStamp; i < stamp; ++i)
			s = Mutate(s, rndGen->floatValue());

		samples[index] = s;
		sampleStamps[index] = stamp;

		return s;
	}
}

void MetropolisSampler::NextSample(const vector<SampleResult> &sampleResults) {
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
				throw runtime_error("Unknown sample type in MetropolisSampler::NextSample(): " + ToString(sampleType));
		}
		film->AddSampleCount(pixelNormalizedCount, screenNormalizedCount);
	}

	// Calculate the sample result luminance
	float newLuminance = 0.f;
	for (vector<SampleResult>::const_iterator sr = sampleResults.begin(); sr != sampleResults.end(); ++sr) {
		if (sr->HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED)) {
			for (u_int i = 0; i < sr->radiance.size(); ++i) {
				const float luminance = sr->radiance[i].Y();
				assert (!isnan(luminance) && !isinf(luminance) && (luminance >= 0.f));

				if ((luminance > 0.f) && !isnan(luminance) && !isinf(luminance))
					newLuminance += luminance;
			}
		}

		if (sr->HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED)) {
			for (u_int i = 0; i < sr->radiance.size(); ++i) {
				const float luminance = sr->radiance[i].Y();
				assert (!isnan(luminance) && !isinf(luminance) && (luminance >= 0.f));

				if ((luminance > 0.f) && !isnan(luminance) && !isinf(luminance))
					newLuminance += luminance;
			}
		}
	}

	if (isLargeMutation) {
		// Atomic 64bit for double are not available on all CPUs so I simply
		// avoid synchronization.
		sharedData->totalLuminance += newLuminance;
		sharedData->sampleCount += 1.;
	}

	const float invMeanIntensity = (sharedData->totalLuminance > 0.) ?
		static_cast<float>(sharedData->sampleCount / sharedData->totalLuminance) : 1.f;

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

	const float rndVal = rndGen->floatValue();

	/*if (!cooldown && (currentSampleResult.size() > 0) && (sampleResults.size() > 0))
		printf("[%d] Current: (%f, %f, %f) [%f] Proposed: (%f, %f, %f) [%f] accProb: %f <%f>\n",
				consecRejects,
				currentSampleResult[0].radiance[0].c[0], currentSampleResult[0].radiance[0].c[1], currentSampleResult[0].radiance[0].c[2], weight,
				sampleResults[0].radiance[0].c[0], sampleResults[0].radiance[0].c[1], sampleResults[0].radiance[0].c[2], newWeight,
				accProb, rndVal);*/
	
	// Try or force accepting of the new sample
	if ((accProb == 1.f) || (rndVal < accProb)) {
		/*if (!cooldown)
			printf("\t\tACCEPTED !\n");*/

		// Add accumulated SampleResult of previous reference sample
		const float norm = weight / (currentLuminance * invMeanIntensity + currentLargeMutationProbability);
		if (norm > 0.f) {
			/*if (!cooldown)
				printf("\t\tContrib: (%f, %f, %f) [%f] consecutiveRejects: %d\n",
						currentSampleResult[0].radiance[0].c[0],
						currentSampleResult[0].radiance[0].c[1],
						currentSampleResult[0].radiance[0].c[2],
						norm, consecRejects);*/

			if (film)
				AtomicAddSamplesToFilm(currentSampleResult, norm);
		}

		lastSampleAcceptance = METRO_ACCEPTED;
		lastSampleWeight = norm;

		// Save new contributions for reference
		weight = newWeight;
		currentStamp = stamp;
		currentLuminance = newLuminance;
		copy(samples, samples + requestedSamples, currentSamples);
		copy(sampleStamps, sampleStamps + requestedSamples, currentSampleStamps);
		currentSampleResult = sampleResults;

		consecRejects = 0;
	} else {
		/*if (!cooldown)
			printf("\t\tREJECTED !\n");*/
		
		// Add contribution of new sample before rejecting it
		const float norm = newWeight / (newLuminance * invMeanIntensity + currentLargeMutationProbability);
		if (norm > 0.f) {
			/*if (!cooldown)
				printf("\t\tContrib: (%f, %f, %f) [%f] consecutiveRejects: %d\n",
						sampleResults[0].radiance[0].c[0],
						sampleResults[0].radiance[0].c[1],
						sampleResults[0].radiance[0].c[2],
						norm, consecRejects);*/

			if (film)
				AtomicAddSamplesToFilm(sampleResults, norm);
		}

		lastSampleAcceptance = METRO_REJECTED;
		lastSampleWeight = norm;

		// Restart from previous reference
		stamp = currentStamp;
		copy(currentSamples, currentSamples + requestedSamples, samples);
		copy(currentSampleStamps, currentSampleStamps + requestedSamples, sampleStamps);

		++consecRejects;
	}

	// Cooldown is used in order to not have problems in the estimation of meanIntensity
	// when large mutation probability is very small.
	if (cooldown) {
		// Check if it is time to end the cooldown (i.e. I have an average of
		// 1 sample for each pixel).
		const u_int pixelCount = film ? (film->GetWidth() * film->GetHeight()) : 8192;
		if (sharedData->sampleCount > pixelCount) {
			cooldown = false;
			isLargeMutation = (rndGen->floatValue() < currentLargeMutationProbability);
		} else
			isLargeMutation = (rndGen->floatValue() < .5f);
	} else
		isLargeMutation = (rndGen->floatValue() < currentLargeMutationProbability);

	if (isLargeMutation) {
		stamp = 1;
		fill(sampleStamps, sampleStamps + requestedSamples, 0);
	} else
		++stamp;
}

// Used, most of the times, when not having a film
MetropolisSampleType MetropolisSampler::GetLastSampleAcceptance(float &weight) const {
	weight = lastSampleWeight;
	
	return lastSampleAcceptance;
}

Properties MetropolisSampler::ToProperties() const {
	return Sampler::ToProperties() <<
			Property("sampler.metropolis.largesteprate")(largeMutationProbability) <<
			Property("sampler.metropolis.maxconsecutivereject")(maxRejects) <<
			Property("sampler.metropolis.imagemutationrate")(imageMutationRange);
}

//------------------------------------------------------------------------------
// Static methods used by SamplerRegistry
//------------------------------------------------------------------------------

Properties MetropolisSampler::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("sampler.type")) <<
			cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")) <<
			cfg.Get(GetDefaultProps().Get("sampler.metropolis.largesteprate")) <<
			cfg.Get(GetDefaultProps().Get("sampler.metropolis.maxconsecutivereject")) <<
			cfg.Get(GetDefaultProps().Get("sampler.metropolis.imagemutationrate"));
}

Sampler *MetropolisSampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")).Get<bool>();

	const float rate = Clamp(cfg.Get(GetDefaultProps().Get("sampler.metropolis.largesteprate")).Get<float>(), 0.f, 1.f);
	const u_int reject = cfg.Get(GetDefaultProps().Get("sampler.metropolis.maxconsecutivereject")).Get<u_int>();
	const float mutationRate = Clamp(cfg.Get(GetDefaultProps().Get("sampler.metropolis.imagemutationrate")).Get<float>(), 0.f, 1.f);

	return new MetropolisSampler(rndGen, film, flmSplatter, imageSamplesEnable,
			reject, rate, mutationRate, (MetropolisSamplerSharedData *)sharedData);
}

slg::ocl::Sampler *MetropolisSampler::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Sampler *oclSampler = new slg::ocl::Sampler();

	oclSampler->type = slg::ocl::METROPOLIS;
	oclSampler->metropolis.largeMutationProbability = cfg.Get(GetDefaultProps().Get("sampler.metropolis.largesteprate")).Get<float>();
	oclSampler->metropolis.imageMutationRange = cfg.Get(GetDefaultProps().Get("sampler.metropolis.imagemutationrate")).Get<float>();
	oclSampler->metropolis.maxRejects = cfg.Get(GetDefaultProps().Get("sampler.metropolis.maxconsecutivereject")).Get<u_int>();

	return oclSampler;
}

Film::FilmChannelType MetropolisSampler::GetRequiredChannels(const luxrays::Properties &cfg) {
	return Film::NONE;
}

const Properties &MetropolisSampler::GetDefaultProps() {
	static Properties props = Properties() <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag()) <<
			Property("sampler.metropolis.largesteprate")(.4f) <<
			Property("sampler.metropolis.maxconsecutivereject")(512) <<
			Property("sampler.metropolis.imagemutationrate")(.1f);

	return props;
}
