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

#ifndef _LUXRAYS_SDL_MATERIAL_H
#define	_LUXRAYS_SDL_MATERIAL_H

#include <vector>

#include "luxrays/utils/core/spectrum.h"
#include "luxrays/utils/sdl/bsdfevents.h"
#include "luxrays/utils/core/exttrianglemesh.h"
#include "luxrays/utils/core/mc.h"

namespace luxrays { namespace sdl {

class Scene;

enum MaterialType {
	MATTE, AREALIGHT, MIRROR, MATTEMIRROR, GLASS, METAL, MATTEMETAL,
	ARCHGLASS, ALLOY
};

class Material {
public:
	virtual ~Material() { }

	virtual MaterialType GetType() const = 0;

	virtual bool IsLightSource() const = 0;
	virtual bool IsDiffuse() const = 0;
	virtual bool IsSpecular() const = 0; // TODO: rename to IsDelta
	virtual bool IsShadowTransparent() const { return false; }

	virtual const Spectrum &GetSahdowTransparency() const {
		throw std::runtime_error("Internal error, called Material::GetSahdowTransparency()");
	}
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

	MaterialType GetType() const { return AREALIGHT; }

	const Spectrum &GetGain() const { return gain; }

private:
	Spectrum gain;
};

class SurfaceMaterial : public Material {
public:
	bool IsLightSource() const { return false; }

	virtual Spectrum Evaluate(const bool fromLight, const bool into,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const {
		throw std::runtime_error("Internal error, called SurfaceMaterial::Evaluate()");
	}

	virtual Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const {
		throw std::runtime_error("Internal error, called SurfaceMaterial::Sample()");
	}
};

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

class MatteMaterial : public SurfaceMaterial {
public:
	MatteMaterial(const Spectrum &col) {
		Kd = col;
		KdOverPI = Kd * INV_PI;
	}

	MaterialType GetType() const { return MATTE; }

	bool IsDiffuse() const { return true; }
	bool IsSpecular() const { return false; }

	const Spectrum &GetKd() const { return Kd; }

	//--------------------------------------------------------------------------
	// Old interface
	//--------------------------------------------------------------------------

	Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		if (Dot(N, wi) > 0.f)
			return KdOverPI;
		else
			return Spectrum();
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, const bool onlySpecular,
		float *pdf, bool &specularBounce) const {
		if (onlySpecular) {
			*pdf = 0.f;
			return Spectrum();
		}

		Vector dir = CosineSampleHemisphere(u0, u1);
		*pdf = dir.z * INV_PI;

		Vector v1, v2;
		CoordinateSystem(Vector(shadeN), &v1, &v2);

		dir = Vector(
				v1.x * dir.x + v2.x * dir.y + shadeN.x * dir.z,
				v1.y * dir.x + v2.y * dir.y + shadeN.y * dir.z,
				v1.z * dir.x + v2.z * dir.y + shadeN.z * dir.z);

		(*wi) = dir;

		const float dp = Dot(shadeN, *wi);
		// Using 0.0001 instead of 0.0 to cut down fireflies
		if (dp <= 0.0001f) {
			*pdf = 0.f;
			return Spectrum();
		}
		*pdf /=  dp;

		specularBounce = false;

		return KdOverPI;
	}

	//--------------------------------------------------------------------------
	// New interface
	//--------------------------------------------------------------------------
	
	Spectrum Evaluate(const bool fromLight, const bool into,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;

private:
	Spectrum Kd, KdOverPI;
};

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

class MirrorMaterial : public SurfaceMaterial {
public:
	MirrorMaterial(const Spectrum &refl, bool reflSpecularBounce) {
		Kr = refl;
		reflectionSpecularBounce = reflSpecularBounce;
	}

	MaterialType GetType() const { return MIRROR; }

	bool IsDiffuse() const { return false; }
	bool IsSpecular() const { return true; }

	const Spectrum &GetKr() const { return Kr; }

	//--------------------------------------------------------------------------
	// Old interface
	//--------------------------------------------------------------------------

	Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		throw std::runtime_error("Internal error, called MirrorMaterial::f()");
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, const bool onlySpecular,
		float *pdf, bool &specularBounce) const {
		const Vector dir = -wo;
		const float dp = Dot(shadeN, dir);
		(*wi) = dir - (2.f * dp) * Vector(shadeN);

		specularBounce = reflectionSpecularBounce;
		*pdf = 1.f;

		return Kr;
	}

	bool HasSpecularBounceEnabled() const { return reflectionSpecularBounce; }

	//--------------------------------------------------------------------------
	// New interface
	//--------------------------------------------------------------------------

	Spectrum Evaluate(const bool fromLight, const bool into,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdf, float *cosSampledDir, BSDFEvent *event) const;

private:
	Spectrum Kr;
	bool reflectionSpecularBounce;
};

//------------------------------------------------------------------------------
// MatteMirror material
//------------------------------------------------------------------------------

class MatteMirrorMaterial : public SurfaceMaterial {
public:
	MatteMirrorMaterial(const Spectrum &col, const Spectrum refl, bool reflSpecularBounce) :
		matte(col), mirror(refl, reflSpecularBounce) {
		matteFilter = matte.GetKd().Filter();
		const float mirrorFilter = mirror.GetKr().Filter();
		totFilter = matteFilter + mirrorFilter;

		mattePdf = matteFilter / totFilter;
		mirrorPdf = mirrorFilter / totFilter;
	}

	MaterialType GetType() const { return MATTEMIRROR; }

	bool IsDiffuse() const { return true; }
	bool IsSpecular() const { return true; }

	const MatteMaterial &GetMatte() const { return matte; }
	const MirrorMaterial &GetMirror() const { return mirror; }
	float GetMatteFilter() const { return matteFilter; }
	float GetTotFilter() const { return totFilter; }
	float GetMattePdf() const { return mattePdf; }
	float GetMirrorPdf() const { return mirrorPdf; }

	//--------------------------------------------------------------------------
	// Old interface
	//--------------------------------------------------------------------------

	Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		return matte.f(wo, wi, N) * mattePdf;
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, const bool onlySpecular,
		float *pdf, bool &specularBounce) const {
		const float comp = u2 * totFilter;

		if (onlySpecular || (comp > matteFilter)) {
			const Spectrum f = mirror.Sample_f(wo, wi, N, shadeN, u0, u1, u2, onlySpecular, pdf, specularBounce);
			*pdf *= mirrorPdf;

			return f;
		} else {
			const Spectrum f = matte.Sample_f(wo, wi, N, shadeN, u0, u1, u2, onlySpecular, pdf, specularBounce);
			*pdf *= mattePdf;

			return f;
		}
	}

	//--------------------------------------------------------------------------
	// New interface
	//--------------------------------------------------------------------------

	Spectrum Evaluate(const bool fromLight, const bool into,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdf, float *cosSampledDir, BSDFEvent *event) const;

private:
	MatteMaterial matte;
	MirrorMaterial mirror;
	float matteFilter, totFilter, mattePdf, mirrorPdf;
};

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

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

	MaterialType GetType() const { return GLASS; }

	bool IsDiffuse() const { return false; }
	bool IsSpecular() const { return true; }

	const Spectrum &GetKrefl() const { return Krefl; }
	const Spectrum &GetKrefrct() const { return Krefrct; }
	const float GetOutsideIOR() const { return ousideIor; }
	const float GetIOR() const { return ior; }
	const float GetR0() const { return R0; }

	//--------------------------------------------------------------------------
	// Old interface
	//--------------------------------------------------------------------------

	Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		throw std::runtime_error("Internal error, called GlassMaterial::f()");
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, const bool onlySpecular,
		float *pdf, bool &specularBounce) const {
		const Vector rayDir = -wo;
		const Vector reflDir = rayDir - (2.f * Dot(N, rayDir)) * Vector(N);

		// Ray from outside going in ?
		const bool into = (Dot(N, shadeN) > 0.f);

		const float nc = ousideIor;
		const float nt = ior;
		const float nnt = into ? (nc / nt) : (nt / nc);
		const float ddn = Dot(rayDir, shadeN);
		const float cos2t = 1.f - nnt * nnt * (1.f - ddn * ddn);

		// Total internal reflection
		if (cos2t < 0.f) {
			(*wi) = reflDir;
			*pdf = 1.f;
			specularBounce = reflectionSpecularBounce;

			return Krefl;
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
				*pdf = 0.f;
				return Spectrum();
			} else {
				(*wi) = reflDir;
				*pdf = 1.f;
				specularBounce = reflectionSpecularBounce;

				return Krefl;
			}
		} else if (Re == 0.f) {
			(*wi) = transDir;
			*pdf = 1.f;
			specularBounce = transmitionSpecularBounce;

			return Krefrct;
		} else if (u0 < P) {
			(*wi) = reflDir;
			*pdf = P / Re;
			specularBounce = reflectionSpecularBounce;

			return Krefl;
		} else {
			(*wi) = transDir;
			*pdf = (1.f - P) / Tr;
			specularBounce = transmitionSpecularBounce;

			return Krefrct;
		}
	}

	bool HasReflSpecularBounceEnabled() const { return reflectionSpecularBounce; }
	bool HasRefrctSpecularBounceEnabled() const { return transmitionSpecularBounce; }

	//--------------------------------------------------------------------------
	// New interface
	//--------------------------------------------------------------------------

	Spectrum Evaluate(const bool fromLight, const bool into,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdf, float *cosSampledDir, BSDFEvent *event) const;

private:
	Spectrum Krefl, Krefrct;
	float ousideIor, ior;
	float R0;
	bool reflectionSpecularBounce, transmitionSpecularBounce;
};

//------------------------------------------------------------------------------
// Metal material
//------------------------------------------------------------------------------

class MetalMaterial : public SurfaceMaterial {
public:
	MetalMaterial(const Spectrum &refl, const float exp, bool reflSpecularBounce) {
		Kr = refl;
		exponent = 1.f / (exp + 1.f);
		reflectionSpecularBounce = reflSpecularBounce;
	}

	MaterialType GetType() const { return METAL; }

	bool IsDiffuse() const { return false; }
	bool IsSpecular() const { return true; }
	
	const Spectrum &GetKr() const { return Kr; }
	float GetExp() const { return exponent; }
	bool HasSpecularBounceEnabled() const { return reflectionSpecularBounce; }

	Spectrum Evaluate(const bool fromLight, const bool into,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdf, float *cosSampledDir, BSDFEvent *event) const;

	static Vector GlossyReflection(const Vector &fixedDir, const float exponent,
			const float u0, const float u1);
private:
	Spectrum Kr;
	float exponent;
	bool reflectionSpecularBounce;
};

//------------------------------------------------------------------------------
// MetalMetal material
//------------------------------------------------------------------------------

class MatteMetalMaterial : public SurfaceMaterial {
public:
	MatteMetalMaterial(const Spectrum &col, const Spectrum refl, const float exp, bool reflSpecularBounce) :
		matte(col), metal(refl, exp, reflSpecularBounce) {
		matteFilter = matte.GetKd().Filter();
		const float metalFilter = metal.GetKr().Filter();
		totFilter = matteFilter + metalFilter;
		
		mattePdf = matteFilter / totFilter;
		metalPdf = metalFilter / totFilter;
	}

	MaterialType GetType() const { return MATTEMETAL; }

	bool IsDiffuse() const { return true; }
	bool IsSpecular() const { return true; }

	/*Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		return matte.f(wo, wi, N) * mattePdf;
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, const bool onlySpecular,
		float *pdf, bool &specularBounce) const {
		const float comp = u2 * totFilter;

		if (onlySpecular || (comp > matteFilter)) {
			const Spectrum f = metal.Sample_f(wo, wi, N, shadeN, u0, u1, u2, onlySpecular, pdf, specularBounce);
			*pdf *= metalPdf;

			return f;
		} else {
			const Spectrum f = matte.Sample_f(wo, wi, N, shadeN, u0, u1, u2, onlySpecular, pdf, specularBounce);
			*pdf *= mattePdf;

			return f;
		}
	}*/
	const MatteMaterial &GetMatte() const { return matte; }
	const MetalMaterial &GetMetal() const { return metal; }
	float GetMatteFilter() const { return matteFilter; }
	float GetTotFilter() const { return totFilter; }
	float GetMattePdf() const { return mattePdf; }
	float GetMetalPdf() const { return metalPdf; }

private:
	MatteMaterial matte;
	MetalMaterial metal;
	float matteFilter, totFilter, mattePdf, metalPdf;
};

//------------------------------------------------------------------------------
// Architectural glass material
//------------------------------------------------------------------------------

class ArchGlassMaterial : public SurfaceMaterial {
public:
	ArchGlassMaterial(const Spectrum &refl, const Spectrum &refrct,
			bool reflSpecularBounce, bool transSpecularBounce) {
		Krefl = refl;
		Ktrans = refrct;

		reflectionSpecularBounce = reflSpecularBounce;
		transmitionSpecularBounce = transSpecularBounce;

		const float reflFilter = Krefl.Filter();
		transFilter = Ktrans.Filter();
		totFilter = reflFilter + transFilter;

		reflPdf = reflFilter / totFilter;
		transPdf = transFilter / totFilter;
	}

	MaterialType GetType() const { return ARCHGLASS; }

	bool IsDiffuse() const { return false; }
	bool IsSpecular() const { return true; }
	bool IsShadowTransparent() const { return true; }
	const Spectrum &GetSahdowTransparency() const { return Ktrans; }

	const Spectrum &GetKrefl() const { return Krefl; }
	const Spectrum &GetKrefrct() const { return Ktrans; }
	const float GetTransFilter() const { return transFilter; }
	const float GetTotFilter() const { return totFilter; }
	const float GetReflPdf() const { return reflPdf; }
	const float GetTransPdf() const { return transPdf; }

	//--------------------------------------------------------------------------
	// Old interface
	//--------------------------------------------------------------------------

	Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		throw std::runtime_error("Internal error, called ArchGlassMaterial::f()");
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, const bool onlySpecular,
		float *pdf, bool &specularBounce) const {
		// Ray from outside going in ?
		const bool into = (Dot(N, shadeN) > 0.f);

		if (!into) {
			// No internal reflections
			(*wi) = -wo;
			*pdf = 1.f;
			specularBounce = reflectionSpecularBounce;

			return Ktrans;
		} else {
			// RR to choose if reflect the ray or go trough the glass
			const float comp = u0 * totFilter;
			if (comp > transFilter) {
				const Vector mwo = -wo;
				(*wi) = mwo - (2.f * Dot(N, mwo)) * Vector(N);
				*pdf = reflPdf;
				specularBounce = reflectionSpecularBounce;

				return Krefl;
			} else {
				(*wi) = -wo;
				*pdf = transPdf;
				specularBounce = transmitionSpecularBounce;

				return Ktrans;
			}
		}
	}

	bool HasReflSpecularBounceEnabled() const { return reflectionSpecularBounce; }
	bool HasRefrctSpecularBounceEnabled() const { return transmitionSpecularBounce; }

	//--------------------------------------------------------------------------
	// New interface
	//--------------------------------------------------------------------------

	Spectrum Evaluate(const bool fromLight, const bool into,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdf, float *cosSampledDir, BSDFEvent *event) const;

private:
	Spectrum Krefl, Ktrans;
	float transFilter, totFilter, reflPdf, transPdf;
	bool reflectionSpecularBounce, transmitionSpecularBounce;
};

//------------------------------------------------------------------------------
// Alloy material
//------------------------------------------------------------------------------

class AlloyMaterial : public SurfaceMaterial {
public:
	AlloyMaterial(const Spectrum &col, const Spectrum &refl,
			const float exp, const float schlickTerm, bool reflSpecularBounce) {
		Krefl = refl;
		Kdiff = col;
		KdiffOverPI = Kdiff * INV_PI;
		R0 = schlickTerm;
		exponent = 1.f / (exp + 1.f);
		
		reflectionSpecularBounce = reflSpecularBounce;
	}

	MaterialType GetType() const { return ALLOY; }

	bool IsDiffuse() const { return true; }
	bool IsSpecular() const { return true; }

	/*Spectrum f(const Vector &wo, const Vector &wi, const Normal &N) const {
		// Schilick's approximation
		const float c = 1.f - Dot(wo, N);
		const float Re = R0 + (1.f - R0) * c * c * c * c * c;

		const float P = .25f + .5f * Re;

		return KdiffOverPI * (1.f - Re) / (1.f - P);
	}

	Spectrum Sample_f(const Vector &wo, Vector *wi, const Normal &N, const Normal &shadeN,
		const float u0, const float u1,  const float u2, const bool onlySpecular,
		float *pdf, bool &specularBounce) const {
		// Schilick's approximation
		const float c = 1.f - Dot(wo, shadeN);
		const float Re = R0 + (1.f - R0) * c * c * c * c * c;

		const float P = .25f + .5f * Re;

		if (onlySpecular || (u2 < P)) {
			(*wi) = MetalMaterial::GlossyReflection(wo, exponent, shadeN, u0, u1);
			*pdf = P / Re;
			specularBounce = reflectionSpecularBounce;

			return Re * Krefl;
		} else {
			Vector dir = CosineSampleHemisphere(u0, u1);
			*pdf = dir.z * INV_PI;

			Vector v1, v2;
			CoordinateSystem(Vector(shadeN), &v1, &v2);

			dir = Vector(
					v1.x * dir.x + v2.x * dir.y + shadeN.x * dir.z,
					v1.y * dir.x + v2.y * dir.y + shadeN.y * dir.z,
					v1.z * dir.x + v2.z * dir.y + shadeN.z * dir.z);

			(*wi) = dir;

			const float dp = Dot(shadeN, *wi);
			// Using 0.0001 instead of 0.0 to cut down fireflies
			if (dp <= 0.0001f) {
				*pdf = 0.f;
				return Spectrum();
			}
			*pdf /=  dp;

			const float iRe = 1.f - Re;
			*pdf *= (1.f - P) / iRe;
			specularBounce = false;

			return iRe * Kdiff;

		}
	}*/

	const Spectrum &GetKrefl() const { return Krefl; }
	const Spectrum &GetKd() const { return Kdiff; }
	float GetExp() const { return exponent; }
	float GetR0() const { return R0; }
	bool HasSpecularBounceEnabled() const { return reflectionSpecularBounce; }

private:
	Spectrum Krefl;
	Spectrum Kdiff, KdiffOverPI;
	float exponent;
	float R0;
	bool reflectionSpecularBounce;
};

} }

#endif	/* _LUXRAYS_SDL_MATERIAL_H */
