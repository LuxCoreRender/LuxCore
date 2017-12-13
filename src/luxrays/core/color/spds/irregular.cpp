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

#include <vector>

#include "luxrays/core/color/spds/irregular.h"

using namespace std;
using namespace luxrays;

// creates an irregularly sampled SPD
// may "truncate" the edges to fit the new resolution
//  wavelengths   array containing the wavelength of each sample
//  samples       array of sample values at the given wavelengths
//  n             number of samples
//  resolution    resampling resolution (in nm)
IrregularSPD::IrregularSPD(const float* const wavelengths, const float* const samples,
	u_int n, float resolution, SPDResamplingMethod resamplingMethod) 
	: SPD() {
	const float lambdaMin = wavelengths[0];
	const float lambdaMax = wavelengths[n - 1];

	const u_int sn = Ceil2UInt((lambdaMax - lambdaMin) / resolution) + 1;

	vector<float> sam(sn);

	if (resamplingMethod == Linear) {
		u_int k = 0;
		for (u_int i = 0; i < sn; i++) {
			const float lambda = lambdaMin + i * resolution;

			if (lambda < wavelengths[0] || lambda > wavelengths[n-1]) {
				sam[i] = 0.f;
				continue;
			}

			for (; k < n; ++k) {
				if (wavelengths[k] >= lambda)
					break;
			}

			if (wavelengths[k] == lambda)
				sam[i] = samples[k];
			else { 
				const float intervalWidth = wavelengths[k] - wavelengths[k - 1];
				const float u = (lambda - wavelengths[k - 1]) / intervalWidth;
				sam[i] = Lerp(u, samples[k - 1], samples[k]);
			}
		}
	} else {
		vector<float> sd(n);

		calc_spline_data(wavelengths, samples, n, &sd[0]);

		u_int k = 0;
		for (u_int i = 0; i < sn; i++) {
			const float lambda = lambdaMin + i * resolution;

			if (lambda < wavelengths[0] || lambda > wavelengths[n-1]) {
				sam[i] = 0.f;
				continue;
			}

			while (lambda > wavelengths[k+1])
				k++;

			const float h = wavelengths[k + 1] - wavelengths[k];
			const float a = (wavelengths[k + 1] - lambda) / h;
			const float b = (lambda - wavelengths[k]) / h;

			sam[i] = max(a*samples[k] + b*samples[k+1]+
				((a*a*a-a)*sd[k] + (b*b*b-b)*sd[k+1])*(h*h)/6.f, 0.f);
		}
	}

	init(lambdaMin, lambdaMax, &sam[0], sn);
}

void IrregularSPD::init(float lMin, float lMax, const float* const s, u_int n) {
	lambdaMin = lMin;
	lambdaMax = lMax;
	delta = (lambdaMax - lambdaMin) / (n-1);
	invDelta = 1.f / delta;
	nSamples = n;

	AllocateSamples(n);

	// Copy samples
	for (u_int i = 0; i < n; ++i)
		samples[i] = s[i];

}

// computes data for natural spline interpolation
// from Numerical Recipes in C
void IrregularSPD::calc_spline_data(const float* const wavelengths,
	const float* const amplitudes, u_int n, float *spline_data) {
	vector<float> u(n - 1);

	// natural spline
	spline_data[0] = u[0] = 0.f;

	for (u_int i = 1; i <= n - 2; i++) {
		const float sig = (wavelengths[i] - wavelengths[i - 1]) /
			(wavelengths[i + 1] - wavelengths[i - 1]);
		const float p = sig * spline_data[i - 1] + 2.f;
		spline_data[i] = (sig - 1.f) / p;
		u[i] = (amplitudes[i + 1] - amplitudes[i]) /
			(wavelengths[i + 1] - wavelengths[i]) - 
			(amplitudes[i] - amplitudes[i - 1]) /
			(wavelengths[i] - wavelengths[i - 1]);
		u[i] = (6.f * u[i] /
			(wavelengths[i + 1] - wavelengths[i - 1]) -
			sig * u[i - 1]) / p;
	}

	float qn, un;
  
	// natural spline
	qn = un = 0.f;
	spline_data[n - 1] = (un - qn * u[n-2]) /
		(qn * spline_data[n - 2] + 1.f);
	for (u_int k = 0; k < n - 1; ++k)
		spline_data[n - k - 1] = spline_data[n - k - 1] *
			spline_data[n - k] + u[k];
}
