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

#ifndef _SLG_DISNEYMAT_H
#define	_SLG_DISNEYMAT_H

#include "slg/materials/material.h"


namespace slg {

//------------------------------------------------------------------------------
//							Disney BRDF
// Based on "Physically Based Shading at Disney" presentet SIGGRAPH 2012 
//------------------------------------------------------------------------------
class DisneyMaterial : public Material {
public:
	DisneyMaterial(
		const Texture *frontTransp,
		const Texture *backTransp,
		const Texture *emitted,
		const Texture *bump,
		const Texture *baseColor,
		const Texture *subsurface,
		const Texture *roughness,
		const Texture *metallic,
		const Texture *specular,
		const Texture *specularTint,
		const Texture *clearcoat,
		const Texture *clearcoatGloss,
		const Texture *anisotropic,
		const Texture *sheen,
		const Texture *sheenTint
	);

	virtual MaterialType GetType() const { return DISNEY; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | DIFFUSE | REFLECT; };

	virtual luxrays::Spectrum Albedo(
		const HitPoint &hitPoint
	) const;

	virtual luxrays::Spectrum Evaluate(
		const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, 
		const luxrays::Vector &localEyeDir, 
		BSDFEvent *event,
		float *directPdfW = NULL, 
		float *reversePdfW = NULL
	) const;

	virtual luxrays::Spectrum Sample(
		const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, 
		luxrays::Vector *localSampledDir,
		const float u0, 
		const float u1, 
		const float passThroughEvent,
		float *pdfW, 
		float *absCosSampledDir, 
		BSDFEvent *event
	) const;

	virtual void Pdf(
		const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, 
		const luxrays::Vector &localEyeDir,
		float *directPdfW, 
		float *reversePdfW
	) const;

	luxrays::Properties ToProperties(
		const ImageMapCache & imgMapCache, 
		const bool useRealFileName
	) const;

	void UpdateTextureReferences(
		const Texture * oldTex, 
		const Texture * newTex
	);

	void AddReferencedTextures(boost::unordered_set<const Texture*>& referencedTexs) const;

	const Texture *GetBaseColor() const { return BaseColor; };
	const Texture *GetSubsurface() const { return Subsurface; };
	const Texture *GetRoughness() const { return Roughness; };
	const Texture *GetMetallic() const { return Metallic; };
	const Texture *GetSpecular() const { return Specular; };
	const Texture *GetSpecularTint() const { return SpecularTint; };
	const Texture *GetClearcoat() const { return Clearcoat; };
	const Texture *GetClearcoatGloss() const { return ClearcoatGloss; };
	const Texture *GetAnisotropic() const { return Anisotropic; };
	const Texture *GetSheen() const { return Sheen; };
	const Texture *GetSheenTint() const { return SheenTint; };

private:

	const Texture *BaseColor;
	const Texture *Subsurface;
	const Texture *Roughness;
	const Texture *Metallic;
	const Texture *Specular;
	const Texture *SpecularTint;
	const Texture *Clearcoat;
	const Texture *ClearcoatGloss;
	const Texture *Anisotropic;
	const Texture *Sheen;
	const Texture *SheenTint;

	luxrays::Spectrum CalculateTint(luxrays::Spectrum &color) const;

	float GTR1(float NdotH, float a) const;
	float GTR2_Aniso(float NdotH, float HdotX, float HdotY, float ax, float ay) const;
	float SmithG_GGX_Aniso(float NdotV, float VdotX, float VdotY, float ax, float ay) const;
	float SmithG_GGX(float NdotV, float alphaG) const;
	float Schlick_Weight(float cosi) const;
	void Anisotropic_Params(const HitPoint &hitPoint, float &ax, float &ay) const;
	void ComputeRatio(const HitPoint &hitPoint, float &RatioGlossy, float &diffuseWeight, float &RatioClearcoat) const;

	luxrays::Spectrum DisneyDiffuse(const HitPoint &hitPoint, float NdotL, float NdotV, float LdotH) const;
	luxrays::Spectrum DisneySubsurface(const HitPoint &hitPoint, float NdotL, float NdotV, float LdotH) const;
	luxrays::Spectrum DisneyMetallic(const HitPoint &hitPoint, float NdotL, float NdotV, float NdotH, float LdotH, float VdotH, luxrays::Vector wi, luxrays::Vector wo, luxrays::Vector H) const;
	float DisneyClearCoat(const HitPoint &hitPoint, float NdotL, float NdotV, float NdotH, float LdotH) const;
	luxrays::Spectrum DisneySheen(const HitPoint &hitPoint, float LdotH) const;

	luxrays::Vector DisneyDiffuseSample(luxrays::Vector & wo, float u0, float u1) const;
	luxrays::Vector DisneyMetallicSample(const HitPoint &hitPoint, luxrays::Vector & wo, float u0, float u1) const;
	luxrays::Vector DisneyClearcoatSample(const HitPoint &hitPoint, luxrays::Vector & wo, float u0, float u1) const;

	float DisneyPdf(const HitPoint &hitPoint, const luxrays::Vector & localLightDir, const luxrays::Vector localEyeDir) const;
	float DiffusePdf(luxrays::Vector & wi, luxrays::Vector & wo) const;
	float MetallicPdf(const HitPoint &hitPoint, luxrays::Vector & wi, luxrays::Vector & wo) const;
	float ClearcoatPdf(const HitPoint &hitPoint, Vector & wi, Vector & wo) const;
};

}

#endif	/* _SLG_DISNEYMAT_H */