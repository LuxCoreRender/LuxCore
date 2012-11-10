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

/* SPD classes are from luxrender */

#ifndef _LUXRAYS_SDL_SPD_H
#define	_LUXRAYS_SDL_SPD_H

#include <cstddef>

#include "luxrays/luxrays.h"
#include "luxrays/utils/core/mc.h"
#include "luxrays/utils/core/spectrum.h"

namespace luxrays { namespace sdl {

class SPD {
public:
	SPD() {
		nSamples = 0U;
		lambdaMin = lambdaMax = delta = invDelta = 0.f;
		samples = NULL;
	}
	virtual ~SPD() { FreeSamples(); }

	// samples the SPD by performing a linear interpolation on the data
	inline float sample(const float lambda) const {
		if (nSamples <= 1 || lambda < lambdaMin || lambda > lambdaMax)
			return 0.f;

		// interpolate the two closest samples linearly
		const float x = (lambda - lambdaMin) * invDelta;
		const unsigned int b0 = Floor2UInt(x);
		const unsigned int b1 = Min(b0 + 1, nSamples - 1);
		const float dx = x - b0;
		return Lerp(dx, samples[b0], samples[b1]);
	}

	inline void sample(unsigned int n, const float lambda[], float *p) const {
		for (unsigned int i = 0; i < n; ++i) {
			if (nSamples <= 1 || lambda[i] < lambdaMin ||
				lambda[i] > lambdaMax) {
				p[i] = 0.f;
				continue;
			}

			// interpolate the two closest samples linearly
			const float x = (lambda[i] - lambdaMin) * invDelta;
			const unsigned int b0 = Floor2UInt(x);
			const unsigned int b1 = Min(b0 + 1, nSamples - 1);
			const float dx = x - b0;
			p[i] = Lerp(dx, samples[b0], samples[b1]);
		}
	}

	float Y() const;
	float Filter() const;
	void AllocateSamples(unsigned int n);
	void FreeSamples();
	void Normalize();
	void Clamp();
	void Scale(float s);
	void Whitepoint(float temp);
	Spectrum ToRGB();

protected:
	unsigned int nSamples;
	float lambdaMin, lambdaMax;
	float delta, invDelta;
	float *samples;

};

// regularly sampled SPD, reconstructed using linear interpolation
class RegularSPD : public SPD {
public:
	RegularSPD() : SPD() {}

	//  creates a regularly sampled SPD
	//  samples    array of sample values
	//  lambdaMin  wavelength (nm) of first sample
	//  lambdaMax  wavelength (nm) of last sample
	//  n          number of samples
	RegularSPD(const float* const s, float lMin, float lMax, unsigned int n) : SPD() {
		init(lMin, lMax, s, n);
	}

	virtual ~RegularSPD() {}

protected:
	void init(float lMin, float lMax, const float* const s, unsigned int n);
};

// only use spline for regular data
enum SPDResamplingMethod { Linear, Spline };

// Irregularly sampled SPD
// Resampled to a fixed resolution (at construction)
// using cubic spline interpolation
class IrregularSPD : public SPD {
public:

	IrregularSPD() : SPD() {}

	// creates an irregularly sampled SPD
	// may "truncate" the edges to fit the new resolution
	//  wavelengths   array containing the wavelength of each sample
	//  samples       array of sample values at the given wavelengths
	//  n             number of samples
	//  resolution    resampling resolution (in nm)
	IrregularSPD(const float* const wavelengths, const float* const samples,
		unsigned int n, float resolution = 5, SPDResamplingMethod resamplignMethod = Linear);

	virtual ~IrregularSPD() {}

protected:
	  void init(float lMin, float lMax, const float* const s, unsigned int n);

private:
	// computes data for natural spline interpolation
	// from Numerical Recipes in C
	void calc_spline_data(const float* const wavelengths,
	const float* const amplitudes, unsigned int n, float *spline_data);
};

} }

#endif	/* _LUXRAYS_SDL_SPD_H */
