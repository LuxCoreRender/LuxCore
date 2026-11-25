/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
#include <memory>

#include "luxrays/core/randomgen.h"
#include "luxrays/usings.h"
#include "luxrays/utils/atomic.h"

#include "slg/slg.h"
#include "slg/usings.h"
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
	// Constructors
	// Note that film is optional for this object
	SobolSamplerSharedData(const luxrays::RandomGeneratorUPtr & rndGen, FilmPtr engineFlm);
	SobolSamplerSharedData(const u_int seed, FilmPtr engineFlm);
	virtual ~SobolSamplerSharedData() { }

	virtual void Reset();

	std::tuple<u_int, u_int> GetNewBucket(u_int bucketCount);

	u_int GetNewPixelPass(const u_int pixelIndex = 0);

	u_int GetPassCount(const u_int bucketCount) const;

	static std::unique_ptr<SamplerSharedData> FromProperties(
		const luxrays::Properties &cfg,
		const luxrays::RandomGeneratorUPtr & rndGen,
		FilmPtr film
	);

	FilmRef GetEngineFilm() { return *engineFilm; }
	FilmConstRef GetEngineFilm() const { return *engineFilm; }
	bool HasEngineFilm() { return bool(engineFilm); }

	std::shared_ptr<u_int> seedBase;
	u_int filmRegionPixelCount;

private:
	std::shared_ptr<u_int> bucketIndex;  // This can potentially be accessed by
										 // multiple objects, so we share
										 // ownership with all to avoid
										 // heap-use-after-free
	FilmPtr engineFilm;

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

	SobolSampler(
		const luxrays::RandomGeneratorUPtr & rnd,
		FilmPtr flm,
		const FilmSampleSplatterUPtr& flmSplatter,
		const bool imgSamplesEnable,
		const float adaptiveStr,
		const float adaptiveUserImpWeight,
		const u_int bucketSize,
		const u_int tileSize,
		const u_int superSampling,
		const u_int overlapping,
		SamplerSharedDataSPtr samplerSharedData
	);
	SobolSampler(
		const luxrays::RandomGeneratorUPtr & rnd,
		FilmRef flm,
		const FilmSampleSplatterUPtr& flmSplatter,
		const bool imgSamplesEnable,
		const float adaptiveStr,
		const float adaptiveUserImpWeight,
		const u_int bucketSize,
		const u_int tileSize,
		const u_int superSampling,
		const u_int overlapping,
		SamplerSharedDataSPtr samplerSharedData
	);
	virtual ~SobolSampler();

	virtual SamplerType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }
	virtual void RequestSamples(const SampleType sampleType, const u_int size);

	virtual float GetSample(const u_int index);
	virtual void NextSample(const std::vector<SampleResult> &sampleResults);

	virtual luxrays::PropertiesUPtr ToProperties() const;

	u_int GetPassCount() const;

	//--------------------------------------------------------------------------
	// Static methods used by SamplerRegistry
	//--------------------------------------------------------------------------

	static SamplerType GetObjectType() { return SOBOL; }
	static std::string GetObjectTag() { return "SOBOL"; }
	static luxrays::PropertiesUPtr ToProperties(const luxrays::Properties &cfg);
	static SamplerUPtr FromProperties(
		const luxrays::Properties &cfg,
		const luxrays::RandomGeneratorUPtr & rndGen,
		FilmPtr film, const FilmSampleSplatterUPtr& flmSplatter,
		SamplerSharedDataSPtr sharedData
	);
	static slg::ocl::Sampler *FromPropertiesOCL(const luxrays::Properties &cfg);
	static void AddRequiredChannels(Film::FilmChannels &channels, const luxrays::Properties &cfg);

private:
	void InitNewSample();
	float GetSobolSample(const u_int index);

	static luxrays::PropertiesUPtr GetDefaultProps();

	std::shared_ptr<SobolSamplerSharedData> sharedData;
	SobolSequence sobolSequence;
	float adaptiveStrength, adaptiveUserImportanceWeight;
	u_int bucketSize, tileSize, superSampling, overlapping;

	std::shared_ptr<u_int> bucketIndex;
	u_int pixelOffset, passOffset, pass;
	luxrays::TauswortheRandomGenerator rngGenerator;

	float sample0, sample1;
};

}

#endif	/* _SLG_SOBOL_SAMPLER_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
