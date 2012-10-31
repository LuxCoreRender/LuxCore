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

#include "luxrays/core/geometry/frame.h"
#include "luxrays/utils/sdl/material.h"

using namespace luxrays;
using namespace luxrays::sdl;

//------------------------------------------------------------------------------
// Matte
//------------------------------------------------------------------------------

Spectrum MatteMaterial::Evaluate(const bool fromLight, const bool into,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	*event = DIFFUSE | REFLECT;

	if (!into ||
			(fabsf(lightDir.z) < DEFAULT_COS_EPSILON_STATIC) ||
			(fabsf(eyeDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	if(directPdfW)
		*directPdfW = fabsf(eyeDir.z * INV_PI);

	if(reversePdfW)
		*reversePdfW = fabsf(lightDir.z * INV_PI);

	return KdOverPI;
}

Spectrum MatteMaterial::Sample(const bool fromLight,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float u2,
	float *pdfW, float *cosSampledDir, BSDFEvent *event) const {
	*event = DIFFUSE | REFLECT;

	if (fabsf(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*sampledDir = Sgn(fixedDir.z) * CosineSampleHemisphere(u0, u1);

	*cosSampledDir = fabsf(sampledDir->z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*pdfW = INV_PI * (*cosSampledDir);

	return KdOverPI;
}

//------------------------------------------------------------------------------
// Mirror
//------------------------------------------------------------------------------

Spectrum MirrorMaterial::Evaluate(const bool fromLight, const bool into,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	*event = SPECULAR | REFLECT;

	return Spectrum();
}

Spectrum MirrorMaterial::Sample(const bool fromLight,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float u2,
	float *pdf, float *cosSampledDir, BSDFEvent *event) const {
	*event = SPECULAR | REFLECT;

	*sampledDir = Vector(-fixedDir.x, -fixedDir.y, fixedDir.z);
	*pdf = 1.f;

	*cosSampledDir = fabsf(sampledDir->z);
	// The cosSampledDir is used to compensate the other one used inside the integrator
	return Kr / (*cosSampledDir);
}

//------------------------------------------------------------------------------
// Glass
//------------------------------------------------------------------------------

Spectrum GlassMaterial::Evaluate(const bool fromLight, const bool into,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	*event = SPECULAR | REFLECT;

	return Spectrum();
}

Spectrum GlassMaterial::Sample(const bool fromLight,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float u2,
	float *pdf, float *cosSampledDir, BSDFEvent *event) const {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	const Vector shadeN(0.f, 0.f, into ? 1.f : -1.f);
	const Vector N(0.f, 0.f, 1.f);

	const Vector rayDir = -fixedDir;
	const Vector reflDir = rayDir - (2.f * Dot(N, rayDir)) * Vector(N);

	const float nc = ousideIor;
	const float nt = ior;
	const float nnt = into ? (nc / nt) : (nt / nc);
	const float nnt2 = nnt * nnt;
	const float ddn = Dot(rayDir, shadeN);
	const float cos2t = 1.f - nnt2 * (1.f - ddn * ddn);

	// Total internal reflection
	if (cos2t < 0.f) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdf = 1.f;

		return Krefl / (*cosSampledDir);
	}

	const float kk = (into ? 1.f : -1.f) * (ddn * nnt + sqrtf(cos2t));
	const Vector nkk = kk * Vector(N);
	const Vector transDir = Normalize(nnt * rayDir - nkk);

	const float c = 1.f - (into ? -ddn : Dot(transDir, N));

	const float Re = R0 + (1.f - R0) * c * c * c * c * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f) {
		if (Re == 0.f) {
			*event = NONE;
			return Spectrum();
		} else {
			*event = SPECULAR | REFLECT;
			*sampledDir = reflDir;
			*cosSampledDir = fabsf(sampledDir->z);
			*pdf = 1.f;

			return Krefl / (*cosSampledDir);
		}
	} else if (Re == 0.f) {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdf = 1.f;

		if (fromLight)
			return Krefrct * (nnt2 / (*cosSampledDir));
		else
			return Krefrct / (*cosSampledDir);
	} else if (u0 < P) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdf = P / Re;

		return Krefl / (*cosSampledDir);
	} else {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdf = (1.f - P) / Tr;

		if (fromLight)
			return Krefrct * (nnt2 / (*cosSampledDir));
		else
			return Krefrct / (*cosSampledDir);
	}
}
