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
	virtual bool IsDelta() const = 0;
	virtual bool IsShadowTransparent() const { return false; }
	virtual BSDFEvent GetEventTypes() const = 0;

	virtual const Spectrum &GetSahdowTransparency() const {
		throw std::runtime_error("Internal error, called Material::GetSahdowTransparency()");
	}
};

class LightMaterial : public Material {
public:
	bool IsLightSource() const { return true; }
	bool IsDelta() const { return false; }
	BSDFEvent GetEventTypes() const { return NONE; };
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
	bool IsDelta() const { return false; }

	virtual Spectrum Evaluate(const bool fromLight,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const = 0;

	virtual Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const = 0;

	virtual void Pdf(const bool fromLight, const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const = 0;
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
	BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT; };

	const Spectrum &GetKd() const { return Kd; }

	Spectrum Evaluate(const bool fromLight,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const;

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
	BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT; };

	bool IsDelta() const { return true; }

	const Spectrum &GetKr() const { return Kr; }
	bool HasSpecularBounceEnabled() const { return reflectionSpecularBounce; }

	Spectrum Evaluate(const bool fromLight,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

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
	BSDFEvent GetEventTypes() const { return DIFFUSE | SPECULAR | REFLECT; };

	const MatteMaterial &GetMatte() const { return matte; }
	const MirrorMaterial &GetMirror() const { return mirror; }
	float GetMatteFilter() const { return matteFilter; }
	float GetTotFilter() const { return totFilter; }
	float GetMattePdf() const { return mattePdf; }
	float GetMirrorPdf() const { return mirrorPdf; }

	Spectrum Evaluate(const bool fromLight,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const;

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
	BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	bool IsDelta() const { return true; }

	const Spectrum &GetKrefl() const { return Krefl; }
	const Spectrum &GetKrefrct() const { return Krefrct; }
	const float GetOutsideIOR() const { return ousideIor; }
	const float GetIOR() const { return ior; }
	const float GetR0() const { return R0; }

	bool HasReflSpecularBounceEnabled() const { return reflectionSpecularBounce; }
	bool HasRefrctSpecularBounceEnabled() const { return transmitionSpecularBounce; }

	Spectrum Evaluate(const bool fromLight,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

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
	BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };
	
	const Spectrum &GetKr() const { return Kr; }
	float GetExp() const { return exponent; }
	bool HasSpecularBounceEnabled() const { return reflectionSpecularBounce; }

	Spectrum Evaluate(const bool fromLight,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	static Vector GlossyReflection(const Vector &fixedDir, const float exponent,
			const float u0, const float u1);
private:
	Spectrum Kr;
	float exponent;
	bool reflectionSpecularBounce;
};

//------------------------------------------------------------------------------
// MatteMetal material
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
	BSDFEvent GetEventTypes() const { return DIFFUSE | GLOSSY | REFLECT; };

	const MatteMaterial &GetMatte() const { return matte; }
	const MetalMaterial &GetMetal() const { return metal; }
	float GetMatteFilter() const { return matteFilter; }
	float GetTotFilter() const { return totFilter; }
	float GetMattePdf() const { return mattePdf; }
	float GetMetalPdf() const { return metalPdf; }

	Spectrum Evaluate(const bool fromLight,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const;

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
	BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	bool IsDelta() const { return true; }
	bool IsShadowTransparent() const { return true; }
	const Spectrum &GetSahdowTransparency() const { return Ktrans; }

	const Spectrum &GetKrefl() const { return Krefl; }
	const Spectrum &GetKrefrct() const { return Ktrans; }
	const float GetTransFilter() const { return transFilter; }
	const float GetTotFilter() const { return totFilter; }
	const float GetReflPdf() const { return reflPdf; }
	const float GetTransPdf() const { return transPdf; }

	bool HasReflSpecularBounceEnabled() const { return reflectionSpecularBounce; }
	bool HasRefrctSpecularBounceEnabled() const { return transmitionSpecularBounce; }

	Spectrum Evaluate(const bool fromLight,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

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
	BSDFEvent GetEventTypes() const { return DIFFUSE | GLOSSY | REFLECT; };

	const Spectrum &GetKrefl() const { return Krefl; }
	const Spectrum &GetKd() const { return Kdiff; }
	float GetExp() const { return exponent; }
	float GetR0() const { return R0; }
	bool HasSpecularBounceEnabled() const { return reflectionSpecularBounce; }

	Spectrum Evaluate(const bool fromLight,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const;

private:
	Spectrum Krefl;
	Spectrum Kdiff, KdiffOverPI;
	float exponent;
	float R0;
	bool reflectionSpecularBounce;
};

} }

#endif	/* _LUXRAYS_SDL_MATERIAL_H */
