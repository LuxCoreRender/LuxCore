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

#ifndef _LUXRAYS_MC_H
#define _LUXRAYS_MC_H

#include "luxrays/core/geometry/vector.h"
#include "luxrays/utils/utils.h"

// MC Utility Declarations
namespace luxrays {
	void RejectionSampleDisk(float u1, float u2, float *x, float *y);
	Vector UniformSampleHemisphere(float u1, float u2);
	float  UniformHemispherePdf(float theta, float phi);
	Vector UniformSampleSphere(float u1, float u2);
	float  UniformSpherePdf();
	Vector UniformSampleCone(float u1, float u2, float costhetamax);
	Vector UniformSampleCone(float u1, float u2, float costhetamax,
		const Vector &x, const Vector &y, const Vector &z);
	float  UniformConePdf(float costhetamax);
	void UniformSampleDisk(float u1, float u2, float *x, float *y);
	void UniformSampleTriangle(float ud1, float ud2, float *u, float *v);
	Vector SampleHG(const Vector &w, float g, float u1, float u2);
	float PhaseHG(const Vector &w, const Vector &wp, float g);
	float HGPdf(const Vector &w, const Vector &wp, float g);
	void ComputeStep1dCDF(const float *f, u_int nValues, float *c, float *cdf);
	float SampleStep1d(const float *f, const float *cdf, float c, u_int nSteps, float u,
		float *weight);
	float NormalCDFInverse(float u);
	void ConcentricSampleDisk(float u1, float u2, float *dx, float *dy);
	float GaussianSampleDisk(float u1);
	float InverseGaussianSampleDisk(float u1);
	float ExponentialSampleDisk(float u1, int power);
	float InverseExponentialSampleDisk(float u1, int power);
	float TriangularSampleDisk(float u1);
	void HoneycombSampleDisk(float u1, float u2, float *dx, float *dy);

	// MC Inline Functions
	inline Vector CosineSampleHemisphere(const float u1, const float u2, float *pdfW = NULL) {
		Vector ret;
		ConcentricSampleDisk(u1, u2, &ret.x, &ret.y);
		ret.z = sqrtf(luxrays::Max(0.f, 1.f - ret.x * ret.x - ret.y * ret.y));

		if (pdfW)
			*pdfW = ret.z * INV_PI;

		return ret;
	}

	inline float CosineHemispherePdf(const float costheta, const float phi) {
		return costheta * INV_PI;
	}
	inline float BalanceHeuristic(const u_int nf, const float fPdf, const u_int ng, const float gPdf) {
		return (nf * fPdf) / (nf * fPdf + ng * gPdf);
	}
	inline float BalanceHeuristic(const float fPdf, const float gPdf) {
		return BalanceHeuristic(1, fPdf, 1, gPdf);
	}
	inline float PowerHeuristic(const u_int nf, const float fPdf, const u_int ng, const float gPdf) {
		const float f = nf * fPdf, g = ng * gPdf;
		return (f * f) / (f * f + g * g);
	}
	inline float PowerHeuristic(const float fPdf, const float gPdf) {
		return PowerHeuristic(1, fPdf, 1, gPdf);
	}

	inline float PdfWtoA(const float pdfW, const float dist, const float cosThere) {
		return pdfW * fabsf(cosThere) / (dist * dist);
	}

	inline float PdfAtoW(const float pdfA, const float dist, const float cosThere) {
		return pdfA * dist * dist / fabsf(cosThere);
	}
}

#endif // _LUXRAYS_MC_H
