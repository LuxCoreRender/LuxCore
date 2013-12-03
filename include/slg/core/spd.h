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

/* SPD classes are from luxrender */

#ifndef _SLG_SPD_H
#define	_SLG_SPD_H

#include <cstddef>

#include "luxrays/luxrays.h"
#include "luxrays/core/spectrum.h"
#include "luxrays/utils/mc.h"

namespace slg {

class SPD {
public:
	SPD() {
		nSamples = 0U;
		lambdaMin = lambdaMax = delta = invDelta = 0.f;
		samples = NULL;
	}
	virtual ~SPD() { FreeSamples(); }

	// Samples the SPD by performing a linear interpolation on the data
	inline float sample(const float lambda) const {
		if (nSamples <= 1 || lambda < lambdaMin || lambda > lambdaMax)
			return 0.f;

		// interpolate the two closest samples linearly
		const float x = (lambda - lambdaMin) * invDelta;
		const unsigned int b0 = luxrays::Floor2UInt(x);
		const unsigned int b1 = luxrays::Min(b0 + 1, nSamples - 1);
		const float dx = x - b0;
		return luxrays::Lerp(dx, samples[b0], samples[b1]);
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
			const unsigned int b0 = luxrays::Floor2UInt(x);
			const unsigned int b1 = luxrays::Min(b0 + 1, nSamples - 1);
			const float dx = x - b0;
			p[i] = luxrays::Lerp(dx, samples[b0], samples[b1]);
		}
	}

	// Get offsets for fast sampling
	inline void Offsets(u_int n, const float lambda[], int *bins, float *offsets) const {
		for (u_int i = 0; i < n; ++i) {
			if (nSamples <= 1 || lambda[i] < lambdaMin ||
				lambda[i] > lambdaMax) {
				bins[i] = -1;
				continue;
			}

			const float x = (lambda[i] - lambdaMin) * invDelta;
			bins[i] = luxrays::Floor2UInt(x);
			offsets[i] = x - bins[i];
		}
	}

	// Fast sampling
	inline void Sample(u_int n, const int bins[], const float offsets[], float *p) const {
		for (u_int i = 0; i < n; ++i) {
			if (bins[i] < 0 || bins[i] >= static_cast<int>(nSamples - 1)) {
				p[i] = 0.f;
				continue;
			}
			p[i] = luxrays::Lerp(offsets[i], samples[bins[i]], samples[bins[i] + 1]);
		}
	}

	float Y() const;
	float Filter() const;
	luxrays::Spectrum ToXYZ() const;
	luxrays::Spectrum ToNormalizedXYZ() const;

	void AllocateSamples(unsigned int n);
	void FreeSamples();

	void Normalize();
	void Clamp();
	void Scale(float s);
	void Whitepoint(float temp);

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
typedef enum { Linear, Spline } SPDResamplingMethod;

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

}

#endif	/* _SLG_SPD_H */
