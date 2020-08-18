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

#ifndef _SLG_SAMPLER_H
#define	_SLG_SAMPLER_H

#include <string>
#include <vector>

#include "luxrays/core/randomgen.h"
#include "slg/slg.h"
#include "slg/film/film.h"
#include "slg/film/filmsamplesplatter.h"
#include "slg/film/sampleresult.h"

namespace slg {

//------------------------------------------------------------------------------
// OpenCL data types
//------------------------------------------------------------------------------

namespace ocl {
#include "slg/samplers/sampler_types.cl"
}

//------------------------------------------------------------------------------
// SamplerSharedData
//
// Used to share sampler specific data across multiple threads
//------------------------------------------------------------------------------

class SamplerSharedDataRegistry;

class SamplerSharedData {
public:
	SamplerSharedData() { }
	virtual ~SamplerSharedData() { }

	virtual void Reset() = 0;

	static SamplerSharedData *FromProperties(const luxrays::Properties &cfg,
			luxrays::RandomGenerator *rndGen, Film *film);
};

//------------------------------------------------------------------------------
// Sampler
//------------------------------------------------------------------------------

typedef enum {
	RANDOM, METROPOLIS, SOBOL, RTPATHCPUSAMPLER, TILEPATHSAMPLER,
	SAMPLER_TYPE_COUNT
} SamplerType;

typedef enum {
	PIXEL_NORMALIZED_ONLY, SCREEN_NORMALIZED_ONLY, PIXEL_NORMALIZED_AND_SCREEN_NORMALIZED
} SampleType;

class Sampler : public luxrays::NamedObject {
public:
	Sampler(luxrays::RandomGenerator *rnd, Film *flm,
			const FilmSampleSplatter *flmSplatter,
			const bool imgSamplesEnable) : NamedObject("sampler"), 
			threadIndex(0), rndGen(rnd), film(flm), filmSplatter(flmSplatter),
			imageSamplesEnable(imgSamplesEnable) { }
	virtual ~Sampler() { }

	virtual void SetThreadIndex(const u_int index) { threadIndex = index; }

	virtual SamplerType GetType() const = 0;
	virtual std::string GetTag() const = 0;
	virtual void RequestSamples(const SampleType sampleType, const u_int size);

	// index 0 and 1 are always image X and image Y
	virtual float GetSample(const u_int index) = 0;
	virtual void NextSample(const std::vector<SampleResult> &sampleResults) = 0;

	// Transform the current object in Properties
	virtual luxrays::Properties ToProperties() const;

	//--------------------------------------------------------------------------
	// Static methods used by SamplerRegistry
	//--------------------------------------------------------------------------

	// Transform the current configuration Properties in a complete list of
	// object Properties (including all defaults values)
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	// Allocate a Object based on the cfg definition
	static Sampler *FromProperties(const luxrays::Properties &cfg, luxrays::RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData);
	static slg::ocl::Sampler *FromPropertiesOCL(const luxrays::Properties &cfg);

	static void AddRequiredChannels(Film::FilmChannels &channels, const luxrays::Properties &cfg);

	static SamplerType String2SamplerType(const std::string &type);
	static std::string SamplerType2String(const SamplerType type);

protected:
	static const luxrays::Properties &GetDefaultProps();

	void AtomicAddSamplesToFilm(const std::vector<SampleResult> &sampleResults, const float weight = 1.f) const;

	u_int threadIndex;
	luxrays::RandomGenerator *rndGen;
	Film *film;
	const FilmSampleSplatter *filmSplatter;
	
	SampleType sampleType;
	u_int requestedSamples;
	// If samples 0 and 1 should be expressed in pixels
	bool imageSamplesEnable;
};

}

#endif	/* _SLG_SAMPLER_H */
