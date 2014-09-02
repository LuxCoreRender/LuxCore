#line 2 "materialdefs_funcs_glossy2.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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
// Glossy2 material
//
// LuxRender Glossy2 material porting.
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_GLOSSY2)

BSDFEvent Glossy2Material_GetEventTypes() {
	return GLOSSY | REFLECT;
}

bool Glossy2Material_IsDelta() {
	return false;
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 Glossy2Material_GetPassThroughTransparency(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	return BLACK;
}
#endif

float SchlickBSDF_CoatingWeight(const float3 ks, const float3 fixedDir) {
	// No sampling on the back face
	if (fixedDir.z <= 0.f)
		return 0.f;

	// Approximate H by using reflection direction for wi
	const float u = fabs(fixedDir.z);
	const float3 S = FresnelSchlick_Evaluate(ks, u);

	// Ensures coating is never sampled less than half the time
	// unless we are on the back face
	return .5f * (1.f + Spectrum_Filter(S));
}

float3 SchlickBSDF_CoatingF(const float3 ks, const float roughness,
		const float anisotropy, const int multibounce, const float3 fixedDir,
		const float3 sampledDir) {
	// No sampling on the back face
	if (fixedDir.z <= 0.f)
		return BLACK;

	const float coso = fabs(fixedDir.z);
	const float cosi = fabs(sampledDir.z);

	const float3 wh = normalize(fixedDir + sampledDir);
	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, wh)));

	const float G = SchlickDistribution_G(roughness, fixedDir, sampledDir);

	// Multibounce - alternative with interreflection in the coating creases
	float factor = SchlickDistribution_D(roughness, wh, anisotropy) * G;
	//if (!fromLight)
		factor = factor / 4.f * coso +
				(multibounce ? cosi * clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) : 0.f);
	//else
	//	factor = factor / (4.f * cosi) + 
	//			(multibounce ? coso * Clamp((1.f - G) / (4.f * cosi * coso), 0.f, 1.f) : 0.f);

	return factor * S;
}

float3 SchlickBSDF_CoatingSampleF(const float3 ks,
		const float roughness, const float anisotropy, const int multibounce,
		const float3 fixedDir, float3 *sampledDir,
		float u0, float u1, float *pdf) {
	// No sampling on the back face
	if (fixedDir.z <= 0.f)
		return BLACK;

	float3 wh;
	float d, specPdf;
	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);
	const float cosWH = dot(fixedDir, wh);
	*sampledDir = 2.f * cosWH * wh - fixedDir;

	if (((*sampledDir).z < DEFAULT_COS_EPSILON_STATIC) || (fixedDir.z * (*sampledDir).z < 0.f))
		return BLACK;

	const float coso = fabs(fixedDir.z);
	const float cosi = fabs((*sampledDir).z);

	*pdf = specPdf / (4.f * cosWH);
	if (*pdf <= 0.f)
		return BLACK;

	float3 S = FresnelSchlick_Evaluate(ks, cosWH);

	const float G = SchlickDistribution_G(roughness, fixedDir, *sampledDir);

	//CoatingF(sw, *wi, wo, f_);
	S *= (d / *pdf) * G / (4.f * coso) + 
			(multibounce ? cosi * clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) : 0.f);

	return S;
}

float SchlickBSDF_CoatingPdf(const float roughness, const float anisotropy,
		const float3 fixedDir, const float3 sampledDir) {
	// No sampling on the back face
	if (fixedDir.z <= 0.f)
		return 0.f;

	const float3 wh = normalize(fixedDir + sampledDir);
	return SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * fabs(dot(fixedDir, wh)));
}

float3 Glossy2Material_ConstEvaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
		const float i,
#endif
		const float nuVal,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
		const float nvVal,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
		const float3 kaVal,
		const float d,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
		const int multibounceVal,
#endif
		const float3 kdVal, const float3 ksVal) {
	const float3 fixedDir = eyeDir;
	const float3 sampledDir = lightDir;

	const float3 baseF = Spectrum_Clamp(kdVal) * M_1_PI_F * fabs(lightDir.z);
	if (eyeDir.z <= 0.f) {
		// Back face: no coating

		if (directPdfW)
			*directPdfW = fabs(sampledDir.z * M_1_PI_F);

		*event = DIFFUSE | REFLECT;
		return baseF;
	}

	// Front face: coating+base
	*event = GLOSSY | REFLECT;

	float3 ks = ksVal;
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
#endif
	ks = Spectrum_Clamp(ks);

	const float u = clamp(nuVal, 6e-3f, 1.f);
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
	const float v = clamp(nvVal, 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	if (directPdfW) {
		const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
		const float wBase = 1.f - wCoating;

		*directPdfW = wBase * fabs(sampledDir.z * M_1_PI_F) +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, sampledDir);
	}

	// Absorption
	const float cosi = fabs(sampledDir.z);
	const float coso = fabs(fixedDir.z);

#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
	const float3 alpha = Spectrum_Clamp(kaVal);
	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);
#else
	const float3 absorption = WHITE;
#endif

	// Coating fresnel factor
	const float3 H = normalize(fixedDir + sampledDir);
	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(sampledDir, H)));

#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
	const int multibounce = multibounceVal;
#else
	const int multibounce = 0;
#endif
	const float3 coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, multibounce,
			fixedDir, sampledDir);

	// Blend in base layer Schlick style
	// assumes coating bxdf takes fresnel factor S into account

	return coatingF + absorption * (WHITE - S) * baseF;
}

float3 Glossy2Material_ConstSample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
		const float i,
#endif
		const float nuVal,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
		const float nvVal,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
		const float3 kaVal,
		const float d,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
		const int multibounceVal,
#endif
		const float3 kdVal, const float3 ksVal) {
	if ((!(requestedEvent & (GLOSSY | REFLECT)) && fixedDir.z > 0.f) ||
		(!(requestedEvent & (DIFFUSE | REFLECT)) && fixedDir.z <= 0.f) ||
		(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	if (fixedDir.z <= 0.f) {
		// Back Face
		*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);
		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;
		*event = DIFFUSE | REFLECT;
		return Spectrum_Clamp(kdVal);
	}

	float3 ks = ksVal;
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
#endif
	ks = Spectrum_Clamp(ks);

	const float u = clamp(nuVal, 6e-3f, 1.f);
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
	const float v = clamp(nvVal, 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	// Coating is used only on the front face
	const float wCoating = SchlickBSDF_CoatingWeight(ks, fixedDir);
	const float wBase = 1.f - wCoating;

	const float3 baseF = Spectrum_Clamp(kdVal) * M_1_PI_F;

#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
	const int multibounce = multibounceVal;
#else
	const int multibounce = 0;
#endif

	float basePdf, coatingPdf;
	float3 coatingF;
	if (passThroughEvent < wBase) {
		// Sample base BSDF (Matte BSDF)
		*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, &basePdf);

		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		// Evaluate coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingF(ks, roughness, anisotropy, multibounce,
				fixedDir, *sampledDir);
		coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, fixedDir, *sampledDir);

		*event = GLOSSY | REFLECT;
	} else {
		// Sample coating BSDF (Schlick BSDF)
		coatingF = SchlickBSDF_CoatingSampleF(ks, roughness, anisotropy,
				multibounce, fixedDir, sampledDir, u0, u1, &coatingPdf);
		if (Spectrum_IsBlack(coatingF))
			return BLACK;

		*cosSampledDir = fabs((*sampledDir).z);
		if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return BLACK;

		coatingF *= coatingPdf;

		// Evaluate base BSDF (Matte BSDF)
		basePdf = *cosSampledDir * M_1_PI_F;

		*event = GLOSSY | REFLECT;
	}

	*pdfW = coatingPdf * wCoating + basePdf * wBase;

	// Absorption
	const float cosi = fabs((*sampledDir).z);
	const float coso = fabs(fixedDir.z);

#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
	const float3 alpha = Spectrum_Clamp(kaVal);
	const float3 absorption = CoatingAbsorption(cosi, coso, alpha, d);
#else
	const float3 absorption = WHITE;
#endif

	// Coating fresnel factor
	const float3 H = normalize(fixedDir + *sampledDir);
	const float3 S = FresnelSchlick_Evaluate(ks, fabs(dot(*sampledDir, H)));

	// Blend in base layer Schlick style
	// coatingF already takes fresnel factor S into account

	return (coatingF + absorption * (WHITE - S) * baseF * *cosSampledDir) / *pdfW;
}

#if defined(PARAM_DISABLE_MAT_DYNAMIC_EVALUATION)
float3 Glossy2Material_Evaluate(__global Material *material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
	const float i = Texture_GetFloatValue(material->glossy2.indexTexIndex, hitPoint
			TEXTURES_PARAM);
#endif

	const float nuVal = Texture_GetFloatValue(material->glossy2.nuTexIndex, hitPoint
		TEXTURES_PARAM);
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
	const float nvVal = Texture_GetFloatValue(material->glossy2.nvTexIndex, hitPoint
		TEXTURES_PARAM);
#endif

#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
	const float3 kaVal = Texture_GetSpectrumValue(material->glossy2.kaTexIndex, hitPoint
			TEXTURES_PARAM);
	const float d = Texture_GetFloatValue(material->glossy2.depthTexIndex, hitPoint
			TEXTURES_PARAM);
#endif

#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
	const int multibounce = material->glossy2.multibounce;
#endif

	const float3 kdVal = Texture_GetSpectrumValue(material->glossy2.kdTexIndex, hitPoint
			TEXTURES_PARAM);
	const float3 ksVal = Texture_GetSpectrumValue(material->glossy2.ksTexIndex, hitPoint
			TEXTURES_PARAM);

	return Glossy2Material_ConstEvaluate(
			hitPoint, lightDir, eyeDir,
			event, directPdfW,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
			i,
#endif
			nuVal,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
			nvVal,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
			kaVal,
			d,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
			multibounce,
#endif
			kdVal, ksVal);
}

float3 Glossy2Material_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
	const float i = Texture_GetFloatValue(material->glossy2.indexTexIndex, hitPoint
			TEXTURES_PARAM);
#endif

	const float nuVal = Texture_GetFloatValue(material->glossy2.nuTexIndex, hitPoint
		TEXTURES_PARAM);
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
	const float nvVal = Texture_GetFloatValue(material->glossy2.nvTexIndex, hitPoint
		TEXTURES_PARAM);
#endif

#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
	const float3 kaVal = Texture_GetSpectrumValue(material->glossy2.kaTexIndex, hitPoint
			TEXTURES_PARAM);
	const float d = Texture_GetFloatValue(material->glossy2.depthTexIndex, hitPoint
			TEXTURES_PARAM);
#endif

	const float3 kdVal = Texture_GetSpectrumValue(material->glossy2.kdTexIndex, hitPoint
			TEXTURES_PARAM);
	const float3 ksVal = Texture_GetSpectrumValue(material->glossy2.ksTexIndex, hitPoint
			TEXTURES_PARAM);

#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
	const int multibounce = material->glossy2.multibounce;
#endif

	return Glossy2Material_ConstSample(
			hitPoint, fixedDir, sampledDir,
			u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
			passThroughEvent,
#endif
			pdfW, cosSampledDir, event,
			requestedEvent,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)
			i,
#endif
			nuVal,
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)
			nvVal,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)
			kaVal,
			d,
#endif
#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)
			multibounce,
#endif
			kdVal, ksVal);
}
#endif

#endif
