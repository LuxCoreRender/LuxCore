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
		const float subsurface,
		const float roughness,
		const float metallic,
		const float specular,
		const float specularTint,
		const float clearcoat,
		const float clearcoatGloss,
		const float anisotropic,
		const float sheen,
		const float sheenTint
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

protected:

	luxrays::Spectrum EvaluateSheen(
		const float dotHL, 
		const luxrays::Spectrum &baseColor
	) const;

	luxrays::Spectrum CalculateTint(luxrays::Spectrum &color) const;

	float GTR1(float NdotH, float a) const;
	float GTR2_Aniso(float NdotH, float HdotX, float HdotY, float ax, float ay) const;
	float SmithG_GGX_Aniso(float NdotV, float VdotX, float VdotY, float ax, float ay) const;
	float SmithG_GGX(float NdotV, float alphaG) const;
	float Schlick_Weight(float cosi) const;
	void Anisotropic_Params(float & ax, float & ay) const;

private:

	const Texture *BaseColor;
	const Texture *ExteriorIor;
	const Texture *InteriorIor;
	float Subsurface;
	float Roughness;
	float Metallic;
	float Specular;
	float SpecularTint;
	float Clearcoat;
	float ClearcoatGloss;
	float Anisotropic;
	float Sheen;
	float SheenTint;

	float RatioGlossy;
	float RatioTrans;
	float RatioDiffuse;
	float RatioClearcoat;

	luxrays::Spectrum DisneyDiffuse(luxrays::Spectrum &color, float NdotL, float NdotV, float LdotH) const;
	luxrays::Spectrum DisneySubsurface(luxrays::Spectrum &color, float NdotL, float NdotV, float LdotH) const;
	luxrays::Spectrum DisneyMetallic(luxrays::Spectrum & color, float NdotL, float NdotV, float NdotH, float LdotH, float VdotH, luxrays::Vector wi, luxrays::Vector wo, luxrays::Vector H) const;
	float DisneyClearCoat(luxrays::Spectrum &color, float NdotL, float NdotV, float NdotH, float LdotH) const;
	luxrays::Spectrum DisneySheen(luxrays::Spectrum &color, float LdotH) const;

	luxrays::Vector DisneyDiffuseSample(luxrays::Vector & wo, float u0, float u1) const;
	luxrays::Vector DisneyMetallicSample(luxrays::Vector & wo, float u0, float u1) const;
	luxrays::Vector DisneyClearcoatSample(luxrays::Vector & wo, float u0, float u1) const;

	float DisneyPdf(const luxrays::Vector & localLightDir, const luxrays::Vector localEyeDir) const;
	float LambertianReflectionPdf(luxrays::Vector & wi, luxrays::Vector & wo) const;
	float MetallicPdf(luxrays::Vector & wi, luxrays::Vector & wo) const;
	float ClearcoatPdf(Vector & wi, Vector & wo) const;
};

}

#endif	/* _SLG_DISNEYMAT_H */