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

#ifndef _LUXRAYS_IRREGULARSPD_H
#define _LUXRAYS_IRREGULARSPD_H

#include "luxrays/core/color/spd.h"

namespace luxrays {
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
		u_int n, float resolution = 5, SPDResamplingMethod resamplignMethod = Linear);

	virtual ~IrregularSPD() {}

protected:
	  void init(float lMin, float lMax, const float* const s, u_int n);

private:
	// computes data for natural spline interpolation
	// from Numerical Recipes in C
	void calc_spline_data(const float* const wavelengths,
		const float* const amplitudes, u_int n, float *spline_data);
};

}

#endif // _LUXRAYS_IRREGULARSPD_H
