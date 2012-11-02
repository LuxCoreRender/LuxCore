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

namespace luxrays { namespace utils {

class Sampler {
public:
	Sampler() { };
	virtual ~Sampler() { }
	
	virtual void RequestSamples(const unsigned int size) = 0;

	virtual float GetSample(const unsigned int index) = 0;
	virtual void NextSample(const float currentSampleLuminance) = 0;
};

//------------------------------------------------------------------------------
// Random sampler
//------------------------------------------------------------------------------

class InlinedRandomSampler : public Sampler {
public:
	InlinedRandomSampler(RandomGenerator *rnd) : rndGen(rnd) { };
	~InlinedRandomSampler() { }

	void RequestSamples(const unsigned int size) { };

	float GetSample(const unsigned int index) { return rndGen->floatValue(); }
	void NextSample(const float currentSampleLuminance) { }

private:
	RandomGenerator *rndGen;
};

} }

#endif	/* _LUXRAYS_SDL_BSDF_H */
