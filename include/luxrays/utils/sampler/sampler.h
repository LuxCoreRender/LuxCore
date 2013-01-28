/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _LUXRAYS_SAMPLER_H
#define	_LUXRAYS_SAMPLER_H

#include <string>
#include <vector>

#include "luxrays/luxrays.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/utils/film/film.h"

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCL data types
//------------------------------------------------------------------------------

namespace ocl {
#include "luxrays/utils/sampler/sampler_types.cl"
} 

//------------------------------------------------------------------------------
// Sampler
//------------------------------------------------------------------------------

namespace utils {

typedef struct {
	FilmBufferType type;
	float screenX, screenY;
	Spectrum radiance;
	float alpha;
} SampleResult;

inline void AddSampleResult(std::vector<SampleResult> &sampleResults, const FilmBufferType type,
	const float screenX, const float screenY, const Spectrum &radiance, const float alpha) {
	SampleResult sr;
	sr.type = type;
	sr.screenX = screenX;
	sr.screenY = screenY;
	sr.radiance = radiance;
	sr.alpha = alpha;

	sampleResults.push_back(sr);
}

typedef enum {
	RANDOM = 0,
	METROPOLIS = 1,
	SOBOL = 2
} SamplerType;

class Sampler {
public:
	Sampler(RandomGenerator *rnd, Film *flm) : rndGen(rnd), film(flm) { }
	virtual ~Sampler() { }

	virtual SamplerType GetType() const = 0;
	virtual void RequestSamples(const u_int size) = 0;

	// index 0 and 1 are always image X and image Y
	virtual float GetSample(const u_int index) = 0;
	virtual void NextSample(const std::vector<SampleResult> &sampleResults) = 0;

	static SamplerType String2SamplerType(const std::string &type);
	static const std::string SamplerType2String(const SamplerType type);

protected:
	RandomGenerator *rndGen;
	Film *film;
};

//------------------------------------------------------------------------------
// Random sampler
//------------------------------------------------------------------------------

class RandomSampler : public Sampler {
public:
	RandomSampler(RandomGenerator *rnd, Film *flm) : Sampler(rnd, flm) { }
	virtual ~RandomSampler() { }

	virtual SamplerType GetType() const { return RANDOM; }
	virtual void RequestSamples(const u_int size) { }

	virtual float GetSample(const u_int index) { return rndGen->floatValue(); }
	virtual void NextSample(const std::vector<SampleResult> &sampleResults) {
		film->AddSampleCount(1.0);

		for (std::vector<SampleResult>::const_iterator sr = sampleResults.begin(); sr < sampleResults.end(); ++sr)
			film->SplatFiltered(sr->type, sr->screenX, sr->screenY, sr->radiance, sr->alpha);
	}
};

//------------------------------------------------------------------------------
// Metropolis sampler
//------------------------------------------------------------------------------

class MetropolisSampler : public Sampler {
public:
	MetropolisSampler(RandomGenerator *rnd, Film *film, const u_int maxRej,
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

extern void SobolGenerateDirectionVectors(u_int *vectors, const u_int dimensions);

class SobolSampler : public Sampler {
public:
	SobolSampler(RandomGenerator *rnd, Film *flm) : Sampler(rnd, flm),
			directions(NULL), rng0(rnd->floatValue()), rng1(rnd->floatValue()), pass(0) { }
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

} }

#endif	/* _LUXRAYS_SDL_BSDF_H */
