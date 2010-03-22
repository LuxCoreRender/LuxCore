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

#ifndef _MATERIAL_H
#define	_MATERIAL_H

#include <vector>

#include "smalllux.h"
#include "luxrays/utils/core/exttrianglemesh.h"

class Material {
public:
	virtual ~Material() { }

	virtual bool IsLightSource() const = 0;
	virtual bool IsDiffuse() const = 0;
	virtual bool IsSpecular() const = 0;
};

class LightMaterial : public Material {
public:
	bool IsLightSource() const { return true; }
	bool IsDiffuse() const { return false; }
	bool IsSpecular() const { return false; }
};

class AreaLightMaterial : public LightMaterial {
public:
	AreaLightMaterial(const Spectrum &col) { gain = col; }

	const Spectrum &GetGain() const { return gain; }

private:
	Spectrum gain;
};

class SurfaceMaterial : public Material {
public:
	bool IsLightSource() const { return false; }

	virtual Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const = 0;
	virtual Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N,
		const Normal &shadeN, const float u0, const float u1,  const float u2,
		float *pdf,	bool &specularBounce) const = 0;
};

class MatteMaterial : public SurfaceMaterial {
public:
	MatteMaterial(const Spectrum &col) {
		Kd = col;
		KdOverPI = Kd * INV_PI;
	}

	bool IsDiffuse() const { return true; }
	bool IsSpecular() const { return false; }

	Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		return KdOverPI;
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, float *pdf, bool &specularBounce) const {
		Vector dir = CosineSampleHemisphere(u0, u1);
		*pdf = dir.z * INV_PI;

		Vector v1, v2;
		CoordinateSystem(Vector(N), &v1, &v2);

		dir = Vector(
				v1.x * dir.x + v2.x * dir.y + N.x * dir.z,
				v1.y * dir.x + v2.y * dir.y + N.y * dir.z,
				v1.z * dir.x + v2.z * dir.y + N.z * dir.z);

		(*wi) = dir;
		specularBounce = false;

		return KdOverPI;
	}

	const Spectrum &GetKd() const { return Kd; }

private:
	Spectrum Kd, KdOverPI;
};

class MirrorMaterial : public SurfaceMaterial {
public:
	MirrorMaterial(const Spectrum &refl, bool reflSpecularBounce) {
		Kr = refl;
		reflectionSpecularBounce = reflSpecularBounce;
	}

	bool IsDiffuse() const { return false; }
	bool IsSpecular() const { return true; }

	Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		throw std::runtime_error("Internal error, called MirrorMaterial::f()");
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, float *pdf, bool &specularBounce) const {
		const Vector dir = -wo;
		const float dp = Dot(shadeN, dir);
		(*wi) = dir - (2.f * dp) * Vector(shadeN);

		specularBounce = reflectionSpecularBounce;
		*pdf = 1.f;

		return Kr / (-dp);
	}

	const Spectrum &GetKr() const { return Kr; }

private:
	Spectrum Kr;
	bool reflectionSpecularBounce;
};

class MatteMirrorMaterial : public SurfaceMaterial {
public:
	MatteMirrorMaterial(const Spectrum &col, const Spectrum refl, bool reflSpecularBounce) :
		matte(col), mirror(refl, reflSpecularBounce) {
		matteFilter = matte.GetKd().Filter();
		mirrorFilter = mirror.GetKr().Filter();
		totFilter = matteFilter + mirrorFilter;

		mattePdf = matteFilter / totFilter;
		mirrorPdf = mirrorFilter / totFilter;
	}

	bool IsDiffuse() const { return true; }
	bool IsSpecular() const { return true; }

	Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		return matte.f(wo, wi, N) * mattePdf;
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, float *pdf, bool &specularBounce) const {
		const float comp = u2 * totFilter;

		if (comp > matteFilter) {
			const Spectrum f = mirror.Sample_f(wo, wi, N, shadeN, u0, u1, u2, pdf, specularBounce);
			*pdf *= mirrorPdf;

			return f;
		} else {
			const Spectrum f = matte.Sample_f(wo, wi, N, shadeN, u0, u1, u2, pdf, specularBounce);
			*pdf *= mattePdf;

			return f;
		}
	}

private:
	MatteMaterial matte;
	MirrorMaterial mirror;
	float matteFilter, mirrorFilter, totFilter, mattePdf, mirrorPdf;
};

class GlassMaterial : public SurfaceMaterial {
public:
	GlassMaterial(const Spectrum &refl, const Spectrum &refrct, const float outsideIorFact,
			const float iorFact, bool reflSpecularBounce, bool transSpecularBounce) {
		Krefl = refl;
		Krefrct = refrct;
		ousideIor = outsideIorFact;
		ior = iorFact;

		reflectionSpecularBounce = reflSpecularBounce;
		transmitionSpecularBounce = transSpecularBounce;

		const float nc = ousideIor;
		const float nt = ior;
		const float a = nt - nc;
		const float b = nt + nc;
		R0 = a * a / (b * b);
	}

	bool IsDiffuse() const { return false; }
	bool IsSpecular() const { return true; }

	Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		throw std::runtime_error("Internal error, called GlassMaterial::f()");
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, float *pdf, bool &specularBounce) const {
		Vector reflDir = -wo;
		reflDir = reflDir - (2.f * Dot(N, reflDir)) * Vector(N);

		// Ray from outside going in ?
		const bool into = (Dot(N, shadeN) > 0);

		const float nc = ousideIor;
		const float nt = ior;
		const float nnt = into ? (nc / nt) : (nt / nc);
		const float ddn = Dot(-wo, shadeN);
		const float cos2t = 1.f - nnt * nnt * (1.f - ddn * ddn);

		// Total internal reflection
		if (cos2t < 0.f) {
			(*wi) = reflDir;
			*pdf = 1.f;
			specularBounce = reflectionSpecularBounce;

			return Krefl; // / (-ddn);
		}

		const float kk = (into ? 1.f : -1.f) * (ddn * nnt + sqrt(cos2t));
		const Vector nkk = kk * Vector(N);
		const Vector transDir = Normalize(nnt * (-wo) - nkk);

		const float c = 1.f - (into ? -ddn : Dot(transDir, N));

		const float Re = R0 + (1.f - R0) * c * c * c * c * c;
		const float Tr = 1.f - Re;
		const float P = .25f + .5f * Re;

		if (u0 < P) {
			(*wi) = reflDir;

			*pdf = P / Re;
			specularBounce = reflectionSpecularBounce;

			// I don't multiply for Dot(wi, shadeN) in order to avoid fireflies
			return Krefl; // / (-ddn);
		} else {
			(*wi) = transDir;

			*pdf = (1.f - P) / Tr;
			specularBounce = transmitionSpecularBounce;

			return Krefrct / (-ddn);
		}
	}

	const Spectrum &GetKrefl() const { return Krefl; }
	const Spectrum &GetKrefrct() const { return Krefrct; }

private:
	Spectrum Krefl, Krefrct;
	float ousideIor, ior;
	float R0;
	bool reflectionSpecularBounce, transmitionSpecularBounce;
};

class MetalMaterial : public SurfaceMaterial {
public:
	MetalMaterial(const Spectrum &refl, const float exp, bool reflSpecularBounce) {
		Kr = refl;
		exponent = 1.f / (exp + 1.f);
		reflectionSpecularBounce = reflSpecularBounce;
	}

	bool IsDiffuse() const { return false; }
	bool IsSpecular() const { return true; }

	Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		throw std::runtime_error("Internal error, called MetalMaterial::f()");
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, float *pdf, bool &specularBounce) const {
		const float phi = 2.f * M_PI * u0;
		const float cosTheta = powf(1.f - u1, exponent);
		const float sinTheta = sqrtf(1.f - cosTheta * cosTheta);
		const float x = cosf(phi) * sinTheta;
		const float y = sinf(phi) * sinTheta;
		const float z = cosTheta;

		const Vector dir = -wo;
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

		(*wi) = x * u + y * v + z * w;

		if (Dot(*wi, shadeN) > 0.f) {
			specularBounce = reflectionSpecularBounce;
			*pdf = 1.f;

			return Kr / (-dp);
		} else {
			*pdf = 0.f;

			return Spectrum();
		}
	}

	const Spectrum &GetKr() const { return Kr; }

private:
	Spectrum Kr;
	float exponent;
	bool reflectionSpecularBounce;
};

class MatteMetalMaterial : public SurfaceMaterial {
public:
	MatteMetalMaterial(const Spectrum &col, const Spectrum refl, const float exp, bool reflSpecularBounce) :
		matte(col), metal(refl, exp, reflSpecularBounce) {
		matteFilter = matte.GetKd().Filter();
		metalFilter = metal.GetKr().Filter();
		totFilter = matteFilter + metalFilter;

		mattePdf = matteFilter / totFilter;
		mirrorPdf = metalFilter / totFilter;
	}

	bool IsDiffuse() const { return true; }
	bool IsSpecular() const { return true; }

	Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		return matte.f(wo, wi, N) * mattePdf;
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, float *pdf, bool &specularBounce) const {
		const float comp = u2 * totFilter;

		if (comp > matteFilter) {
			const Spectrum f = metal.Sample_f(wo, wi, N, shadeN, u0, u1, u2, pdf, specularBounce);
			*pdf *= mirrorPdf;

			return f;
		} else {
			const Spectrum f = matte.Sample_f(wo, wi, N, shadeN, u0, u1, u2, pdf, specularBounce);
			*pdf *= mattePdf;

			return f;
		}
	}

private:
	MatteMaterial matte;
	MetalMaterial metal;
	float matteFilter, metalFilter, totFilter, mattePdf, mirrorPdf;
};

#endif	/* _MATERIAL_H */
