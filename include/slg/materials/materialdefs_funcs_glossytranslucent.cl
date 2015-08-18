#line 2 "materialdefs_funcs_glossy2.cl"

/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#if defined (PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT)

BSDFEvent GlossyTranslucentMaterial_GetEventTypes() {
	return GLOSSY | DIFFUSE | REFLECT | TRANSMIT;
}

bool GlossyTranslucentMaterial_IsDelta() {
	return false;
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 GlossyTranslucentMaterial_GetPassThroughTransparency(__global const Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	return BLACK;
}
#endif

float3 GlossyTranslucentMaterial_ConstEvaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
		const float i, const float i_bf,
#endif
		const float nuVal, const float nuVal_bf,
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
		const float nvVal, const float nvVal_bf,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
		const float3 kaVal, const float3 kaVal_bf,
		const float d, const float d_bf,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE)
		const int multibounceVal, const int multibounceVal_bf,
#endif
		const float3 kdVal, const float3 ktVal, const float3 ksVal, const float3 ksVal_bf) {
	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	const float cosi = fabs(sampledDir.z);
	const float coso = fabs(fixedDir.z);

	const float3 geometryN = VLOAD3F(&hitPoint->geometryN.x);
	Frame frame;
	Frame_SetFromZ_Private(&frame, geometryN);

	const float sideTest = dot(Frame_ToWorld_Private(&frame, fixedDir), geometryN) * dot(Frame_ToWorld_Private(&frame, sampledDir), geometryN);
	if (sideTest < -DEFAULT_COS_EPSILON_STATIC) {
		// Transmition
		*event = DIFFUSE | TRANSMIT;

		if (directPdfW)
			*directPdfW = fabs(sampledDir.z) * M_1_PI_F * 0.5f;

		const float3 H = normalize((float3)(lightDir.x + eyeDir.x, lightDir.y + eyeDir.y,
			lightDir.z - eyeDir.z));
		const float u = fabs(dot(lightDir, H));
		float3 ks = ksVal;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}
#endif
		ks = Spectrum_Clamp(ks);
		const float3 S1 = FresnelSchlick_Evaluate(ks, u);

		ks = ksVal_bf;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
		if (i_bf > 0.f) {
			const float ti = (i_bf - 1.f) / (i_bf + 1.f);
			ks *= ti * ti;
		}
#endif
		ks = Spectrum_Clamp(ks);
		const float3 S2 = FresnelSchlick_Evaluate(ks, u);
		float3 S = Spectrum_Sqrt((WHITE - S1) * (WHITE - S2));
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
		if (lightDir.z > 0.f) {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / cosi) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / coso));
		} else {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / coso) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / cosi));
		}
#endif
		return (M_1_PI_F * coso) * S * Spectrum_Clamp(ktVal) *
			(WHITE - Spectrum_Clamp(kdVal));
	} else if (sideTest > DEFAULT_COS_EPSILON_STATIC) {
		// Reflection
		*event = GLOSSY | REFLECT;

		const float3 baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * cosi;
		float3 ks;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
		float index;
#endif
		float u;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
		float v;
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
		float3 alpha;
		float depth;
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE)
		int mbounce;
#else
		int mbounce = 0;
#endif
		if (eyeDir.z >= 0.f) {
			ks = ksVal;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
			index = i;
#endif
			u = clamp(nuVal, 0.f, 1.f);
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
			v = clamp(nvVal, 0.f, 1.f);
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
			alpha = Spectrum_Clamp(kaVal);
			depth = d;
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE)
			mbounce = multibounceVal;
#endif
		} else {
			ks = ksVal_bf;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
			index = i_bf;
#endif
			u = clamp(nuVal_bf, 0.f, 1.f);
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
			v = clamp(nvVal_bf, 0.f, 1.f);
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
			alpha = Spectrum_Clamp(kaVal_bf);
			depth = d_bf;
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE)
			mbounce = multibounceVal_bf;
#endif
		}

#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
		if (index > 0.f) {
			const float ti = (index - 1.f) / (index + 1.f);
			ks *= ti * ti;
		}
#endif
		ks = Spectrum_Clamp(ks);

#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
		const float u2 = u * u;
		const float v2 = v * v;
		const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
		const float roughness = u * v;
#else
		const float anisotropy = 0.f;
		const float roughness = u * u;
#endif

		if (directPdfW) {
			const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
			const float wBase = 1.f - wCoating;

			*directPdfW = 0.5f * (wBase * fabs(sampledDir.z * M_1_PI_F) +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir));
		}

		// Absorption
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, depth);
#else
		const float3 absorption = WHITE;
#endif

		// Coating fresnel factor
		const float3 H = normalize(fixedDir + sampledDir);
		const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));

		const float3 coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, mbounce, fixedDir, sampledDir);

		// Blend in base layer Schlick style
		// assumes coating bxdf takes fresnel factor S into account

		return coatingF + absorption * (WHITE - S) * baseF;
	} else {
		return BLACK;
	}
}

float3 GlossyTranslucentMaterial_ConstSample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
		const float i, const float i_bf,
#endif
		const float nuVal, const float nuVal_bf,
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
		const float nvVal, const float nvVal_bf,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
		const float3 kaVal, const float3 kaVal_bf,
		const float d, const float d_bf,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE)
		const int multibounceVal, const int multibounceVal_bf,
#endif
		const float3 kdVal, const float3 ktVal, const float3 ksVal, const float3 ksVal_bf) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	if (passThroughEvent < 0.5f && (requestedEvent & (GLOSSY | REFLECT))) {
		// Reflection
		float3 ks;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
		float index;
#endif
		float u;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
		float v;
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
		float3 alpha;
		float depth;
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE)
		int mbounce;
#else
		int mbounce = 0;
#endif
		if (fixedDir.z >= 0.f) {
			ks = ksVal;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
			index = i;
#endif
			u = clamp(nuVal, 0.f, 1.f);
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
			v = clamp(nvVal, 0.f, 1.f);
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
			alpha = Spectrum_Clamp(kaVal);
			depth = d;
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE)
			mbounce = multibounceVal;
#endif
		} else {
			ks = ksVal_bf;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
			index = i_bf;
#endif
			u = clamp(nuVal_bf, 0.f, 1.f);
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
			v = clamp(nvVal_bf, 0.f, 1.f);
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
			alpha = Spectrum_Clamp(kaVal_bf);
			depth = d_bf;
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE)
			mbounce = multibounceVal_bf;
#endif
		}

#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
		if (index > 0.f) {
			const float ti = (index - 1.f) / (index + 1.f);
			ks *= ti * ti;
		}
#endif
		ks = Spectrum_Clamp(ks);

#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)
		const float u2 = u * u;
		const float v2 = v * v;
		const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
		const float roughness = u * v;
#else
		const float anisotropy = 0.f;
		const float roughness = u * u;
#endif

		const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
		const float wBase = 1.f - wCoating;

		float basePdf, coatingPdf;
		float3 baseF, coatingF;

		if (2.f * passThroughEvent < wBase) {
			// Sample base BSDF (Matte BSDF)
			*sampledDir = CosineSampleHemisphereWithPdf(u0, u1, &basePdf);
			*sampledDir *= signbit(fixedDir.z) ? -1.f : 1.f;

			*cosSampledDir = fabs((*sampledDir).z);
			if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
				return BLACK;

			baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * *cosSampledDir;

			// Evaluate coating BSDF (Schlick BSDF)
			coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, mbounce,
				fixedDir, *sampledDir);
			coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, *sampledDir);

			*event = GLOSSY | REFLECT;
		} else {
			// Sample coating BSDF (Schlick BSDF)
			coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy, mbounce,
				fixedDir, sampledDir, u0, u1, &coatingPdf);
			if (Spectrum_IsBlack(coatingF))
				return BLACK;

			*cosSampledDir = fabs((*sampledDir).z);
			if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
				return BLACK;

			coatingF *= coatingPdf;

			// Evaluate base BSDF (Matte BSDF)
			basePdf = *cosSampledDir * M_1_PI_F;
			baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * *cosSampledDir;

			*event = GLOSSY | REFLECT;
		}

		const float3 geometryN = VLOAD3F(&hitPoint->geometryN.x);
		Frame frame;
		Frame_SetFromZ_Private(&frame, geometryN);
		if (!(dot(Frame_ToWorld_Private(&frame, fixedDir), geometryN) * dot(Frame_ToWorld_Private(&frame, *sampledDir), geometryN) > DEFAULT_COS_EPSILON_STATIC))
			return BLACK;

		*pdfW = 0.5f * (coatingPdf * wCoating + basePdf * wBase);

		// Absorption
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
		const float cosi = fabs((*sampledDir).z);
		const float coso = fabs(fixedDir.z);
		const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);
#else
		const float3 absorption = WHITE;
#endif

		// Coating fresnel factor
		const float3 H = normalize(fixedDir + *sampledDir);
		const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(*sampledDir, H)));

		// Blend in base layer Schlick style
		// coatingF already takes fresnel factor S into account

		return (coatingF + absorption * (WHITE - S) * baseF) / *pdfW;
	} else if (passThroughEvent >= .5f && (requestedEvent & (DIFFUSE | TRANSMIT))) {
		// Transmition
		*sampledDir = CosineSampleHemisphereWithPdf(u0, u1, pdfW);
		*sampledDir *= signbit(fixedDir.z) ? 1.f : -1.f;

		const float3 geometryN = VLOAD3F(&hitPoint->geometryN.x);
		Frame frame;
		Frame_SetFromZ_Private(&frame, geometryN);
		if (!(dot(Frame_ToWorld_Private(&frame, fixedDir), geometryN) * dot(Frame_ToWorld_Private(&frame, *sampledDir), geometryN) < -DEFAULT_COS_EPSILON_STATIC))
			return BLACK;

		*pdfW *= 0.5f;

		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		*event = DIFFUSE | TRANSMIT;

		const float cosi = fabs((*sampledDir).z);
		const float coso = fabs(fixedDir.z);

		const float3 H = normalize((float3)((*sampledDir).x + fixedDir.x, (*sampledDir).y + fixedDir.y,
			(*sampledDir).z - fixedDir.z));
		const float u = fabs(dot(*sampledDir, H));
		float3 ks = ksVal;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}
#endif
		ks = Spectrum_Clamp(ks);
		const float3 S1 = FresnelSchlick_Evaluate(ks, u);

		ks = ksVal_bf;
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)
		if (i_bf > 0.f) {
			const float ti = (i_bf - 1.f) / (i_bf + 1.f);
			ks *= ti * ti;
		}
#endif
		ks = Spectrum_Clamp(ks);
		const float3 S2 = FresnelSchlick_Evaluate(ks, u);
		float3 S = Spectrum_Sqrt((WHITE - S1) * (WHITE - S2));
#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)
		if ((*sampledDir).z > 0.f) {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / cosi) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / coso));
		} else {
			S *= Spectrum_Exp(Spectrum_Clamp(kaVal) * -(d / coso) +
				Spectrum_Clamp(kaVal_bf) * -(d_bf / cosi));
		}
#endif
		return (M_1_PI_F * coso / *pdfW) * S * Spectrum_Clamp(ktVal) *
			(WHITE - Spectrum_Clamp(kdVal));
	} else {
		return BLACK;
	}
}

#endif
