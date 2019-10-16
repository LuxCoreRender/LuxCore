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

#ifndef _SLG_pmj02_SAMPLER_H
#define	_SLG_pmj02_SAMPLER_H

#include <string>
#include <vector>
#include <array>

#include "luxrays/core/randomgen.h"
#include "luxrays/utils/atomic.h"
#include "slg/slg.h"
#include "slg/film/film.h"
#include "slg/samplers/sampler.h"

namespace slg {

//------------------------------------------------------------------------------
// PMJ02SamplerSharedData
//
// Used to share sampler specific data across multiple threads
//------------------------------------------------------------------------------

class PMJ02SamplerSharedData : public SamplerSharedData {
public:
	PMJ02SamplerSharedData(Film *engineFilm);
	virtual ~PMJ02SamplerSharedData() { }

	u_int GetNewPixelIndex();
	
	static SamplerSharedData *FromProperties(const luxrays::Properties &cfg,
			luxrays::RandomGenerator *rndGen, Film *film);

	Film *engineFilm;
	u_int filmRegionPixelCount;

private:
	luxrays::SpinLock spinLock;
	u_int pixelIndex;
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
	float adaptiveStrength;

	float sample0, sample1;
	u_int pixelIndexBase, pixelIndexOffset;

	struct float2 {
		float x;
		float y;
	};

	void generate_2D(float2 points[], u_int size, luxrays::RandomGenerator *rndGen);
	void mark_occupied_strata(float2 points[], u_int N);
	void generate_sample_point(float2 points[], float i, float j, float xhalf, float yhalf, u_int n, u_int N);
	void extend_sequence_even(float2 points[], u_int N);
	void extend_sequence_odd(float2 points[], u_int N);
	void mark_occupied_strata1(float2 pt, u_int NN);
	bool is_occupied(float2 pt, u_int NN);
	void shuffle(float2 points[], u_int size);
	
	std::vector<std::vector<bool>> occupiedStrata;
	std::vector<bool> occupied1Dx, occupied1Dy;

	// Hardcode that for now
	u_int num_samples = 4096;

};

}

#endif	/* _SLG_pmj02_SAMPLER_H */
