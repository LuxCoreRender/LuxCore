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

#ifndef _SLG_MC_H
#define	_SLG_MC_H

#include <cstring>
#include <cmath>

#include "luxrays/core/geometry/vector.h"

namespace slg {

inline void LatLongMappingMap(float s, float t, luxrays::Vector *wh, float *pdf) {
	const float theta = t * M_PI;
	const float sinTheta = sinf(theta);

	if (wh) {
		const float phi = s * 2.f * M_PI;
		*wh = luxrays::SphericalDirection(sinTheta, cosf(theta), phi);
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
				theta = 8.f + sy / r;
		} else {
			// Handle second region of disk
			r = sy;
			theta = 2.f - sx / r;
		}
	} else {
		if (sx <= sy) {
			// Handle third region of disk
			r = -sx;
			theta = 4.f - sy / r;
		} else {
			// Handle fourth region of disk
			r = -sy;
			theta = 6.f + sx / r;
		}
	}
	theta *= M_PI / 4.f;
	*dx = r * cosf(theta);
	*dy = r * sinf(theta);
}

inline luxrays::Vector UniformSampleSphere(const float u1, const float u2) {
	float z = 1.f - 2.f * u1;
	float r = sqrtf(luxrays::Max(0.f, 1.f - z * z));
	float phi = 2.f * M_PI * u2;
	float x = r * cosf(phi);
	float y = r * sinf(phi);

	return luxrays::Vector(x, y, z);
}

inline luxrays::Vector CosineSampleHemisphere(const float u1, const float u2, float *pdfW = NULL) {
	luxrays::Vector ret;
	ConcentricSampleDisk(u1, u2, &ret.x, &ret.y);
	ret.z = sqrtf(luxrays::Max(0.f, 1.f - ret.x * ret.x - ret.y * ret.y));

	if (pdfW)
		*pdfW = ret.z * INV_PI;
	
	return ret;
}

inline luxrays::Vector UniformSampleCone(float u1, float u2, float costhetamax) {
	float costheta = luxrays::Lerp(u1, costhetamax, 1.f);
	float sintheta = sqrtf(1.f - costheta*costheta);
	float phi = u2 * 2.f * M_PI;
	return luxrays::Vector(cosf(phi) * sintheta,
	              sinf(phi) * sintheta,
		          costheta);
}

inline luxrays::Vector UniformSampleCone(float u1, float u2, float costhetamax,
		const luxrays::Vector &x, const luxrays::Vector &y, const luxrays::Vector &z) {
	float costheta = luxrays::Lerp(u1, costhetamax, 1.f);
	float sintheta = sqrtf(1.f - costheta*costheta);
	float phi = u2 * 2.f * M_PI;
	return cosf(phi) * sintheta * x + sinf(phi) * sintheta * y + costheta * z;
}

inline float UniformConePdf(float costhetamax) {
	return 1.f / (2.f * M_PI * (1.f - costhetamax));
}

inline void ComputeStep1dCDF(const float *f, unsigned int nSteps, float *c, float *cdf) {
	// Compute integral of step function at $x_i$
	cdf[0] = 0.f;
	for (unsigned int i = 1; i < nSteps + 1; ++i)
		cdf[i] = cdf[i - 1] + f[i - 1] / nSteps;
	// Transform step function integral into cdf
	*c = cdf[nSteps];
	for (unsigned int i = 1; i < nSteps + 1; ++i)
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
		invCount = 1.f / count;
		memcpy(func, f, n * sizeof (float));
		// funcInt is the sum of all f elements divided by the number
		// of elements, ie the average value of f over [0;1)
		ComputeStep1dCDF(func, n, &funcInt, cdf);
		if (funcInt > 0.f) {
			const float invFuncInt = 1.f / funcInt;
			// Normalize func to speed up computations
			for (u_int i = 0; i < count; ++i)
				func[i] *= invFuncInt;
		}
	}

	~Distribution1D() {
		delete[] func;
		delete[] cdf;
	}

	/**
	 * Samples a point from this distribution.
	 * The pdf is computed so that int(u=0..1, pdf(u)*du) = 1
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
		if (u <= cdf[0]) {
			*pdf = func[0];
			if (off)
				*off = 0;
			return 0.f;
		}
		if (u >= cdf[count]) {
			*pdf = func[count - 1];
			if (off)
				*off = count - 1;
			return 1.f;
		}
		const float *ptr = std::upper_bound(cdf, cdf + count + 1, u);
		const u_int offset = ptr - cdf - 1;

		// Compute offset along CDF segment
		const float du = (u - cdf[offset]) /
				(cdf[offset + 1] - cdf[offset]);

		// Compute PDF for sampled offset
		*pdf = func[offset];

		// Save offset
		if (off)
			*off = offset;

		// Return $x \in [0,1)$ corresponding to sample
		return (offset + du) * invCount;
	}

	/**
	 * Samples an interval from this distribution.
	 * The pdf is computed so that sum(i=0..n-1, pdf(i)) = 1
	 * with n the number of intervals
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
		if (u <= cdf[0]) {
			if (du)
				*du = 0.f;
			*pdf = func[0] * invCount;
			return 0;
		}
		if (u >= cdf[count]) {
			if (du)
				*du = 1.f;
			*pdf = func[count - 1] * invCount;
			return count - 1;
		}
		float *ptr = std::upper_bound(cdf, cdf + count + 1, u);
		u_int offset = ptr - cdf - 1;

		// Compute offset along CDF segment
		if (du)
			*du = (u - cdf[offset]) /
			(cdf[offset + 1] - cdf[offset]);

		// Compute PDF for sampled offset
		*pdf = func[offset] * invCount;
		return offset;
	}

	/**
	 * The pdf associated to a given interval
	 * 
	 * @param offset The interval number in the [0,n) range
	 *
	 * @return The pdf so that sum(i=0..n-1, pdf(i)) = 1
	 */
	float Pdf(u_int offset) const {
		return func[offset] * invCount;
	}

	/**
	 * The pdf associated to a given point
	 * 
	 * @param offset The point position in the [0,1) range
	 *
	 * @return The pdf so that int(u=0..1, pdf(u)*du) = 1
	 */
	float Pdf(float u) const {
		return func[Offset(u)];
	}

	float Average() const {
		return funcInt;
	}

	u_int Offset(float u) const {
		return luxrays::Min(count - 1, luxrays::Floor2UInt(u * count));
	}

	const u_int GetCount() const { return count; }
	const float *GetFuncs() const { return func; }
	const float *GetCDFs() const { return cdf; }

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
	float funcInt, invCount;
	/*
	 * The number of function values. The number of cdf values is count+1.
	 */
	u_int count;
};

class Distribution2D {
public:
	// Distribution2D Public Methods

	Distribution2D(const float *data, u_int nu, u_int nv) {
		pConditionalV.reserve(nv);
		// Compute conditional sampling distribution for $\tilde{v}$
		for (u_int v = 0; v < nv; ++v)
			pConditionalV.push_back(new Distribution1D(data + v * nu, nu));
		// Compute marginal sampling distribution $p[\tilde{v}]$
		std::vector<float> marginalFunc;
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
		uv[1] = pMarginal->SampleDiscrete(u1, &pdfs[1]);
		uv[0] = pConditionalV[uv[1]]->SampleDiscrete(u0, &pdfs[0]);
		*pdf = pdfs[0] * pdfs[1];
	}

	float Pdf(float u, float v) const {
		return pConditionalV[pMarginal->Offset(v)]->Pdf(u) *
				pMarginal->Pdf(v);
	}

	float Average() const {
		return pMarginal->Average();
	}

	const u_int GetWidth() const { return pConditionalV[0]->GetCount(); }
	const u_int GetHeight() const { return pMarginal->GetCount(); }
	const Distribution1D *GetMarginalDistribution() const { return pMarginal; }
	const Distribution1D *GetConditionalDistribution(const u_int i) const {
		return pConditionalV[i];
	}

private:
	// Distribution2D Private Data
	std::vector<Distribution1D *> pConditionalV;
	Distribution1D *pMarginal;
};

inline float PdfWtoA(const float pdfW, const float dist, const float cosThere) {
    return pdfW * fabsf(cosThere) / (dist * dist);
}

inline float PdfAtoW(const float pdfA, const float dist, const float cosThere) {
    return pdfA * dist * dist / fabsf(cosThere);
}

inline float BalanceHeuristic(const u_int nf, const float fPdf, const u_int ng, const float gPdf) {
	return (nf * fPdf) / (nf * fPdf + ng * gPdf);
}

inline float BalanceHeuristic(const float fPdf, const float gPdf) {
	return fPdf / (fPdf + gPdf);
}

inline float PowerHeuristic(const u_int nf, const float fPdf, const u_int ng, const float gPdf) {
	const float f = nf * fPdf, g = ng * gPdf;
	return (f * f) / (f * f + g * g);
}

inline float PowerHeuristic(const float fPdf, const float gPdf) {
	const float f = fPdf, g = gPdf;
	return (f * f) / (f * f + g * g);
}

}

#endif	/* _SLG_MC_H */

