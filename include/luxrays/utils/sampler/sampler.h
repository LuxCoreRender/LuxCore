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

#include "luxrays/luxrays.h"
#include "luxrays/utils/core/randomgen.h"
#include "luxrays/utils/film/film.h"

namespace luxrays { namespace utils {

typedef struct {
	FilmBufferType type;
	float screenX, screenY;
	Spectrum radiance;
	float alpha;
} SampleResult;

inline void AddSampleResult(vector<SampleResult> &sampleResults, const FilmBufferType type,
	const float screenX, const float screenY, const Spectrum &radiance, const float alpha) {
	SampleResult sr;
	sr.type = type;
	sr.screenX = screenX;
	sr.screenY = screenY;
	sr.radiance = radiance;
	sr.alpha = alpha;

	sampleResults.push_back(sr);
}

class Sampler {
public:
	Sampler(RandomGenerator *rnd, Film *flm) : rndGen(rnd), film(flm) { }
	virtual ~Sampler() { }
	
	virtual void RequestSamples(const unsigned int size) = 0;

	// index 0 and 1 are always image X and image Y
	virtual float GetSample(const unsigned int index) = 0;
	virtual void NextSample(const vector<SampleResult> &sampleResults) = 0;

protected:
	RandomGenerator *rndGen;
	Film *film;
};

//------------------------------------------------------------------------------
// Random sampler
//------------------------------------------------------------------------------

class InlinedRandomSampler : public Sampler {
public:
	InlinedRandomSampler(RandomGenerator *rnd, Film *flm) : Sampler(rnd, flm) { }
	~InlinedRandomSampler() { }

	void RequestSamples(const unsigned int size) { };

	float GetSample(const unsigned int index) { return rndGen->floatValue(); }
	void NextSample(const vector<SampleResult> &sampleResults) {
		film->AddSampleCount(1.0);

		for (vector<SampleResult>::const_iterator sr = sampleResults.begin(); sr < sampleResults.end(); ++sr) {
			film->SplatFiltered(sr->type, sr->screenX, sr->screenY, sr->radiance);
			film->SplatFilteredAlpha(sr->screenX, sr->screenY, sr->alpha);
		}
	}
};

//------------------------------------------------------------------------------
// Metropolis sampler
//------------------------------------------------------------------------------

class MetropolisSampler : public Sampler {
public:
	MetropolisSampler(RandomGenerator *rnd, Film *film, const unsigned int maxRej,
			const float pLarge, const float imgRange);
	~MetropolisSampler();

	void RequestSamples(const unsigned int size);

	float GetSample(const unsigned int index);

	void NextSample(const vector<SampleResult> &sampleResults);

private:
	unsigned int maxRejects;
	float largeMutationProbability, imageRange;

	unsigned int sampleSize;
	float *samples;
	unsigned int *sampleStamps;

	double totalLuminance, sampleCount;

	float weight;
	unsigned int consecRejects;
	unsigned int stamp;

	// Data saved for the current sample
	unsigned int currentStamp;
	double currentLuminance;
	float *currentSamples;
	unsigned int *currentSampleStamps;
	vector<SampleResult> currentSampleResult;

	bool isLargeMutation, cooldown;
};

} }

#endif	/* _LUXRAYS_SDL_BSDF_H */
