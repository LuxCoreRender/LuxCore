/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_SPD_H
#define _LUXRAYS_SPD_H

#include "luxrays/luxrays.h"
#include "luxrays/utils/utils.h"
#include <vector>
#include <span>

namespace luxrays {

class XYZColor;

class SPD {
public:
	SPD() {
		nSamples = 0U;
		lambdaMin = lambdaMax = delta = invDelta = 0.f;
	}
	virtual ~SPD() {}

	// Access to samples in a span
	std::span<const float> samples() const {
		return std::span<const float>(_samples.data(), nSamples);
	}
	std::span<float> samples() {
		return std::span<float>(_samples.data(), nSamples);
	}

	// samples the SPD by performing a linear interpolation on the data
	inline float Sample(const float lambda) const {
		if (nSamples <= 1 || lambda < lambdaMin || lambda > lambdaMax)
			return 0.f;

		// interpolate the two closest samples linearly
		const float x = (lambda - lambdaMin) * invDelta;

		const auto b0 = static_cast<size_t>(std::floor(std::max(x, 0.f)));
		const auto b1 = std::min(b0 + 1, nSamples - 1);
		const float dx = x - b0;
		return std::lerp(samples()[b0], samples()[b1], dx);
	}

	inline void Sample(u_int n, const float lambda[], float *p) const {
		for (u_int i = 0; i < n; ++i) {
			if (nSamples <= 1 || lambda[i] < lambdaMin ||
				lambda[i] > lambdaMax) {
				p[i] = 0.f;
				continue;
			}

			// interpolate the two closest samples linearly
			const float x = (lambda[i] - lambdaMin) * invDelta;
			const auto b0 = static_cast<size_t>(std::floor(std::max(x, 0.f)));
			const auto b1 = std::min(b0 + 1, nSamples - 1);
			const float dx = x - b0;
			p[i] = std::lerp(samples()[b0], samples()[b1], dx);
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
			bins[i] = Floor2UInt(x);
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
			p[i] = std::lerp(samples()[bins[i]], samples()[bins[i] + 1], offsets[i]);
		}
	}

	float Y() const;
	float Filter() const;
	XYZColor ToXYZ() const;
	XYZColor ToNormalizedXYZ() const;
	void AllocateSamples(u_int n);
	void Normalize();
	void Clamp();
	void Scale(float s);
	void Whitepoint(float temp);




protected:
	void AddWeighted(float w, const float *c);

	// Data
	size_t nSamples;
	float lambdaMin, lambdaMax;
	float delta, invDelta;
	std::vector<float> _samples;

};

}

#endif // _LUXRAYS_SPD_H
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
