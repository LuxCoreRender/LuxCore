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
#include "luxrays/utils/sdl/texture.h"
#include "texture.h"

namespace luxrays { namespace sdl {

class Scene;

enum MaterialType {
	MATTE, MIRROR, GLASS, METAL, ARCHGLASS, MIX
};

class Material {
public:
	Material(const Texture *emitted, const Texture *bump, const Texture *normal) :
		emittedTex(emitted), bumpTex(bump), normalTex(normal) { }
	virtual ~Material() { }

	virtual MaterialType GetType() const = 0;

	virtual bool IsLightSource() const {
		return (emittedTex != NULL);
	}
	virtual bool HasBumpTex() const { 
		return (bumpTex != NULL);
	}
	virtual bool HasNormalTex() const { 
		return (normalTex != NULL);
	}

	virtual bool IsDelta() const { return false; }
	virtual bool IsShadowTransparent() const { return false; }
	virtual BSDFEvent GetEventTypes() const = 0;
	virtual Spectrum GetSahdowTransparency(const UV &uv) const {
		throw std::runtime_error("Internal error, called Material::GetSahdowTransparency()");
	}

	virtual Spectrum GetEmittedRadiance(const UV &uv) const {
		if (emittedTex)
			return emittedTex->GetColorValue(uv);
		else
			return Spectrum();
	}

	const Texture *GetEmitTexture() const { return emittedTex; }
	const Texture *GetBumpTexture() const { return bumpTex; }
	const Texture *GetNormalTexture() const { return normalTex; }

	virtual Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const = 0;

	virtual Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const = 0;

	virtual void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const = 0;

protected:
	const Texture *emittedTex;
	const Texture *bumpTex;
	const Texture *normalTex;
};

//------------------------------------------------------------------------------
// MaterialDefinitions
//------------------------------------------------------------------------------

class MaterialDefinitions {
public:
	MaterialDefinitions() { }
	~MaterialDefinitions() {
		for (std::vector<Material *>::const_iterator it = mats.begin(); it != mats.end(); ++it)
			delete (*it);
	}

	bool IsMaterialDefined(const std::string &name) const {
		return (matsByName.count(name) > 0);
	}
	void DefineMaterial(const std::string &name, Material *t) {
		mats.push_back(t);
		matsByName.insert(std::make_pair(name, t));
		indexByName.insert(std::make_pair(name, mats.size() - 1));
	}

	Material *GetMaterial(const std::string &name) {
		// Check if the material has been already defined
		std::map<std::string, Material *>::const_iterator it = matsByName.find(name);

		if (it == matsByName.end())
			throw std::runtime_error("Reference to an undefined material: " + name);
		else
			return it->second;
	}
	Material *GetMaterial(const u_int index) {
		return mats[index];
	}
	u_int GetMaterialIndex(const std::string &name) {
		// Check if the material has been already defined
		std::map<std::string, u_int>::const_iterator it = indexByName.find(name);

		if (it == indexByName.end())
			throw std::runtime_error("Reference to an undefined material: " + name);
		else
			return it->second;
	}

	u_int GetSize()const { return static_cast<u_int>(mats.size()); }
  
private:

	std::vector<Material *> mats;
	std::map<std::string, Material *> matsByName;
	std::map<std::string, u_int> indexByName;
};

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

class MatteMaterial : public Material {
public:
	MatteMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *col) : Material(emitted, bump, normal), Kd(col) { }

	MaterialType GetType() const { return MATTE; }
	BSDFEvent GetEventTypes() const { return DIFFUSE | REFLECT; };

	const Texture *GetKd() const { return Kd; }

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const;

private:
	const Texture *Kd;
};

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

class MirrorMaterial : public Material {
public:
	MirrorMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
		const Texture *refl) : Material(emitted, bump, normal), Kr(refl) { }

	MaterialType GetType() const { return MIRROR; }
	BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT; };

	bool IsDelta() const { return true; }

	const Texture *GetKr() const { return Kr; }

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

private:
	const Texture *Kr;
};

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

class GlassMaterial : public Material {
public:
	GlassMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *refrct,
			const Texture *outsideIorFact, const Texture *iorFact) :
			Material(emitted, bump, normal),
			Krefl(refl), Krefrct(refrct), ousideIor(outsideIorFact), ior(iorFact) { }

	MaterialType GetType() const { return GLASS; }
	BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	bool IsDelta() const { return true; }

	const Texture *GetKrefl() const { return Krefl; }
	const Texture *GetKrefrct() const { return Krefrct; }
	const Texture *GetOutsideIOR() const { return ousideIor; }
	const Texture *GetIOR() const { return ior; }

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

private:
	const Texture *Krefl;
	const Texture *Krefrct;
	const Texture *ousideIor;
	const Texture *ior;
};

//------------------------------------------------------------------------------
// Architectural glass material
//------------------------------------------------------------------------------

class ArchGlassMaterial : public Material {
public:
	ArchGlassMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *refrct) :
			Material(emitted, bump, normal), Krefl(refl), Krefrct(refrct) { }

	MaterialType GetType() const { return ARCHGLASS; }
	BSDFEvent GetEventTypes() const { return SPECULAR | REFLECT | TRANSMIT; };

	bool IsDelta() const { return true; }
	bool IsShadowTransparent() const { return true; }
	Spectrum GetSahdowTransparency(const UV &uv) const { return Krefrct->GetColorValue(uv); }

	const Texture *GetKrefl() const { return Krefl; }
	const Texture *GetKrefrct() const { return Krefrct; }

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

private:
	const Texture *Krefl;
	const Texture *Krefrct;
};

//------------------------------------------------------------------------------
// Metal material
//------------------------------------------------------------------------------

class MetalMaterial : public Material {
public:
	MetalMaterial(const Texture *emitted, const Texture *bump, const Texture *normal,
			const Texture *refl, const Texture *exp) : Material(emitted, bump, normal),
			Kr(refl), exponent(exp) { }

	MaterialType GetType() const { return METAL; }
	BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

	const Texture *GetKr() const { return Kr; }
	const Texture *GetExp() const { return exponent; }

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}

	static Vector GlossyReflection(const Vector &fixedDir, const float exponent,
			const float u0, const float u1);
private:
	const Texture *Kr;
	const Texture *exponent;
};

//------------------------------------------------------------------------------
// Mix material
//------------------------------------------------------------------------------

class MixMaterial : public Material {
public:
	MixMaterial(const Material *mA, const Material *mB, const Texture *mix) : Material(NULL, NULL, NULL),
			matA(mA), matB(mB), mixFactor(mix) { }

	MaterialType GetType() const { return MIX; }
	BSDFEvent GetEventTypes() const { return (matA->GetEventTypes() | matB->GetEventTypes()); };

	bool IsLightSource() const {
		return (matA->IsLightSource() || matB->IsLightSource());
	}
	bool HasBumpTex() const { 
		return (matA->HasBumpTex() || matB->HasBumpTex());
	}
	bool HasNormalTex() const {
		return (matA->HasNormalTex() || matB->HasNormalTex());
	}

	bool IsDelta()  {
		return (matA->IsDelta() && matB->IsDelta());
	}
	bool IsShadowTransparent() const {
		return (matA->IsShadowTransparent() || matB->IsShadowTransparent());
	}
	Spectrum GetSahdowTransparency(const UV &uv) const;
	Spectrum GetEmittedRadiance(const UV &uv) const;

	Spectrum Evaluate(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	Spectrum Sample(const bool fromLight, const UV &uv,
		const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const;
	void Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const;

private:
	const Material *matA;
	const Material *matB;
	const Texture *mixFactor;
};

} }

#endif	/* _LUXRAYS_SDL_MATERIAL_H */
