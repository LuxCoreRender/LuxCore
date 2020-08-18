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

#ifndef _SLG_METROPOLIS_SAMPLER_H
#define	_SLG_METROPOLIS_SAMPLER_H

#include <string>
#include <vector>
#include <atomic>

#include "luxrays/core/randomgen.h"
#include "slg/slg.h"
#include "slg/film/film.h"
#include "slg/samplers/sampler.h"

namespace slg {

//------------------------------------------------------------------------------
// MetropolisSamplerSharedData
//
// Used to share sampler specific data across multiple threads
//------------------------------------------------------------------------------

class MetropolisSamplerSharedData : public SamplerSharedData {
public:
	MetropolisSamplerSharedData();
	virtual ~MetropolisSamplerSharedData() { }

	virtual void Reset();

	static SamplerSharedData *FromProperties(const luxrays::Properties &cfg,
			luxrays::RandomGenerator *rndGen, Film *film);

	// I'm storing totalLuminance, sampleCount and noBlackSampleCount on shared variables
	// in order to have far more accurate estimation in the image mean intensity
	// computation
	
	// Updated by all threads
	std::atomic<double> totalLuminance;
	std::atomic<u_longlong> sampleCount, noBlackSampleCount;

	// Updated only by thread 0
	float lastLuminance;
	u_int lastSampleCount, lastNoBlackSampleCount;

	// Updated only by thread 0 but read by all threads
	float invLuminance;
	bool cooldown;
};

//------------------------------------------------------------------------------
// Metropolis sampler
//------------------------------------------------------------------------------

typedef enum {
	METRO_ACCEPTED, METRO_REJECTED
} MetropolisSampleType;

class MetropolisSampler : public Sampler {
public:
	MetropolisSampler(luxrays::RandomGenerator *rnd, Film *film,
			const FilmSampleSplatter *flmSplatter, const bool imgSamplesEnable,
			const u_int maxRej, const float pLarge, const float imgRange,
			MetropolisSamplerSharedData *samplerSharedData);
	virtual ~MetropolisSampler();

	virtual SamplerType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }
	virtual void RequestSamples(const SampleType sampleType, const u_int size);

	virtual float GetSample(const u_int index);
	virtual void NextSample(const std::vector<SampleResult> &sampleResults);

	// Used, most of the times, when not having a film
	MetropolisSampleType GetLastSampleAcceptance(float &weight) const;

	virtual luxrays::Properties ToProperties() const;

	u_int GetLargeMutationCount() const { return largeMutationCount; }
	
	//--------------------------------------------------------------------------
	// Static methods used by SamplerRegistry
	//--------------------------------------------------------------------------

	static SamplerType GetObjectType() { return METROPOLIS; }
	static std::string GetObjectTag() { return "METROPOLIS"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Sampler *FromProperties(const luxrays::Properties &cfg, luxrays::RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData);
	static slg::ocl::Sampler *FromPropertiesOCL(const luxrays::Properties &cfg);
	static void AddRequiredChannels(Film::FilmChannels &channels, const luxrays::Properties &cfg);

private:
	static const luxrays::Properties &GetDefaultProps();

	MetropolisSamplerSharedData *sharedData;

	u_int maxRejects;
	float largeMutationProbability, imageMutationRange;

	float *samples;
	u_int *sampleStamps;

	float weight;
	u_int consecRejects;
	u_int stamp;

	// Data saved for the current sample
	u_int currentStamp;
	double currentLuminance;
	float *currentSamples;
	u_int *currentSampleStamps;
	std::vector<SampleResult> currentSampleResult;

	// Used, most of the times, when not having a film
	MetropolisSampleType lastSampleAcceptance;
	float lastSampleWeight;

	u_int largeMutationCount;
	
	bool isLargeMutation;
};

}

#endif	/* _SLG_METROPOLIS_SAMPLER_H */
