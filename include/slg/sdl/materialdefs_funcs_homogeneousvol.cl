#line 2 "materialdefs_funcs_homogenousvol.cl"

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
// HomogeneousVol material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_HOMOGENEOUS_VOL)

BSDFEvent HomogeneousVolMaterial_GetEventTypes() {
	return DIFFUSE | REFLECT;
}

bool HomogeneousVolMaterial_IsDelta() {
	return false;
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 HomogeneousVolMaterial_GetPassThroughTransparency(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	return BLACK;
}
#endif

float3 HomogeneousVolMaterial_ConstEvaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 sigmaSTexVal, const float3 sigmaATexVal, const float3 gTexVal) {
	return SchlickScatter_ConstEvaluate(
			hitPoint, eyeDir, lightDir,
			event, directPdfW,
			clamp(sigmaSTexVal, 0.f, INFINITY), clamp(sigmaATexVal, 0.f, INFINITY), gTexVal);
}

float3 HomogeneousVolMaterial_ConstSample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const float3 sigmaSTexVal, const float3 sigmaATexVal, const float3 gTexVal) {
	return SchlickScatter_ConstSample(
			hitPoint, fixedDir, sampledDir,
			u0, u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
			passThroughEvent,
#endif
			pdfW, cosSampledDir, event,
			requestedEvent,
			clamp(sigmaSTexVal, 0.f, INFINITY), clamp(sigmaATexVal, 0.f, INFINITY), gTexVal);
}

#if defined(PARAM_DISABLE_MAT_DYNAMIC_EVALUATION)
float3 HomogeneousVolMaterial_Evaluate(__global Material *material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	const float3 sigmaSTexVal = Texture_GetSpectrumValue(material->volume.homogenous.sigmaSTexIndex, hitPoint
			TEXTURES_PARAM);
	const float3 sigmaATexVal = Texture_GetSpectrumValue(material->volume.homogenous.sigmaATexIndex, hitPoint
			TEXTURES_PARAM);
	const float3 gTexVal = Texture_GetSpectrumValue(material->volume.homogenous.gTexIndex, hitPoint
			TEXTURES_PARAM);
	
	return HomogeneousVolMaterial_ConstEvaluate(hitPoint, lightDir, eyeDir,
			event, directPdfW,
			clamp(sigmaSTexVal, 0.f, INFINITY), clamp(sigmaATexVal, 0.f, INFINITY), gTexVal);
}

float3 HomogeneousVolMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	const float3 sigmaSTexVal = Texture_GetSpectrumValue(material->volume.homogenous.sigmaSTexIndex, hitPoint
			TEXTURES_PARAM);
	const float3 sigmaATexVal = Texture_GetSpectrumValue(material->volume.homogenous.sigmaATexIndex, hitPoint
			TEXTURES_PARAM);
	const float3 gTexVal = Texture_GetSpectrumValue(material->volume.homogenous.gTexIndex, hitPoint
			TEXTURES_PARAM);

	return SchlickScatter_ConstSample(
			hitPoint, fixedDir, sampledDir,
			u0, u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
			passThroughEvent,
#endif
			pdfW, cosSampledDir, event,
			requestedEvent,
			clamp(sigmaSTexVal, 0.f, INFINITY), clamp(sigmaATexVal, 0.f, INFINITY), gTexVal);
}
#endif

#endif
