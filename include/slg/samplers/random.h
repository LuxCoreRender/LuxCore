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

#ifndef _SLG_RANDOM_SAMPLER_H
#define	_SLG_RANDOM_SAMPLER_H

#include <string>
#include <vector>

#include "luxrays/core/randomgen.h"
#include "luxrays/utils/atomic.h"
#include "slg/slg.h"
#include "slg/film/film.h"
#include "slg/samplers/sampler.h"

namespace slg {

//------------------------------------------------------------------------------
// RandomSamplerSharedData
//
// Used to share sampler specific data across multiple threads
//------------------------------------------------------------------------------

class RandomSamplerSharedData : public SamplerSharedData {
public:
	RandomSamplerSharedData(Film *engineFilm);
	virtual ~RandomSamplerSharedData() { }

	virtual void Reset();

	void GetNewBucket(const u_int bucketCount, u_int *newBucketIndex);
	
	static SamplerSharedData *FromProperties(const luxrays::Properties &cfg,
			luxrays::RandomGenerator *rndGen, Film *film);

	Film *engineFilm;

private:
	u_int bucketIndex;
};

//------------------------------------------------------------------------------
// Random sampler
//------------------------------------------------------------------------------

class RandomSampler : public Sampler {
public:
	RandomSampler(luxrays::RandomGenerator *rnd, Film *flm,
			const FilmSampleSplatter *flmSplatter, const bool imgSamplesEnable,
			const float adaptiveStrength, const float adaptiveUserImpWeight,
			const u_int bucketSize, const u_int tileSize, const u_int superSampling,
			const u_int overlapping, RandomSamplerSharedData *samplerSharedData);
	virtual ~RandomSampler() { }

	virtual SamplerType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }
	virtual void RequestSamples(const SampleType sampleType, const u_int size);

	virtual float GetSample(const u_int index);
	virtual void NextSample(const std::vector<SampleResult> &sampleResults);

	virtual luxrays::Properties ToProperties() const;

	//--------------------------------------------------------------------------
	// Static methods used by SamplerRegistry
	//--------------------------------------------------------------------------

	static SamplerType GetObjectType() { return RANDOM; }
	static std::string GetObjectTag() { return "RANDOM"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Sampler *FromProperties(const luxrays::Properties &cfg, luxrays::RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData);
	static slg::ocl::Sampler *FromPropertiesOCL(const luxrays::Properties &cfg);
	static void AddRequiredChannels(Film::FilmChannels &channels, const luxrays::Properties &cfg);

private:
	void InitNewSample();

	static const luxrays::Properties &GetDefaultProps();
	
	RandomSamplerSharedData *sharedData;
	float adaptiveStrength, adaptiveUserImportanceWeight;
	u_int bucketSize, tileSize, superSampling, overlapping;

	float sample0, sample1;
	u_int bucketIndex, pixelOffset, passOffset, pass;
};

}

#endif	/* _SLG_RANDOM_SAMPLER_H */
