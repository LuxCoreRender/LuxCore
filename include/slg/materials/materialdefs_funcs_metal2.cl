#line 2 "materialdefs_funcs_metal2.cl"

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

//------------------------------------------------------------------------------
// Metal2 material
//
// LuxRender Metal2 material porting.
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_METAL2)

OPENCL_FORCE_INLINE void Metal2Material_GetNK(__global const Material* restrict material, __global const HitPoint *hitPoint,
		float3 *n, float3 *k
		TEXTURES_PARAM_DECL) {
	const uint fresnelTexIndex = material->metal2.fresnelTexIndex;
	if (fresnelTexIndex != NULL_INDEX) {
		__global const Texture* restrict fresnelTex = &texs[fresnelTexIndex];

		if (fresnelTex->type == FRESNELCOLOR_TEX) {
			const float3 f = Texture_GetSpectrumValue(fresnelTex->fresnelColor.krIndex, hitPoint TEXTURES_PARAM);
			*n = FresnelApproxN3(f);
			*k = FresnelApproxK3(f);
		} else {
			*n = VLOAD3F(&fresnelTex->fresnelConst.n.c[0]);
			*k = VLOAD3F(&fresnelTex->fresnelConst.k.c[0]);
		}
	} else {
		*n = clamp(Texture_GetSpectrumValue(material->metal2.nTexIndex, hitPoint TEXTURES_PARAM), .001f, INFINITY);
		*k = clamp(Texture_GetSpectrumValue(material->metal2.kTexIndex, hitPoint TEXTURES_PARAM), .001f, INFINITY);
	}
}

OPENCL_FORCE_INLINE BSDFEvent Metal2Material_GetEventTypes() {
	return GLOSSY | REFLECT;
}

OPENCL_FORCE_INLINE float3 Metal2Material_Albedo(const float3 nVal, const float3 kVal) {
	const float3 F = FresnelGeneral_Evaluate(nVal, kVal, 1.f);
	Spectrum_Clamp(F);

	return F;
}

OPENCL_FORCE_NOT_INLINE float3 Metal2Material_Evaluate(
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float uVal,
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
		const float vVal,
#endif
		const float3 nVal, const float3 kVal) {
	const float u = clamp(uVal, 1e-9f, 1.f);
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
	const float v = clamp(vVal, 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
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
	Spectrum_Clamp(F);

	const float G = SchlickDistribution_G(roughness, lightDir, eyeDir);

	*event = GLOSSY | REFLECT;
	return (SchlickDistribution_D(roughness, wh, anisotropy) * G / (4.f * fabs(eyeDir.z))) * F;
}

OPENCL_FORCE_NOT_INLINE float3 Metal2Material_Sample(
		__global const HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event,
		const float uVal,
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
		const float vVal,
#endif
		const float3 nVal, const float3 kVal) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	const float u = clamp(uVal, 1e-9f, 1.f);
#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)
	const float v = clamp(vVal, 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
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
	if ((cosi < DEFAULT_COS_EPSILON_STATIC) || (fixedDir.z * (*sampledDir).z < 0.f))
		return BLACK;

	*pdfW = specPdf / (4.f * fabs(cosWH));
	if (*pdfW <= 0.f)
		return BLACK;

	const float G = SchlickDistribution_G(roughness, fixedDir, *sampledDir);
	
	const float3 F = FresnelGeneral_Evaluate(nVal, kVal, cosWH);
	Spectrum_Clamp(F);

	float factor = (d / specPdf) * G * fabs(cosWH);
	//if (!fromLight)
		factor /= coso;
	//else
	//	factor /= cosi;

	*event = GLOSSY | REFLECT;

	return factor * F;
}

#endif
