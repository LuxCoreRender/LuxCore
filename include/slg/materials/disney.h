/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
		TextureConstPtr frontTransp,
		TextureConstPtr backTransp,
		TextureConstPtr emitted,
		TextureConstPtr bump,
		TextureConstPtr baseColor,
		TextureConstPtr subsurface,
		TextureConstPtr roughness,
		TextureConstPtr metallic,
		TextureConstPtr specular,
		TextureConstPtr specularTint,
		TextureConstPtr clearcoat,
		TextureConstPtr clearcoatGloss,
		TextureConstPtr anisotropic,
		TextureConstPtr sheen,
		TextureConstPtr sheenTint,
		TextureConstPtr filmAmount, 
		TextureConstPtr filmThickness, 
		TextureConstPtr filmIor
	);

	virtual MaterialType GetType() const { return DISNEY; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

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
		BSDFEvent *event
	) const;

	virtual void Pdf(
		const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, 
		const luxrays::Vector &localEyeDir,
		float *directPdfW, 
		float *reversePdfW
	) const;

	luxrays::PropertiesUPtr ToProperties(
		const ImageMapCache &imgMapCache, 
		const bool useRealFileName
	) const;

	void UpdateTextureReferences(
		TextureConstRef  oldTex, 
		TextureRef  newTex
	);

	void AddReferencedTextures(std::unordered_set<const Texture *>& referencedTexs) const;

	TextureConstPtr GetBaseColor() const { return BaseColor; };
	TextureConstPtr GetSubsurface() const { return Subsurface; };
	TextureConstPtr GetRoughness() const { return Roughness; };
	TextureConstPtr GetMetallic() const { return Metallic; };
	TextureConstPtr GetSpecular() const { return Specular; };
	TextureConstPtr GetSpecularTint() const { return SpecularTint; };
	TextureConstPtr GetClearcoat() const { return Clearcoat; };
	TextureConstPtr GetClearcoatGloss() const { return ClearcoatGloss; };
	TextureConstPtr GetAnisotropic() const { return Anisotropic; };
	TextureConstPtr GetSheen() const { return Sheen; };
	TextureConstPtr GetSheenTint() const { return SheenTint; };
	TextureConstPtr GetFilmAmount() const { return filmAmount; }
	TextureConstPtr GetFilmThickness() const { return filmThickness; }
	TextureConstPtr GetFilmIOR() const { return filmIor; }

private:
	TextureConstPtr BaseColor;
	TextureConstPtr Subsurface;
	TextureConstPtr Roughness;
	TextureConstPtr Metallic;
	TextureConstPtr Specular;
	TextureConstPtr SpecularTint;
	TextureConstPtr Clearcoat;
	TextureConstPtr ClearcoatGloss;
	TextureConstPtr Anisotropic;
	TextureConstPtr Sheen;
	TextureConstPtr SheenTint;
	TextureConstPtr filmAmount;
	TextureConstPtr filmThickness;
	TextureConstPtr filmIor;

	void UpdateGlossiness();

	luxrays::Spectrum CalculateTint(const luxrays::Spectrum &color) const;

	float GTR1(const float NdotH, const float a) const;
	float GTR2_Aniso(const float NdotH, const float HdotX, const float HdotY,
			const float ax, const float ay) const;
	float SmithG_GGX_Aniso(const float NdotV, const float VdotX, const float VdotY,
			const float ax, const float ay) const;
	float SmithG_GGX(const float NdotV, const float alphaG) const;
	float Schlick_Weight(const float cosi) const;
	void Anisotropic_Params(const float anisotropic, const float roughness, float &ax, float &ay) const;
	void ComputeRatio(const float metallic, const float clearcoat,
			float &RatioGlossy, float &diffuseWeight, float &RatioClearcoat) const;

	luxrays::Spectrum DisneyDiffuse(const luxrays::Spectrum &color, const float roughness,
			const float NdotL, const float NdotV, const float LdotH) const;
	luxrays::Spectrum DisneySubsurface(const luxrays::Spectrum &color, const float roughness,
			const float NdotL, const float NdotV, const float LdotH) const;
	luxrays::Spectrum DisneyMetallic(const luxrays::Spectrum &color, const float specular,
			const float specularTint, const float metallic,
			const float anisotropic, const float roughness,
			const float NdotL, const float NdotV, const float NdotH,
			const float LdotH, const float VdotH,
			const luxrays::Vector &wi, const luxrays::Vector &wo, const luxrays::Vector &H) const;
	float DisneyClearCoat(const float clearcoat, const float clearcoatGloss,
			const float NdotL, const float NdotV, const float NdotH, const float LdotH) const;
	luxrays::Spectrum DisneySheen(const luxrays::Spectrum &color, const float sheen,
			const float sheenTint, const float LdotH) const;
	
	luxrays::Spectrum DisneyEvaluate(const bool fromLight, 
		const luxrays::Spectrum &color,
		const float subsurface, const float roughness,
		const float metallic, const float specular, const float specularTint,
		const float clearcoat, const float clearcoatGloss, const float anisotropicGloss,
		const float sheen, const float sheenTint, const float localFilmAmount, const float localFilmThickness,
		const float localFilmIor, const Vector &localLightDir, const Vector &localEyeDir, 
		BSDFEvent *event, float *directPdfW, float *reversePdfW) const;

	luxrays::Vector DisneyDiffuseSample(const luxrays::Vector &wo, float u0, float u1) const;
	luxrays::Vector DisneyMetallicSample(const float anisotropic, const float roughness,
			const luxrays::Vector &wo, float u0, float u1) const;
	luxrays::Vector DisneyClearcoatSample(const float clearcoatGloss,
			const luxrays::Vector &wo, float u0, float u1) const;

	void DisneyPdf(const bool fromLight, const float roughness, const float metallic,
			const float clearcoat, const float clearcoatGloss, const float anisotropic,
			const Vector &localLightDir, const Vector &localEyeDir,
			float *directPdfW, float *reversePdfW) const;
	void DiffusePdf(const bool fromLight,
			const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
			float *directPdfW, float *reversePdfW) const;
	void MetallicPdf(const bool fromLight, const float anisotropic, const float roughness,
			const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
			float *directPdfW, float *reversePdfW) const;
	void ClearcoatPdf(const bool fromLight, const float clearcoatGloss,
			const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
			float *directPdfW, float *reversePdfW) const;
};

}

#endif	/* _SLG_DISNEYMAT_H */// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
