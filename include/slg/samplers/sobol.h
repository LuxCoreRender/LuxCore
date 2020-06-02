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

#ifndef _SLG_SOBOL_SAMPLER_H
#define	_SLG_SOBOL_SAMPLER_H

#include "luxrays/core/randomgen.h"
#include "luxrays/utils/atomic.h"

#include "slg/slg.h"
#include "slg/film/film.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/sobolsequence.h"

namespace slg {

//------------------------------------------------------------------------------
// SobolSamplerSharedData
//
// Used to share sampler specific data across multiple threads
//------------------------------------------------------------------------------

class SobolSamplerSharedData : public SamplerSharedData {
public:
	SobolSamplerSharedData(luxrays::RandomGenerator *rndGen, Film *engineFlm);
	SobolSamplerSharedData(const u_int seed, Film *engineFlm);
	virtual ~SobolSamplerSharedData() { }

	virtual void Reset();

	void GetNewBucket(const u_int bucketCount, u_int *bucketIndex, u_int *seed);
	u_int GetNewPixelPass(const u_int pixelIndex = 0);
	
	static SamplerSharedData *FromProperties(const luxrays::Properties &cfg,
			luxrays::RandomGenerator *rndGen, Film *film);

	Film *engineFilm;
	u_int seedBase;
	u_int filmRegionPixelCount;

private:
	u_int bucketIndex;

	// Holds the current pass for each pixel when using adaptive sampling
	std::vector<u_int> passPerPixel;

};

//------------------------------------------------------------------------------
// Sobol sampler
//
// This sampler is based on Blender Cycles Sobol implementation.
//------------------------------------------------------------------------------

class SobolSampler : public Sampler {
public:
	SobolSampler(luxrays::RandomGenerator *rnd, Film *flm,
			const FilmSampleSplatter *flmSplatter, const bool imgSamplesEnable,
			const float adaptiveStr, const float adaptiveUserImpWeight,
			const u_int bucketSize, const u_int tileSize, const u_int superSampling,
			const u_int overlapping, SobolSamplerSharedData *samplerSharedData);
	virtual ~SobolSampler();

	virtual SamplerType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }
	virtual void RequestSamples(const SampleType sampleType, const u_int size);

	virtual float GetSample(const u_int index);
	virtual void NextSample(const std::vector<SampleResult> &sampleResults);

	virtual luxrays::Properties ToProperties() const;

	//--------------------------------------------------------------------------
	// Static methods used by SamplerRegistry
	//--------------------------------------------------------------------------

	static SamplerType GetObjectType() { return SOBOL; }
	static std::string GetObjectTag() { return "SOBOL"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Sampler *FromProperties(const luxrays::Properties &cfg, luxrays::RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData);
	static slg::ocl::Sampler *FromPropertiesOCL(const luxrays::Properties &cfg);
	static Film::FilmChannelType GetRequiredChannels(const luxrays::Properties &cfg);

private:
	void InitNewSample();
	float GetSobolSample(const u_int index);

	static const luxrays::Properties &GetDefaultProps();

	SobolSamplerSharedData *sharedData;
	SobolSequence sobolSequence;
	float adaptiveStrength, adaptiveUserImportanceWeight;
	u_int bucketSize, tileSize, superSampling, overlapping;

	u_int bucketIndex, pixelOffset, passOffset, pass;
	luxrays::TauswortheRandomGenerator rngGenerator;

	float sample0, sample1;
};

}

#endif	/* _SLG_SOBOL_SAMPLER_H */
