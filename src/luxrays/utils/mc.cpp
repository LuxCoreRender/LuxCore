/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#include <limits>

#include "luxrays/core/randomgen.h"
#include "luxrays/utils/mc.h"
#include "luxrays/utils/mcdistribution.h"

namespace luxrays {

// MC Function Definitions
void ComputeStep1dCDF(const float *f, u_int nSteps, float *c, float *cdf) {
	// Compute integral of step function at $x_i$
	cdf[0] = 0.f;
	for (u_int i = 1; i < nSteps + 1; ++i)
		cdf[i] = cdf[i - 1] + f[i - 1] / nSteps;
	// Transform step function integral into cdf
	*c = cdf[nSteps];
	if (*c > 0.f) {
		for (u_int i = 1; i < nSteps + 1; ++i)
			cdf[i] /= *c;
	}
}

float SampleStep1d(const float *f, const float *cdf, float c, u_int nSteps,
	float u, float *pdf) {
	// Find surrounding cdf segments
	if (u >= cdf[nSteps]) {
		*pdf = f[nSteps - 1] / c;
		return 1.f;
	}
	if (u <= cdf[0]) {
		*pdf = f[0] / c;
		return 0.f;
	}
	const float *ptr = std::upper_bound(cdf, cdf + nSteps + 1, u);
	const u_int offset = static_cast<u_int>(ptr - cdf - 1);
	// Return offset along current cdf segment
	u = (u - cdf[offset]) / (cdf[offset + 1] - cdf[offset]);
	*pdf = f[offset] / c;
	return (offset + u) / nSteps;
}

void RejectionSampleDisk(const float u1, const float u2, float *x, float *y) {
	float sx, sy;
	do {
		sx = 1.f - 2.f * u1;
		sy = 1.f - 2.f * u2;
	} while (sx * sx + sy * sy > 1.f);
	*x = sx;
	*y = sy;
}

Vector UniformSampleHemisphere(const float u1, const float u2) {
	const float z = u1;
	const float r = sqrtf(Max(0.f, 1.f - z*z));
	const float phi = 2.f * M_PI * u2;
	const float x = r * cosf(phi);
	const float y = r * sinf(phi);
	return Vector(x, y, z);
}

float UniformHemispherePdf(float theta, float phi) {
	return INV_TWOPI;
}

Vector UniformSampleSphere(const float u1, const float u2) {
	const float z = 1.f - 2.f * u1;
	const float r = sqrtf(Max(0.f, 1.f - z * z));
	const float phi = 2.f * M_PI * u2;
	const float x = r * cosf(phi);
	const float y = r * sinf(phi);
	return Vector(x, y, z);
}

float UniformSpherePdf() {
	return 1.f / (4.f * M_PI);
}

void UniformSampleDisk(const float u1, const float u2, float *x, float *y) {
	const float r = sqrtf(u1);
	const float theta = 2.0f * M_PI * u2;
	*x = r * cosf(theta);
	*y = r * sinf(theta);
}

void ConcentricSampleDisk(const float u1, const float u2, float *dx, float *dy) {
	float r, theta;
	// Map uniform random numbers to $[-1,1]^2$
	const float sx = 2.f * u1 - 1.f;
	const float sy = 2.f * u2 - 1.f;
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

void UniformSampleTriangle(const float u1, const float u2, float *u, float *v) {
	const float su1 = sqrtf(u1);
	*u = 1.f - su1;
	*v = u2 * su1;
}

float UniformConePdf(const float cosThetaMax) {
	return 1.f / (2.f * M_PI * (1.f - cosThetaMax));
}

Vector UniformSampleCone(const float u1, const float u2, float costhetamax) {
	const float costheta = Lerp(u1, 1.f, costhetamax);
	const float u1x = (1.f - costhetamax) * u1;
	const float sintheta = sqrtf(Max(0.f, u1x * (2.f - u1x)));
	const float phi = u2 * 2.f * M_PI;
	return Vector(cosf(phi) * sintheta, sinf(phi) * sintheta, costheta);
}

Vector UniformSampleCone(const float u1, const float u2, float costhetamax,
	const Vector &x, const Vector &y, const Vector &z) {
	const float costheta = Lerp(u1, 1.f, costhetamax);
	const float u1x = (1.f - costhetamax) * u1;
	const float sintheta = sqrtf(Max(0.f, u1x * (2.f - u1x)));
	const float phi = u2 * 2.f * M_PI;
	return cosf(phi) * sintheta * x + sinf(phi) * sintheta * y +
		costheta * z;
}

Vector SampleHG(const Vector &w, float g, const float u1, const float u2) {
	float costheta;
	if (fabsf(g) < 1e-3f)
		costheta = 1.f - 2.f * u1;
	else {
		// NOTE - lordcrc - Bugfix, pbrt tracker id 0000082: bug in SampleHG
		const float sqrTerm = (1.f - g * g) / (1.f - g + 2.f * g * u1);
		costheta = (1.f + g * g - sqrTerm * sqrTerm) / (2.f * g);
	}
	const float sintheta = sqrtf(Max(0.f, 1.f - costheta * costheta));
	const float phi = 2.f * M_PI * u2;
	Vector v1, v2;
	CoordinateSystem(w, &v1, &v2);
	return SphericalDirection(sintheta, costheta, phi, v1, v2, w);
}

float PhaseHG(const Vector &w, const Vector &wp, float g) {
	const float costheta = Dot(w, wp);
	return 1.f / (4.f * M_PI) * (1.f - g * g) /
		powf(1.f + g * g - 2.f * g * costheta, 1.5f);
}

float HGPdf(const Vector &w, const Vector &wp, const float g) {
	return PhaseHG(w, wp, g);
}

 // Code from "An algorithm for computing the 
 // inverse normal cumulative distribution function"
 // by Peter J. Acklam
 // http://home.online.no/~pjacklam/notes/invnorm/index.html
#define  A1  (-3.969683028665376e+01)
#define  A2   2.209460984245205e+02
#define  A3  (-2.759285104469687e+02)
#define  A4   1.383577518672690e+02
#define  A5  (-3.066479806614716e+01)
#define  A6   2.506628277459239e+00

#define  B1  (-5.447609879822406e+01)
#define  B2   1.615858368580409e+02
#define  B3  (-1.556989798598866e+02)
#define  B4   6.680131188771972e+01
#define  B5  (-1.328068155288572e+01)

#define  C1  (-7.784894002430293e-03)
#define  C2  (-3.223964580411365e-01)
#define  C3  (-2.400758277161838e+00)
#define  C4  (-2.549732539343734e+00)
#define  C5   4.374664141464968e+00
#define  C6   2.938163982698783e+00

#define  D1   7.784695709041462e-03
#define  D2   3.224671290700398e-01
#define  D3   2.445134137142996e+00
#define  D4   3.754408661907416e+00

#define P_LOW   0.02425
/* P_high = 1 - p_low*/
#define P_HIGH  0.97575

static double normsinv(double p) {
	if ((0 < p )  && (p < P_LOW)) {
		const double q = sqrt(-2 * log(p));
		return (((((C1 * q + C2) * q + C3) * q + C4) * q + C5) * q + C6) /
			((((D1 * q + D2) * q + D3) * q + D4) * q + 1);
	} else if ((P_LOW <= p) && (p <= P_HIGH)) {
		const double q = p - 0.5;
		const double r = q * q;
		return (((((A1 * r + A2) * r + A3) * r + A4) * r + A5) * r + A6) * q /
			(((((B1 * r + B2) * r + B3) * r + B4) * r + B5) * r + 1);
	} else if ((P_HIGH < p) && (p < 1)) {
                   const double q = sqrt(-2 * log(1 - p));
                   return -(((((C1 * q + C2) * q + C3) * q + C4) * q + C5) * q + C6) /
			   ((((D1 * q + D2) * q + D3) * q + D4) * q + 1);
	} else {
		return INFINITY;
	}

/* Under UNIX OR LINUX, The return value 'x' could be corrected by the following for better accuracy.
if(( 0 < p)&&(p < 1)){
   e = 0.5 * erfc(-x/sqrt(2)) - p;
   u = e * sqrt(2*M_PI) * exp(x*x/2);
   x = x - u/(1 + x*u/2);
}
*/

}

static float normsinvf(float p) {
	return static_cast<float>(normsinv(p));
}

float NormalCDFInverse(float u) {
	return normsinvf(u);
}

float GaussianSampleDisk(float u1) {
	return Clamp(normsinvf(u1), 0.f, 1.f);
}

float InverseGaussianSampleDisk(float u1) {
	return Clamp(1.f - normsinvf(u1), 0.f, 1.f);
}

float ExponentialSampleDisk(float u1, int power) {
	return Clamp(-logf(u1) / power, 0.f, 1.f);
}

float InverseExponentialSampleDisk(float u1, int power) {
	return Clamp(1.f + logf(u1) / power, 0.f, 1.f);
}

float TriangularSampleDisk(float u1) {
	return Clamp(u1 <= .5f ? sqrtf(u1 * .5f) :
		1.f - sqrtf((1.f - u1) * .5f), 0.f, 1.f);
}

}
