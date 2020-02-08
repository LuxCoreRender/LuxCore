#line 2 "materialdefs_funcs_matte.cl"

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
// Matte material
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void MatteMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	const float3 albedo = Spectrum_Clamp(Texture_GetSpectrumValue(material->matte.kdTexIndex,
			hitPoint TEXTURES_PARAM));

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void MatteMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void MatteMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void MatteMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void MatteMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_NOT_INLINE float3 MatteMaterial_Evaluate(
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 kdVal) {
	if (directPdfW)
		*directPdfW = fabs(lightDir.z * M_1_PI_F);

	*event = DIFFUSE | REFLECT;

	return Spectrum_Clamp(kdVal) * fabs(lightDir.z * M_1_PI_F);
}

OPENCL_FORCE_NOT_INLINE float3 MatteMaterial_Sample(__global const HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event,
		const float3 kdVal) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);
	if (fabs(CosTheta(*sampledDir)) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	return Spectrum_Clamp(kdVal);
}

//------------------------------------------------------------------------------
// Rough matte material
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void RoughMatteMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	const float3 albedo = Spectrum_Clamp(Texture_GetSpectrumValue(material->roughmatte.kdTexIndex,
			hitPoint TEXTURES_PARAM));

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void RoughMatteMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void RoughMatteMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void RoughMatteMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE void RoughMatteMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		TEXTURES_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset TEXTURES_PARAM);
}

OPENCL_FORCE_NOT_INLINE float3 RoughMatteMaterial_Evaluate(
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float s, const float3 kdVal) {
	if (directPdfW)
		*directPdfW = fabs(lightDir.z * M_1_PI_F);

	*event = DIFFUSE | REFLECT;

	const float sigma2 = s * s;
	const float A = 1.f - (sigma2 / (2.f * (sigma2 + 0.33f)));
	const float B = 0.45f * sigma2 / (sigma2 + 0.09f);
	const float sinthetai = SinTheta(eyeDir);
	const float sinthetao = SinTheta(lightDir);
	float maxcos = 0.f;
	if (sinthetai > 1e-4f && sinthetao > 1e-4f) {
			const float dcos = CosPhi(lightDir) * CosPhi(eyeDir) +
					SinPhi(lightDir) * SinPhi(eyeDir);
			maxcos = fmax(0.f, dcos);
	}
	return Spectrum_Clamp(kdVal) * fabs(lightDir.z * M_1_PI_F) *
		(A + B * maxcos * sinthetai * sinthetao / fmax(fabs(CosTheta(lightDir)), fabs(CosTheta(eyeDir))));
}

OPENCL_FORCE_NOT_INLINE float3 RoughMatteMaterial_Sample(
		__global const HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event,
		const float s, const float3 kdVal) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	if (fabs((*sampledDir).z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	const float sigma2 = s * s;
	const float A = 1.f - (sigma2 / (2.f * (sigma2 + 0.33f)));
	const float B = 0.45f * sigma2 / (sigma2 + 0.09f);
	const float sinthetai = SinTheta(fixedDir);
	const float sinthetao = SinTheta(*sampledDir);
	float maxcos = 0.f;
	if (sinthetai > 1e-4f && sinthetao > 1e-4f) {
			const float dcos = CosPhi(*sampledDir) * CosPhi(fixedDir) +
					SinPhi(*sampledDir) * SinPhi(fixedDir);
			maxcos = fmax(0.f, dcos);
	}

	return Spectrum_Clamp(kdVal) *
		(A + B * maxcos * sinthetai * sinthetao / fmax(fabs(CosTheta(*sampledDir)), fabs(CosTheta(fixedDir))));
}
