#line 2 "materialdefs_funcs_glossytranslucent.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

//------------------------------------------------------------------------------
// GlossyTranslucent material
//
// LuxRender GlossyTranslucent material porting.
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
    const float3 albedo = Spectrum_Clamp(Texture_GetSpectrumValue(material->glossytranslucent.kdTexIndex, hitPoint TEXTURES_PARAM));

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void GlossyTranslucentMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_NOT_INLINE float3 GlossyTranslucentMaterial_Evaluate(
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float i, const float i_bf,
		const float nuVal, const float nuVal_bf,
		const float nvVal, const float nvVal_bf,
		const float3 kaVal, const float3 kaVal_bf,
		const float d, const float d_bf,
		const int multibounceVal, const int multibounceVal_bf,
		const float3 kdVal, const float3 ktVal, const float3 ksVal, const float3 ksVal_bf) {
	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	const float cosi = fabs(lightDir.z);
	const float coso = fabs(eyeDir.z);

	// Note: this is the same side test used by matte translucent material and
	// it is different from the CPU test because HitPoint::dpdu and HitPoint::dpdv
	// are not available here without bump mapping.
	const float sideTest = CosTheta(lightDir) * CosTheta(eyeDir);

	if (sideTest < -DEFAULT_COS_EPSILON_STATIC) {
		// Transmission
		*event = DIFFUSE | TRANSMIT;

		if (directPdfW)
			*directPdfW = fabs(sampledDir.z) * (M_1_PI_F * .5f);

		const float3 H = normalize((float3)(lightDir.x + eyeDir.x, lightDir.y + eyeDir.y,
			lightDir.z - eyeDir.z));
		const float u = fabs(dot(lightDir, H));
		float3 ks = ksVal;

		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}

		ks = Spectrum_Clamp(ks);
		const float3 S1 = FresnelSchlick_Evaluate(ks, u);

		ks = ksVal_bf;

		if (i_bf > 0.f) {
			const float ti = (i_bf - 1.f) / (i_bf + 1.f);
			ks *= ti * ti;
		}

		ks = Spectrum_Clamp(ks);
		const float3 S2 = FresnelSchlick_Evaluate(ks, u);
		float3 S = Spectrum_Sqrt((WHITE - S1) * (WHITE - S2));

		if (lightDir.z > 0.f) {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / cosi) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / coso));
		} else {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / coso) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / cosi));
		}

		return (M_1_PI_F * cosi) * S * Spectrum_Clamp(ktVal) *
			(WHITE - Spectrum_Clamp(kdVal));
	} else if (sideTest > DEFAULT_COS_EPSILON_STATIC) {
		// Reflection
		*event = GLOSSY | REFLECT;

		const float3 baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * cosi;
		float3 ks;
		float index;
		float u;
		float v;
		float3 alpha;
		float depth;
		int mbounce;

		if (eyeDir.z >= 0.f) {
			ks = ksVal;
			index = i;
			u = clamp(nuVal, 1e-9f, 1.f);
			v = clamp(nvVal, 1e-9f, 1.f);
			alpha = Spectrum_Clamp(kaVal);
			depth = d;
			mbounce = multibounceVal;
		} else {
			ks = ksVal_bf;
			index = i_bf;
			u = clamp(nuVal_bf, 1e-9f, 1.f);
			v = clamp(nvVal_bf, 1e-9f, 1.f);
			alpha = Spectrum_Clamp(kaVal_bf);
			depth = d_bf;
			mbounce = multibounceVal_bf;
		}

		if (index > 0.f) {
			const float ti = (index - 1.f) / (index + 1.f);
			ks *= ti * ti;
		}
		ks = Spectrum_Clamp(ks);

		const float u2 = u * u;
		const float v2 = v * v;
		const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
		const float roughness = u * v;

		if (directPdfW) {
			const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
			const float wBase = 1.f - wCoating;

			*directPdfW = .5f * (wBase * fabs(sampledDir.z * M_1_PI_F) +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir));
		}

		// Absorption
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, depth);

		// Coating fresnel factor
		const float3 H = normalize(fixedDir + sampledDir);
		const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));

		const float3 coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, mbounce, fixedDir, sampledDir);

		// Blend in base layer Schlick style
		// assumes coating bxdf takes fresnel factor S into account

		return coatingF + absorption * (WHITE - S) * baseF;
	} else
		return BLACK;
}

OPENCL_FORCE_NOT_INLINE float3 GlossyTranslucentMaterial_Sample(
		__global const HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event,
		const float i, const float i_bf,
		const float nuVal, const float nuVal_bf,
		const float nvVal, const float nvVal_bf,
		const float3 kaVal, const float3 kaVal_bf,
		const float d, const float d_bf,
		const int multibounceVal, const int multibounceVal_bf,
		const float3 kdVal, const float3 ktVal, const float3 ksVal, const float3 ksVal_bf) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	if (passThroughEvent < .5f) {
		// Reflection
		float3 ks;
		float index;
		float u;
		float v;
		float3 alpha;
		float depth;
		int mbounce;

		if (fixedDir.z >= 0.f) {
			ks = ksVal;
			index = i;
			u = clamp(nuVal, 1e-9f, 1.f);
			v = clamp(nvVal, 1e-9f, 1.f);
			alpha = Spectrum_Clamp(kaVal);
			depth = d;
			mbounce = multibounceVal;
		} else {
			ks = ksVal_bf;
			index = i_bf;
			u = clamp(nuVal_bf, 1e-9f, 1.f);
			v = clamp(nvVal_bf, 1e-9f, 1.f);
			alpha = Spectrum_Clamp(kaVal_bf);
			depth = d_bf;
			mbounce = multibounceVal_bf;
		}

		if (index > 0.f) {
			const float ti = (index - 1.f) / (index + 1.f);
			ks *= ti * ti;
		}
		ks = Spectrum_Clamp(ks);

		const float u2 = u * u;
		const float v2 = v * v;
		const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
		const float roughness = u * v;

		const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
		const float wBase = 1.f - wCoating;

		float basePdf, coatingPdf;
		float3 baseF, coatingF;

		if (2.f * passThroughEvent < wBase) {
			// Sample base BSDF (Matte BSDF)
			*sampledDir = CosineSampleHemisphereWithPdf(u0, u1, &basePdf);
			*sampledDir *= signbit(fixedDir.z) ? -1.f : 1.f;

			const float absCosSampledDir = fabs(CosTheta(*sampledDir));
			if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
				return BLACK;

			baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * absCosSampledDir;

			// Evaluate coating BSDF (Schlick BSDF)
			coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, mbounce,
				fixedDir, *sampledDir);
			coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, *sampledDir);

		} else {
			// Sample coating BSDF (Schlick BSDF)
			coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy, mbounce,
				fixedDir, sampledDir, u0, u1, &coatingPdf);
			if (Spectrum_IsBlack(coatingF))
				return BLACK;

			const float absCosSampledDir = fabs(CosTheta(*sampledDir));
			if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
				return BLACK;

			coatingF *= coatingPdf;

			// Evaluate base BSDF (Matte BSDF)
			basePdf = absCosSampledDir * M_1_PI_F;
			baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * absCosSampledDir;
		}
		*event = GLOSSY | REFLECT;

		// Note: this is the same side test used by matte translucent material and
		// it is different from the CPU test because HitPoint::dpdu and HitPoint::dpdv
		// are not available here without bump mapping.
		const float sideTest = CosTheta(fixedDir) * CosTheta(*sampledDir);
		if (!(sideTest > DEFAULT_COS_EPSILON_STATIC))
			return BLACK;

		*pdfW = .5f * (coatingPdf * wCoating + basePdf * wBase);

		// Absorption
		const float cosi = fabs((*sampledDir).z);
		const float coso = fabs(fixedDir.z);
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);

		// Coating fresnel factor
		const float3 H = normalize(fixedDir + *sampledDir);
		const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(*sampledDir, H)));

		// Blend in base layer Schlick style
		// coatingF already takes fresnel factor S into account

		return (coatingF + absorption * (WHITE - S) * baseF) / *pdfW;
	} else {
		// Transmission
		*sampledDir = CosineSampleHemisphereWithPdf(u0, u1, pdfW);
		*sampledDir *= signbit(fixedDir.z) ? 1.f : -1.f;

		// Note: this is the same side test used by matte translucent material and
		// it is different from the CPU test because HitPoint::dpdu and HitPoint::dpdv
		// are not available here without bump mapping.
		const float sideTest = CosTheta(fixedDir) * CosTheta(*sampledDir);
		if (!(sideTest < -DEFAULT_COS_EPSILON_STATIC))
			return BLACK;

		*pdfW *= .5f;

		const float absCosSampledDir = fabs(CosTheta(*sampledDir));
		if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		*event = DIFFUSE | TRANSMIT;

		// This is an inline expansion of Evaluate(hitPoint, *localSampledDir,
		// localFixedDir, event, pdfW, NULL) / *pdfW
		//
		// Note: pdfW and the pdf computed inside Evaluate() are exactly the same
		
		const float cosi = fabs((*sampledDir).z);
		const float coso = fabs(fixedDir.z);

		const float3 H = normalize((float3)((*sampledDir).x + fixedDir.x, (*sampledDir).y + fixedDir.y,
			(*sampledDir).z - fixedDir.z));
		const float u = fabs(dot(*sampledDir, H));
		float3 ks = ksVal;

		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}

		ks = Spectrum_Clamp(ks);
		const float3 S1 = FresnelSchlick_Evaluate(ks, u);

		ks = ksVal_bf;

		if (i_bf > 0.f) {
			const float ti = (i_bf - 1.f) / (i_bf + 1.f);
			ks *= ti * ti;
		}

		ks = Spectrum_Clamp(ks);
		const float3 S2 = FresnelSchlick_Evaluate(ks, u);
		float3 S = Spectrum_Sqrt((WHITE - S1) * (WHITE - S2));

		if ((*sampledDir).z > 0.f) {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / cosi) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / coso));
		} else {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / coso) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / cosi));
		}

		return (M_1_PI_F * cosi / *pdfW) * S * Spectrum_Clamp(ktVal) *
			(WHITE - Spectrum_Clamp(kdVal));
	}
}
