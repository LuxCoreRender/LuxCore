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

#ifndef _SLG_PMJ02_SAMPLER_H
#define	_SLG_PMJ02_SAMPLER_H

#include <string>
#include <vector>
#include <array>

#include "luxrays/core/randomgen.h"
#include "luxrays/utils/atomic.h"
#include "slg/slg.h"
#include "slg/film/film.h"
#include "slg/samplers/sampler.h"
#include "slg/samplers/pmj02sequence.h"

namespace slg {

//------------------------------------------------------------------------------
// PMJ02SamplerSharedData
//
// Used to share sampler specific data across multiple threads
//------------------------------------------------------------------------------

class PMJ02SamplerSharedData : public SamplerSharedData {
public:
	PMJ02SamplerSharedData(luxrays::RandomGenerator *rndGen, Film *engineFlm);
	PMJ02SamplerSharedData(const u_int seed, Film *engineFlm);
	virtual ~PMJ02SamplerSharedData() { }

	static SamplerSharedData *FromProperties(const luxrays::Properties &cfg,
			luxrays::RandomGenerator *rndGen, Film *film);

	u_int GetNewPixelIndex();

	u_int GetNewPixelPass(const u_int pixelIndex = 0);

	Film *engineFilm;
	u_int seedBase;
	u_int filmRegionPixelCount;

private:
	void Init(const u_int seed, Film *engineFlm);

	luxrays::SpinLock spinLock;
	u_int pixelIndex;

	// Holds the current pass for each pixel when using adaptive sampling
	std::vector<u_int> passPerPixel;
};

//------------------------------------------------------------------------------
// PMJ02Sampler sampler
//------------------------------------------------------------------------------

#define PMJ02_THREAD_WORK_SIZE 4096

class PMJ02Sampler : public Sampler {
public:
	PMJ02Sampler(luxrays::RandomGenerator *rnd, Film *flm,
			const FilmSampleSplatter *flmSplatter, const bool imgSamplesEnable,
			const float adaptiveStrength,
			PMJ02SamplerSharedData *samplerSharedData);
	virtual ~PMJ02Sampler() { }

	virtual SamplerType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }
	virtual void RequestSamples(const SampleType sampleType, const u_int size);

	virtual float GetSample(const u_int index);
	virtual void NextSample(const std::vector<SampleResult> &sampleResults);

	virtual luxrays::Properties ToProperties() const;

	//--------------------------------------------------------------------------
	// Static methods used by SamplerRegistry
	//--------------------------------------------------------------------------

	static SamplerType GetObjectType() { return PMJ02; }
	static std::string GetObjectTag() { return "PMJ02"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Sampler *FromProperties(const luxrays::Properties &cfg, luxrays::RandomGenerator *rndGen,
		Film *film, const FilmSampleSplatter *flmSplatter, SamplerSharedData *sharedData);
	static slg::ocl::Sampler *FromPropertiesOCL(const luxrays::Properties &cfg);
	static Film::FilmChannelType GetRequiredChannels(const luxrays::Properties &cfg);

private:
	void InitNewSample();

	static const luxrays::Properties &GetDefaultProps();

	PMJ02SamplerSharedData *sharedData;
	PMJ02Sequence pmj02sequence;
	float adaptiveStrength;

	float sample0, sample1;
	u_int pixelIndexBase, pixelIndexOffset, pass;

};

}

#endif	/* _SLG_PMJ02_SAMPLER_H */
