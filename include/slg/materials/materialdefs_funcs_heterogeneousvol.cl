#line 2 "materialdefs_funcs_heterogenousvol.cl"

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

OPENCL_FORCE_INLINE float3 SchlickScatter_GetColor(const float3 sigmaS, const float3 sigmaA) {
	float3 r = sigmaS;
	if (r.x > 0.f)
		r.x /= r.x + sigmaA.x;
	else
		r.x = 1.f;
	if (r.y > 0.f)
		r.y /= r.y + sigmaA.y;
	else
		r.y = 1.f;
	if (r.z > 0.f)
		r.z /= r.z + sigmaA.z;
	else
		r.z = 1.f;

	return r;
}

OPENCL_FORCE_INLINE float3 SchlickScatter_Albedo(const float3 sigmaS, const float3 sigmaA) {
	return Spectrum_Clamp(SchlickScatter_GetColor(sigmaS, sigmaA));
}

OPENCL_FORCE_INLINE float3 SchlickScatter_Evaluate(
		__global const HitPoint *hitPoint, const float3 localEyeDir, const float3 localLightDir,
		BSDFEvent *event, float *directPdfW,
		const float3 sigmaS, const float3 sigmaA, const float3 g) {
	const float3 gValue = clamp(g, -1.f, 1.f);
	const float3 k = gValue * (1.55f - .55f * gValue * gValue);

	*event = DIFFUSE | REFLECT;

	const float dotEyeLight = dot(localEyeDir, localLightDir);
	const float kFilter = Spectrum_Filter(k);
	// 1+k*cos instead of 1-k*cos because localEyeDir is reversed compared to the
	// standard phase function definition
	const float compcostFilter = 1.f + kFilter * dotEyeLight;
	const float pdf = (1.f - kFilter * kFilter) / (compcostFilter * compcostFilter * (4.f * M_PI_F));

	if (directPdfW)
		*directPdfW = pdf;

	// 1+k*cos instead of 1-k*cos because localEyeDir is reversed compared to the
	// standard phase function definition
	const float3 compcostValue = 1.f + k * dotEyeLight;

	return SchlickScatter_GetColor(sigmaS, sigmaA) * (1.f - k * k) / (compcostValue * compcostValue * (4.f * M_PI_F));
}

OPENCL_FORCE_INLINE float3 SchlickScatter_Sample(
		__global const HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event,
		const float3 sigmaS, const float3 sigmaA, const float3 g) {
	const float3 gValue = clamp(g, -1.f, 1.f);
	const float3 k = gValue * (1.55f - .55f * gValue * gValue);
	const float kFilter = Spectrum_Filter(k);

	// Add a - because localEyeDir is reversed compared to the standard phase
	// function definition
	const float cost = -(2.f * u0 + kFilter - 1.f) / (2.f * kFilter * u0 - kFilter + 1.f);

	float3 x, y;
	CoordinateSystem(fixedDir, &x, &y);
	*sampledDir = SphericalDirectionWithFrame(sqrt(fmax(0.f, 1.f - cost * cost)), cost,
			2.f * M_PI_F * u1, x, y, fixedDir);

	// The - becomes a + because cost has been reversed above
	const float compcost = 1.f + kFilter * cost;
	*pdfW = (1.f - kFilter * kFilter) / (compcost * compcost * (4.f * M_PI_F));
	if (*pdfW <= 0.f)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	return SchlickScatter_GetColor(sigmaS, sigmaA);
}

//------------------------------------------------------------------------------
// HeterogeneousVol material
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void HeterogeneousVolMaterial_Albedo(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	const float3 sigmaS = Texture_GetSpectrumValue(material->volume.heterogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM);
	const float3 sigmaA = Texture_GetSpectrumValue(material->volume.heterogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM);

    const float3 albedo = SchlickScatter_Albedo(
			clamp(sigmaS, 0.f, INFINITY),
			clamp(sigmaA, 0.f, INFINITY));

	EvalStack_PushFloat3(albedo);
}

OPENCL_FORCE_INLINE void HeterogeneousVolMaterial_GetInteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void HeterogeneousVolMaterial_GetExteriorVolume(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void HeterogeneousVolMaterial_GetPassThroughTransparency(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void HeterogeneousVolMaterial_GetEmittedRadiance(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	DefaultMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
}

OPENCL_FORCE_INLINE void HeterogeneousVolMaterial_Evaluate(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float3 lightDir, eyeDir;
	EvalStack_PopFloat3(eyeDir);
	EvalStack_PopFloat3(lightDir);

	const float3 sigmaSTexVal = Texture_GetSpectrumValue(material->volume.homogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM);
	const float3 sigmaATexVal = Texture_GetSpectrumValue(material->volume.homogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM);
	const float3 gTexVal = Texture_GetSpectrumValue(material->volume.homogenous.gTexIndex, hitPoint TEXTURES_PARAM);

	BSDFEvent event;
	float directPdfW;
	const float3 result = SchlickScatter_Evaluate(
			hitPoint, eyeDir, lightDir,
			&event, &directPdfW,
			clamp(sigmaSTexVal, 0.f, INFINITY), clamp(sigmaATexVal, 0.f, INFINITY), gTexVal);

	EvalStack_PushFloat3(result);
	EvalStack_PushBSDFEvent(event);
	EvalStack_PushFloat(directPdfW);
}

OPENCL_FORCE_INLINE void HeterogeneousVolMaterial_Sample(__global const Material* restrict material,
		__global const HitPoint *hitPoint,
		__global float *evalStack, uint *evalStackOffset
		MATERIALS_PARAM_DECL) {
	float u0, u1, passThroughEvent;
	EvalStack_PopFloat(passThroughEvent);
	EvalStack_PopFloat(u1);
	EvalStack_PopFloat(u0);
	float3 fixedDir;
	EvalStack_PopFloat3(fixedDir);

	const float3 sigmaSTexVal = Texture_GetSpectrumValue(material->volume.homogenous.sigmaSTexIndex, hitPoint TEXTURES_PARAM);
	const float3 sigmaATexVal = Texture_GetSpectrumValue(material->volume.homogenous.sigmaATexIndex, hitPoint TEXTURES_PARAM);
	const float3 gTexVal = Texture_GetSpectrumValue(material->volume.homogenous.gTexIndex, hitPoint TEXTURES_PARAM);

	float3 sampledDir;
	float pdfW;
	BSDFEvent event;
	const float3 result = SchlickScatter_Sample(
			hitPoint, fixedDir, &sampledDir,
			u0, u1, 
			passThroughEvent,
			&pdfW, &event,
			clamp(sigmaSTexVal, 0.f, INFINITY), clamp(sigmaATexVal, 0.f, INFINITY), gTexVal);

	EvalStack_PushFloat3(result);
	EvalStack_PushFloat3(sampledDir);
	EvalStack_PushFloat(pdfW);
	EvalStack_PushBSDFEvent(event);
}

//------------------------------------------------------------------------------
// Material specific EvalOp
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE void HeterogeneousVolMaterial_EvalOp(
		__global const Material* restrict material,
		const MaterialEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	switch (evalType) {
		case EVAL_ALBEDO:
			HeterogeneousVolMaterial_Albedo(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_INTERIOR_VOLUME:
			HeterogeneousVolMaterial_GetInteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EXTERIOR_VOLUME:
			HeterogeneousVolMaterial_GetExteriorVolume(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_EMITTED_RADIANCE:
			HeterogeneousVolMaterial_GetEmittedRadiance(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_GET_PASS_TROUGH_TRANSPARENCY:
			HeterogeneousVolMaterial_GetPassThroughTransparency(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_EVALUATE:
			HeterogeneousVolMaterial_Evaluate(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		case EVAL_SAMPLE:
			HeterogeneousVolMaterial_Sample(material, hitPoint, evalStack, evalStackOffset MATERIALS_PARAM);
			break;
		default:
			// Something wrong here
			break;
	}
}
