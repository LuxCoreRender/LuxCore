#line 2 "materialdefs_funcs_homogenousvol.cl"

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
// HomogeneousVol material
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE BSDFEvent HomogeneousVolMaterial_GetEventTypes() {
	return DIFFUSE | REFLECT;
}

OPENCL_FORCE_INLINE float3 HomogeneousVolMaterial_Albedo(const float3 sigmaSTexVal,
		const float3 sigmaATexVal) {
	return SchlickScatter_Albedo(clamp(sigmaSTexVal, 0.f, INFINITY),
			clamp(sigmaATexVal, 0.f, INFINITY));
}

OPENCL_FORCE_NOT_INLINE float3 HomogeneousVolMaterial_Evaluate(
		__global const HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 sigmaSTexVal, const float3 sigmaATexVal, const float3 gTexVal) {
	return SchlickScatter_Evaluate(
			hitPoint, eyeDir, lightDir,
			event, directPdfW,
			clamp(sigmaSTexVal, 0.f, INFINITY), clamp(sigmaATexVal, 0.f, INFINITY), gTexVal);
}

OPENCL_FORCE_NOT_INLINE float3 HomogeneousVolMaterial_Sample(
		__global const HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		const float passThroughEvent,
		float *pdfW, BSDFEvent *event,
		const float3 sigmaSTexVal, const float3 sigmaATexVal, const float3 gTexVal) {
	return SchlickScatter_Sample(
			hitPoint, fixedDir, sampledDir,
			u0, u1, 
			passThroughEvent,
			pdfW, event,
			clamp(sigmaSTexVal, 0.f, INFINITY), clamp(sigmaATexVal, 0.f, INFINITY), gTexVal);
}
