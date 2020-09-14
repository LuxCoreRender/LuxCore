/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#include <atomic>

#include <boost/lexical_cast.hpp>

#include "luxrays/core/color/color.h"
#include "luxrays/utils/atomic.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/metropolis.h"
#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// MetropolisSamplerSharedData
//------------------------------------------------------------------------------

MetropolisSamplerSharedData::MetropolisSamplerSharedData() : SamplerSharedData() {
	Reset();
}

SamplerSharedData *MetropolisSamplerSharedData::FromProperties(const Properties &cfg,
		RandomGenerator *rndGen, Film *film) {
	return new MetropolisSamplerSharedData();
}

void MetropolisSamplerSharedData::Reset() {
	totalLuminance = 0.f;
	sampleCount = 0;
	noBlackSampleCount = 0;

	lastLuminance = 0.f;
	lastSampleCount = 0;
	lastNoBlackSampleCount = 0;

	invLuminance = 1.f;
	cooldown = true;
}

//------------------------------------------------------------------------------
// Metropolis sampler
//------------------------------------------------------------------------------

MetropolisSampler::MetropolisSampler(RandomGenerator *rnd, Film *flm,
		const FilmSampleSplatter *flmSplatter, const bool imgSamplesEnable,
		const u_int maxRej, const float pLarge, const float imgRange, const bool addOnlyCstcs,
		MetropolisSamplerSharedData *samplerSharedData) : Sampler(rnd, flm, flmSplatter, imgSamplesEnable),
		sharedData(samplerSharedData),
		maxRejects(maxRej),	largeMutationProbability(pLarge), imageMutationRange(imgRange),
		addOnlyCuastics(addOnlyCstcs),
		samples(NULL), sampleStamps(NULL), currentSamples(NULL), currentSampleStamps(NULL),
		largeMutationCount(0) {
}

MetropolisSampler::~MetropolisSampler() {
	delete[] samples;
	delete[] sampleStamps;
	delete[] currentSamples;
	delete[] currentSampleStamps;
}

// Mutate a value in the range [0-1]
//
// The original version used in old LuxRender
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

// Mutate a value in the range [0-1]
//
// Original version from paper "A Simple and Robust Mutation Strategy for the
// Metropolis Light Transport Algorithm"
/*static float Mutate(const float x, const float randomValue) {
	static const float s1 = 1.f / 1024.f;
	static const float s2 = 1.f / 64.f;

	const float dx = s2 * expf(-logf(s2 / s1) * randomValue);

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
}*/

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
	currentSampleResults.resize(0);
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
	//--------------------------------------------------------------------------
	// Some hard coded parameter:
	//--------------------------------------------------------------------------
	const u_longlong warmupMinSampleCount = 250000;
	const u_longlong warmupMinNoBlackSampleCount = warmupMinSampleCount / 100 * 5; // 5%

	const u_longlong stepMinSampleCount = 250000;
	const u_longlong stepMinNoBlackSampleCount = warmupMinSampleCount / 100 * 5; // 5%

	const double luminanceThreshold = .01; // 1%	// Thread 0 check if the cooldown is over
	//--------------------------------------------------------------------------

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
				throw runtime_error("Unknown sample type in MetropolisSampler::NextSample(): " + ToString(sampleType));
		}
		film->AddSampleCount(threadIndex, pixelNormalizedCount, screenNormalizedCount);
	}

	// Calculate the sample result luminance
	float newLuminance = 0.f;
	for (vector<SampleResult>::const_iterator sr = sampleResults.begin(); sr != sampleResults.end(); ++sr) {
		if (sr->HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED)) {
			for (u_int i = 0; i < sr->radiance.Size(); ++i) {
				const float luminance = sr->radiance[i].Y();
				assert (!isnan(luminance) && !isinf(luminance) && (luminance >= 0.f));

				if ((luminance > 0.f) && !isnan(luminance) && !isinf(luminance))
					newLuminance += luminance;
			}
		}

		if (sr->HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED)) {
			for (u_int i = 0; i < sr->radiance.Size(); ++i) {
				const float luminance = sr->radiance[i].Y();
				assert (!isnan(luminance) && !isinf(luminance) && (luminance >= 0.f));

				if ((luminance > 0.f) && !isnan(luminance) && !isinf(luminance))
					newLuminance += luminance;
			}
		}
	}
	
	if (sharedData->cooldown && isLargeMutation) {
		AtomicAdd(&sharedData->totalLuminance, (double)newLuminance);
		sharedData->sampleCount++;
		if (newLuminance > 0.f)
			sharedData->noBlackSampleCount++;
	}

	const float invMeanIntensity = sharedData->invLuminance;

	// Define the probability of large mutations.
	const float currentLargeMutationProbability = (sharedData->cooldown) ? .5 : largeMutationProbability;

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

			if (film) {
				for (auto const &sr : currentSampleResults) {
					if (!addOnlyCuastics || (sr.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED) && sr.isCaustic))
						AtomicAddSampleToFilm(sr, norm);
				}
			}
		}

		lastSampleAcceptance = METRO_ACCEPTED;
		lastSampleWeight = norm;

		// Save new contributions for reference
		weight = newWeight;
		currentStamp = stamp;
		currentLuminance = newLuminance;
		copy(samples, samples + requestedSamples, currentSamples);
		copy(sampleStamps, sampleStamps + requestedSamples, currentSampleStamps);
		currentSampleResults = sampleResults;

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

			if (film) {
				for (auto const &sr : sampleResults) {
					if (!addOnlyCuastics || (sr.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED) && sr.isCaustic))
						AtomicAddSampleToFilm(sr, norm);
				}
			}
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
	if (threadIndex == 0) {
		// Update shared inv. luminance
		const double luminance = (sharedData->totalLuminance > 0.) ?
			(sharedData->totalLuminance / sharedData->sampleCount) : 1.;
		sharedData->invLuminance = (float)(1. / luminance);
	
		/*SLG_LOG("Step: " << sharedData->sampleCount <<  "/" << sharedData->noBlackSampleCount <<
				" Luminance: " << luminance);*/

		const u_longlong sampleCount = sharedData->sampleCount;
		const u_longlong noBlackSampleCount = sharedData->noBlackSampleCount;
		const u_longlong lastSampleCount = sharedData->lastSampleCount;
		const u_longlong lastNoBlackSampleCount = sharedData->lastNoBlackSampleCount;
		const double lastLuminance = sharedData->lastLuminance;

		if (
			// Warmup period
			((sampleCount > warmupMinSampleCount) &&
				(noBlackSampleCount > warmupMinNoBlackSampleCount)) &&
			// Step period
			((sampleCount - lastSampleCount > stepMinSampleCount) &&
				(noBlackSampleCount - lastNoBlackSampleCount > stepMinNoBlackSampleCount))
			) {
			// Time to check if I can end the cooldown
			const double deltaLuminance = fabs(luminance  - lastLuminance) / luminance;

			SLG_LOG("Metropolis sampler image luminance estimation: Step[" << sampleCount <<  "/" << noBlackSampleCount <<
					"] Luminance[" << luminance << "] Delta[" << (deltaLuminance * 100.) << "%]");

			if (
				// To avoid any kind of overflow
				(sampleCount > 0xefffffffu) ||
				// Check if the delta estimated luminance is small enough
				((luminance > 0.) && (deltaLuminance < luminanceThreshold))
				) {
				// I can end the cooldown phase
				SLG_LOG("Metropolis sampler estimated image luminance: " << luminance << " (" << (deltaLuminance * 100.) << "%)");

				sharedData->cooldown = false;
			}

			sharedData->lastSampleCount = sampleCount;
			sharedData->lastNoBlackSampleCount = noBlackSampleCount;
			sharedData->lastLuminance = luminance;
		}
	}

	isLargeMutation = (rndGen->floatValue() < currentLargeMutationProbability);
	if (isLargeMutation) {
		stamp = 1;
		fill(sampleStamps, sampleStamps + requestedSamples, 0);
		
		++largeMutationCount;
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
			Property("sampler.metropolis.imagemutationrate")(imageMutationRange) <<
			Property("sampler.metropolis.addonlycaustics")(addOnlyCuastics);
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
			cfg.Get(GetDefaultProps().Get("sampler.metropolis.imagemutationrate")) <<
			cfg.Get(GetDefaultProps().Get("sampler.metropolis.addonlycaustics"));
}

Sampler *MetropolisSampler::FromProperties(const Properties &cfg, RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData) {
	const bool imageSamplesEnable = cfg.Get(GetDefaultProps().Get("sampler.imagesamples.enable")).Get<bool>();

	const float rate = Clamp(cfg.Get(GetDefaultProps().Get("sampler.metropolis.largesteprate")).Get<float>(), 0.f, 1.f);
	const u_int reject = cfg.Get(GetDefaultProps().Get("sampler.metropolis.maxconsecutivereject")).Get<u_int>();
	const float mutationRate = Clamp(cfg.Get(GetDefaultProps().Get("sampler.metropolis.imagemutationrate")).Get<float>(), 0.f, 1.f);
	const bool addOnlyCaustics = cfg.Get(GetDefaultProps().Get("sampler.metropolis.addonlycaustics")).Get<bool>();

	return new MetropolisSampler(rndGen, film, flmSplatter, imageSamplesEnable,
			reject, rate, mutationRate, addOnlyCaustics,
			(MetropolisSamplerSharedData *)sharedData);
}

slg::ocl::Sampler *MetropolisSampler::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Sampler *oclSampler = new slg::ocl::Sampler();

	oclSampler->type = slg::ocl::METROPOLIS;
	oclSampler->metropolis.largeMutationProbability = cfg.Get(GetDefaultProps().Get("sampler.metropolis.largesteprate")).Get<float>();
	oclSampler->metropolis.imageMutationRange = cfg.Get(GetDefaultProps().Get("sampler.metropolis.imagemutationrate")).Get<float>();
	oclSampler->metropolis.maxRejects = cfg.Get(GetDefaultProps().Get("sampler.metropolis.maxconsecutivereject")).Get<u_int>();

	return oclSampler;
}

void MetropolisSampler::AddRequiredChannels(Film::FilmChannels &channels, const luxrays::Properties &cfg) {
	// No additional channels required
}

const Properties &MetropolisSampler::GetDefaultProps() {
	static Properties props = Properties() <<
			Sampler::GetDefaultProps() <<
			Property("sampler.type")(GetObjectTag()) <<
			Property("sampler.metropolis.largesteprate")(.4f) <<
			Property("sampler.metropolis.maxconsecutivereject")(512) <<
			Property("sampler.metropolis.imagemutationrate")(.1f) <<
			Property("sampler.metropolis.addonlycaustics")(false);

	return props;
}
