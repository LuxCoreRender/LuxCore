/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

namespace slg {

inline double RadicalInverse(u_int n, u_int base) {
	double val = 0.;
	double invBase = 1. / base, invBi = invBase;
	while (n > 0) {
		// Compute next digit of radical inverse
		u_int d_i = (n % base);
		val += d_i * invBi;
		n /= base;
		invBi *= invBase;
	}
	return val;
}

//------------------------------------------------------------------------------
// OpenCL data types
//------------------------------------------------------------------------------

namespace ocl {
#include "slg/sampler/sampler_types.cl"
}

//------------------------------------------------------------------------------
// Sampler
//------------------------------------------------------------------------------

typedef enum {
	RANDOM = 0,
	METROPOLIS = 1,
	SOBOL = 2
} SamplerType;

class Sampler {
public:
	Sampler(luxrays::RandomGenerator *rnd, Film *flm) : rndGen(rnd), film(flm) { }
	virtual ~Sampler() { }

	virtual SamplerType GetType() const = 0;
	virtual void RequestSamples(const u_int size) = 0;

	// index 0 and 1 are always image X and image Y
	virtual float GetSample(const u_int index) = 0;
	virtual void NextSample(const std::vector<SampleResult> &sampleResults) = 0;

	static SamplerType String2SamplerType(const std::string &type);
	static const std::string SamplerType2String(const SamplerType type);

protected:
	void AddSamplesToFilm(const std::vector<SampleResult> &sampleResults, const float weight = 1.f) const {
		for (std::vector<SampleResult>::const_iterator sr = sampleResults.begin(); sr < sampleResults.end(); ++sr)
			film->SplatSample(*sr, weight);
	}

	luxrays::RandomGenerator *rndGen;
	Film *film;
};

//------------------------------------------------------------------------------
// Random sampler
//------------------------------------------------------------------------------

class RandomSampler : public Sampler {
public:
	RandomSampler(luxrays::RandomGenerator *rnd, Film *flm) : Sampler(rnd, flm) { }
	virtual ~RandomSampler() { }

	virtual SamplerType GetType() const { return RANDOM; }
	virtual void RequestSamples(const u_int size) { }

	virtual float GetSample(const u_int index) { return rndGen->floatValue(); }
	virtual void NextSample(const std::vector<SampleResult> &sampleResults);
};

//------------------------------------------------------------------------------
// Metropolis sampler
//------------------------------------------------------------------------------

class MetropolisSampler : public Sampler {
public:
	MetropolisSampler(luxrays::RandomGenerator *rnd, Film *film, const u_int maxRej,
			const float pLarge, const float imgRange,
			double *sharedTotalLuminance, double *sharedSampleCount);
	virtual ~MetropolisSampler();

	virtual SamplerType GetType() const { return METROPOLIS; }
	virtual void RequestSamples(const u_int size);

	virtual float GetSample(const u_int index);
	virtual void NextSample(const std::vector<SampleResult> &sampleResults);

private:
	u_int maxRejects;
	float largeMutationProbability, imageMutationRange;

	// I'm storing totalLuminance and sampleCount on external (shared) variables
	// in order to have far more accurate estimation in the image mean intensity
	// computation
	double *sharedTotalLuminance, *sharedSampleCount;

	u_int sampleSize;
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

	bool isLargeMutation, cooldown;
};

//------------------------------------------------------------------------------
// Sobol sampler
//
// This sampler is based on Blender Cycles Sobol implementation.
//------------------------------------------------------------------------------

#define SOBOL_STARTOFFSET 32

extern void SobolGenerateDirectionVectors(u_int *vectors, const u_int dimensions);

class SobolSampler : public Sampler {
public:
	SobolSampler(luxrays::RandomGenerator *rnd, Film *flm) : Sampler(rnd, flm),
			directions(NULL), rng0(rnd->floatValue()), rng1(rnd->floatValue()),
			pass(SOBOL_STARTOFFSET) { }
	virtual ~SobolSampler() { delete directions; }

	virtual SamplerType GetType() const { return SOBOL; }
	virtual void RequestSamples(const u_int size);

	virtual float GetSample(const u_int index);
	virtual void NextSample(const std::vector<SampleResult> &sampleResults);

private:
	u_int SobolDimension(const u_int index, const u_int dimension) const;

	u_int *directions;

	float rng0, rng1;
	u_int pass;
};

}

#endif	/* _SLG_SAMPLER_H */
