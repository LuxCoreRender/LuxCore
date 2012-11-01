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
// Matte material
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
// Mirror material
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
// MatteMirror material
//------------------------------------------------------------------------------

Spectrum MatteMirrorMaterial::Evaluate(const bool fromLight, const bool into,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	Spectrum result = matte.Evaluate(fromLight, into, lightDir, eyeDir, event,
			directPdfW, reversePdfW);

	if (directPdfW)
		*directPdfW *= mattePdf;
	if (reversePdfW)
		*reversePdfW *= mattePdf;

	return result;
}

Spectrum MatteMirrorMaterial::Sample(const bool fromLight,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float u2,
	float *pdf, float *cosSampledDir, BSDFEvent *event) const {
	// Here, I assume u2 isn't used in other Sample()
	const float comp = u2 * totFilter;

	if (comp > matteFilter) {
		const Spectrum result = mirror.Sample(fromLight, fixedDir, sampledDir,
				u0, u1, u2, pdf, cosSampledDir, event);
		*pdf *= mirrorPdf;

		return result;
	} else {
		const Spectrum result = matte.Sample(fromLight, fixedDir, sampledDir,
				u0, u1, u2, pdf, cosSampledDir, event);
		*pdf *= mattePdf;

		return result;
	}
}

//------------------------------------------------------------------------------
// Glass material
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

		// The cosSampledDir is used to compensate the other one used inside the integrator
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

			// The cosSampledDir is used to compensate the other one used inside the integrator
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

		// The cosSampledDir is used to compensate the other one used inside the integrator
		return Krefl / (*cosSampledDir);
	} else {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdf = (1.f - P) / Tr;

		// The cosSampledDir is used to compensate the other one used inside the integrator
		if (fromLight)
			return Krefrct * (nnt2 / (*cosSampledDir));
		else
			return Krefrct / (*cosSampledDir);
	}
}

//------------------------------------------------------------------------------
// Architectural glass material
//------------------------------------------------------------------------------

Spectrum ArchGlassMaterial::Evaluate(const bool fromLight, const bool into,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	*event = SPECULAR | TRANSMIT;

	return Spectrum();
}

Spectrum ArchGlassMaterial::Sample(const bool fromLight,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float u2,
	float *pdf, float *cosSampledDir, BSDFEvent *event) const {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	if (!into) {
		// Architectural glass has not internal reflections
		*event = SPECULAR | TRANSMIT;
		*sampledDir = -fixedDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdf = 1.f;

		// The cosSampledDir is used to compensate the other one used inside the integrator
		return Ktrans / (*cosSampledDir);
	} else {
		// RR to choose if reflect the ray or go trough the glass
		const float comp = u0 * totFilter;
		if (comp > transFilter) {
			*event = SPECULAR | REFLECT;
			*sampledDir = Vector(-fixedDir.x, -fixedDir.y, fixedDir.z);
			*cosSampledDir = fabsf(sampledDir->z);
			*pdf = reflPdf;

			// The cosSampledDir is used to compensate the other one used inside the integrator
			return Krefl / (*cosSampledDir);
		} else {
			*event = SPECULAR | TRANSMIT;
			*sampledDir = -fixedDir;
			*cosSampledDir = fabsf(sampledDir->z);
			*pdf = transPdf;

			// The cosSampledDir is used to compensate the other one used inside the integrator
			return Ktrans / (*cosSampledDir);
		}
	}
}

//------------------------------------------------------------------------------
// Metal material
//------------------------------------------------------------------------------

Spectrum MetalMaterial::Evaluate(const bool fromLight, const bool into,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	*event = SPECULAR | REFLECT;

	return Spectrum();
}

Vector MetalMaterial::GlossyReflection(const Vector &fixedDir, const float exponent,
		const float u0, const float u1) {
	const Normal shadeN(0.f, 0.f, 1.f);

	const float phi = 2.f * M_PI * u0;
	const float cosTheta = powf(1.f - u1, exponent);
	const float sinTheta = sqrtf(Max(0.f, 1.f - cosTheta * cosTheta));
	const float x = cosf(phi) * sinTheta;
	const float y = sinf(phi) * sinTheta;
	const float z = cosTheta;

	const Vector dir = -fixedDir;
	const float dp = Dot(shadeN, dir);
	const Vector w = dir - (2.f * dp) * Vector(shadeN);

	Vector u;
	if (fabsf(shadeN.x) > .1f) {
		const Vector a(0.f, 1.f, 0.f);
		u = Cross(a, w);
	} else {
		const Vector a(1.f, 0.f, 0.f);
		u = Cross(a, w);
	}
	u = Normalize(u);
	Vector v = Cross(w, u);

	return x * u + y * v + z * w;
}

Spectrum MetalMaterial::Sample(const bool fromLight,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float u2,
	float *pdf, float *cosSampledDir, BSDFEvent *event) const {
	*event = SPECULAR | REFLECT;

	*sampledDir = GlossyReflection(fixedDir, exponent, u0, u1);

	if (sampledDir->z > 0.f) {
		*pdf = 1.f;

		*cosSampledDir = fabsf(sampledDir->z);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return Kr / (*cosSampledDir);
	} else
		return Spectrum();
}
