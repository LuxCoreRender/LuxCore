#line 2 "materialdefs_funcs_metal2.cl"

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
// Metal2 material
//
// LuxRender Metal2 material porting.
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_METAL2)

BSDFEvent Metal2Material_GetEventTypes() {
	return GLOSSY | REFLECT;
}

bool Metal2Material_IsDelta() {
	return false;
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 Metal2Material_GetPassThroughTransparency(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	return BLACK;
}
#endif

float3 Metal2Material_ConstEvaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float uVal,
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
		const float vVal,
#endif
		const float3 nVal, const float3 kVal) {
	const float u = clamp(uVal, 6e-3f, 1.f);
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
	const float v = clamp(vVal, 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	const float3 wh = normalize(lightDir + eyeDir);
	const float cosWH = dot(lightDir, wh);

	if (directPdfW)
		*directPdfW = SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * cosWH);

	const float3 F = FresnelGeneral_Evaluate(nVal, kVal, cosWH);

	const float G = SchlickDistribution_G(roughness, lightDir, eyeDir);

	*event = GLOSSY | REFLECT;
	return SchlickDistribution_D(roughness, wh, anisotropy) * G / (4.f * fabs(eyeDir.z)) * F;
}

float3 Metal2Material_ConstSample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const float uVal,
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
		const float vVal,
#endif
		const float3 nVal, const float3 kVal) {
	if (!(requestedEvent & (GLOSSY | REFLECT)) ||
			(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	const float u = clamp(uVal, 6e-3f, 1.f);
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
	const float v = clamp(vVal, 6e-3f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : (v2 / u2 - 1.f);
	const float roughness = u * v;
#else
	const float anisotropy = 0.f;
	const float roughness = u * u;
#endif

	float3 wh;
	float d, specPdf;
	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);
	const float cosWH = dot(fixedDir, wh);
	*sampledDir = 2.f * cosWH * wh - fixedDir;

	const float coso = fabs(fixedDir.z);
	const float cosi = fabs((*sampledDir).z);
	*cosSampledDir = cosi;
	if ((*cosSampledDir < DEFAULT_COS_EPSILON_STATIC) || (fixedDir.z * (*sampledDir).z < 0.f))
		return BLACK;

	*pdfW = specPdf / (4.f * fabs(cosWH));
	if (*pdfW <= 0.f)
		return BLACK;

	const float G = SchlickDistribution_G(roughness, fixedDir, *sampledDir);
	
	const float3 F = FresnelGeneral_Evaluate(nVal, kVal, cosWH);

	float factor = (d / specPdf) * G * fabs(cosWH);
	//if (!fromLight)
		factor /= coso;
	//else
	//	factor /= cosi;

	*event = GLOSSY | REFLECT;

	return factor * F;
}

#if defined(PARAM_DISABLE_MAT_DYNAMIC_EVALUATION)
float3 Metal2Material_Evaluate(__global Material *material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	const float uVal = Texture_GetFloatValue(material->metal2.nuTexIndex, hitPoint
		TEXTURES_PARAM);
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
	const float vVal = Texture_GetFloatValue(material->metal2.nvTexIndex, hitPoint
		TEXTURES_PARAM);
#endif

	const float3 nVal = Texture_GetSpectrumValue(material->metal2.nTexIndex, hitPoint
			TEXTURES_PARAM);
	const float3 kVal = Texture_GetSpectrumValue(material->metal2.kTexIndex, hitPoint
			TEXTURES_PARAM);

	return Metal2Material_ConstEvaluate(hitPoint, lightDir, eyeDir,
			event, directPdfW,
			uVal,
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
			vVal,
#endif
			nVal, kVal);
}

float3 Metal2Material_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	const float uVal = Texture_GetFloatValue(material->metal2.nuTexIndex, hitPoint
		TEXTURES_PARAM);
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
	const float vVal = Texture_GetFloatValue(material->metal2.nvTexIndex, hitPoint
		TEXTURES_PARAM);
#endif

	const float3 nVal = Texture_GetSpectrumValue(material->metal2.nTexIndex, hitPoint
			TEXTURES_PARAM);
	const float3 kVal = Texture_GetSpectrumValue(material->metal2.kTexIndex, hitPoint
			TEXTURES_PARAM);

	return Metal2Material_ConstSample(hitPoint, fixedDir, sampledDir,
			u0, u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
			passThroughEvent,
#endif
			pdfW, cosSampledDir, event, requestedEvent,
			uVal,
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
			vVal,
#endif
			nVal, kVal);
}
#endif

#endif
