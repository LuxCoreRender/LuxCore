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

#ifndef _MC_H
#define	_MC_H

#include "luxrays/luxrays.h"

inline void LatLongMappingMap(float s, float t, Vector *wh, float *pdf) {
	const float theta = t * M_PI;
	const float sinTheta = sinf(theta);

	if (wh) {
		const float phi = s * 2.f * M_PI;
		*wh = SphericalDirection(sinTheta, cosf(theta), phi);
	}

	if (pdf)
		*pdf = INV_TWOPI * INV_PI / sinTheta;
}

inline void ConcentricSampleDisk(const float u1, const float u2, float *dx, float *dy) {
	float r, theta;
	// Map uniform random numbers to $[-1,1]^2$
	float sx = 2.f * u1 - 1.f;
	float sy = 2.f * u2 - 1.f;
	// Map square to $(r,\theta)$
	// Handle degeneracy at the origin
	if (sx == 0.f && sy == 0.f) {
		*dx = 0.f;
		*dy = 0.f;
		return;
	}
	if (sx >= -sy) {
		if (sx > sy) {
			// Handle first region of disk
			r = sx;
			if (sy > 0.f)
				theta = sy / r;
			else
				theta = 8.0f + sy / r;
		} else {
			// Handle second region of disk
			r = sy;
			theta = 2.0f - sx / r;
		}
	} else {
		if (sx <= sy) {
			// Handle third region of disk
			r = -sx;
			theta = 4.0f - sy / r;
		} else {
			// Handle fourth region of disk
			r = -sy;
			theta = 6.0f + sx / r;
		}
	}
	theta *= M_PI / 4.f;
	*dx = r * cosf(theta);
	*dy = r * sinf(theta);
}

inline Vector CosineSampleHemisphere(const float u1, const float u2) {
	Vector ret;
	ConcentricSampleDisk(u1, u2, &ret.x, &ret.y);
	ret.z = sqrtf(max(0.f, 1.f - ret.x * ret.x - ret.y * ret.y));

	return ret;
}

inline void ComputeStep1dCDF(const float *f, u_int nSteps, float *c, float *cdf) {
	// Compute integral of step function at $x_i$
	cdf[0] = 0.f;
	for (u_int i = 1; i < nSteps + 1; ++i)
		cdf[i] = cdf[i - 1] + f[i - 1] / nSteps;
	// Transform step function integral into cdf
	*c = cdf[nSteps];
	for (u_int i = 1; i < nSteps + 1; ++i)
		cdf[i] /= *c;
}

// A utility class for sampling from a regularly sampled 1D distribution.
class Distribution1D {
public:

	/**
	 * Creates a 1D distribution for the given function.
	 * It is assumed that the given function is sampled regularly sampled in
	 * the interval [0,1] (ex. 0.1, 0.3, 0.5, 0.7, 0.9 for 5 samples).
	 *
	 * @param f The values of the function.
	 * @param n The number of samples.
	 */
	Distribution1D(const float *f, u_int n) {
		func = new float[n];
		cdf = new float[n + 1];
		count = n;
		memcpy(func, f, n * sizeof (float));
		// funcInt is the sum of all f elements divided by the number
		// of elements, ie the average value of f over [0;1)
		ComputeStep1dCDF(func, n, &funcInt, cdf);
		invFuncInt = 1.f / funcInt;
		invCount = 1.f / count;
	}

	~Distribution1D() {
		delete[] func;
		delete[] cdf;
	}

	/**
	 * Samples from this distribution.
	 *
	 * @param u   The random value used to sample.
	 * @param pdf The pointer to the float where the pdf of the sample
	 *            should be stored.
	 * @param off Optional parameter to get the offset of the value
	 *
	 * @return The x value of the sample (i.e. the x in f(x)).
	 */
	float SampleContinuous(float u, float *pdf, u_int *off = NULL) const {
		// Find surrounding CDF segments and _offset_
		float *ptr = std::lower_bound(cdf, cdf + count + 1, u);
		u_int offset = max<int>(0, ptr - cdf - 1);

		// Compute offset along CDF segment
		const float du = (u - cdf[offset]) /
				(cdf[offset + 1] - cdf[offset]);

		// Compute PDF for sampled offset
		*pdf = func[offset] * invFuncInt;

		// Save offset
		if (off)
			*off = offset;

		// Return $x \in [0,1)$ corresponding to sample
		return (offset + du) * invCount;
	}

	/**
	 * Samples from this distribution.
	 *
	 * @param u   The random value used to sample.
	 * @param pdf The pointer to the float where the pdf of the sample
	 *            should be stored.
	 * @param du  Optional parameter to get the remaining offset
	 *
	 * @return The index of the sampled interval.
	 */
	u_int SampleDiscrete(float u, float *pdf, float *du = NULL) const {
		// Find surrounding CDF segments and _offset_
		float *ptr = std::lower_bound(cdf, cdf + count + 1, u);
		u_int offset = max<int>(0, ptr - cdf - 1);

		// Compute offset along CDF segment
		if (du)
			*du = (u - cdf[offset]) /
			(cdf[offset + 1] - cdf[offset]);

		// Compute PDF for sampled offset
		*pdf = func[offset] * invFuncInt * invCount;
		return offset;
	}

	float Pdf(float u) const {
		return func[Offset(u)] * invFuncInt;
	}

	float Average() const {
		return funcInt;
	}

	u_int Offset(float u) const {
		return min(count - 1, Floor2UInt(u * count));
	}

private:
	// Distribution1D Private Data
	/*
	 * The function and its cdf.
	 */
	float *func, *cdf;
	/**
	 * The function integral (assuming it is regularly sampled with an interval of 1),
	 * the inverted function integral and the inverted count.
	 */
	float funcInt, invFuncInt, invCount;
	/*
	 * The number of function values. The number of cdf values is count+1.
	 */
	u_int count;
};

class Distribution2D {
public:
	Distribution2D(const float *data, u_int nu, u_int nv) {
		pConditionalV.reserve(nv);
		// Compute conditional sampling distribution for $\tilde{v}$
		for (u_int v = 0; v < nv; ++v)
			pConditionalV.push_back(new Distribution1D(data + v * nu, nu));
		// Compute marginal sampling distribution $p[\tilde{v}]$
		vector<float> marginalFunc;
		marginalFunc.reserve(nv);
		for (u_int v = 0; v < nv; ++v)
			marginalFunc.push_back(pConditionalV[v]->Average());
		pMarginal = new Distribution1D(&marginalFunc[0], nv);
	}

	~Distribution2D() {
		delete pMarginal;
		for (u_int i = 0; i < pConditionalV.size(); ++i)
			delete pConditionalV[i];
	}

	void SampleContinuous(float u0, float u1, float uv[2],
			float *pdf) const {
		float pdfs[2];
		u_int v;
		uv[1] = pMarginal->SampleContinuous(u1, &pdfs[1], &v);
		uv[0] = pConditionalV[v]->SampleContinuous(u0, &pdfs[0]);
		*pdf = pdfs[0] * pdfs[1];
	}

	void SampleDiscrete(float u0, float u1, u_int uv[2], float *pdf) const {
		float pdfs[2];
		uv[1] = pMarginal->SampleDiscrete(u1, &pdf[1]);
		uv[0] = pConditionalV[uv[1]]->SampleDiscrete(u0, &pdf[0]);
		*pdf = pdfs[0] * pdfs[1];
	}

	float Pdf(float u, float v) const {
		return pConditionalV[pMarginal->Offset(v)]->Pdf(u) *
				pMarginal->Pdf(v);
	}

	float Average() const {
		return pMarginal->Average();
	}

private:
	// Distribution2D Private Data
	vector<Distribution1D *> pConditionalV;
	Distribution1D *pMarginal;
};

#endif	/* _MC_H */

