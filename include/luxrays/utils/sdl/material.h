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
	MATTE, MIRROR, GLASS, METAL, ARCHGLASS
};

class Material {
public:
	Material(const Spectrum &emitted) : emittedRadiance(emitted) { }
	virtual ~Material() { }

	virtual MaterialType GetType() const = 0;

	virtual bool IsLightSource() const {
		return (emittedRadiance.r > 0.f) || (emittedRadiance.g > 0.f) || (emittedRadiance.g > 0.f);
	}
	virtual bool IsDelta() const { return false; }
	virtual bool IsShadowTransparent() const { return false; }
	virtual BSDFEvent GetEventTypes() const = 0;
	virtual const Spectrum &GetSahdowTransparency() const {
		throw std::runtime_error("Internal error, called Material::GetSahdowTransparency()");
	}

	const Spectrum &GetEmittedRadiance() const { return emittedRadiance; }

	virtual Spectrum Evaluate(const bool fromLight,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const = 0;

	virtual Spectrum Sample(const bool fromLight,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const = 0;

	virtual void Pdf(const bool fromLight, const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const = 0;

protected:
	Spectrum emittedRadiance;
};

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

class MatteMaterial : public Material {
public:
	MatteMaterial(const Spectrum &emitted, const Spectrum &col) : Material(emitted) {
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

class MirrorMaterial : public Material {
public:
	MirrorMaterial(const Spectrum &emitted, const Spectrum &refl) : Material(emitted) {
		Kr = refl;
	}

	MaterialType GetType() const { return MIRROR; }
	BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT; };

	bool IsDelta() const { return true; }

	const Spectrum &GetKr() const { return Kr; }

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
// Glass material
//------------------------------------------------------------------------------

class GlassMaterial : public Material {
public:
	GlassMaterial(const Spectrum &emitted,
			const Spectrum &refl, const Spectrum &refrct,
			const float outsideIorFact,	const float iorFact) : Material(emitted) {
		Krefl = refl;
		Krefrct = refrct;
		ousideIor = outsideIorFact;
		ior = iorFact;

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
};

//------------------------------------------------------------------------------
// Metal material
//------------------------------------------------------------------------------

class MetalMaterial : public Material {
public:
	MetalMaterial(const Spectrum &emitted, const Spectrum &refl,
			const float exp) : Material(emitted) {
		Kr = refl;
		exponent = 1.f / (exp + 1.f);
	}

	MaterialType GetType() const { return METAL; }
	BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

	const Spectrum &GetKr() const { return Kr; }
	float GetExp() const { return exponent; }

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
};

//------------------------------------------------------------------------------
// Architectural glass material
//------------------------------------------------------------------------------

class ArchGlassMaterial : public Material {
public:
	ArchGlassMaterial(const Spectrum &emitted,
			const Spectrum &refl, const Spectrum &refrct) : Material(emitted) {
		Krefl = refl;
		Ktrans = refrct;

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
};

} }

#endif	/* _LUXRAYS_SDL_MATERIAL_H */
