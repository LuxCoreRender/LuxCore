#line 2 "materialdefs_funcs_velvet.cl"

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
// Velvet material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_VELVET)

BSDFEvent VelvetMaterial_GetEventTypes() {
	return DIFFUSE | REFLECT;
}

bool VelvetMaterial_IsDelta() {
	return false;
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 VelvetMaterial_GetPassThroughTransparency(__global Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	return BLACK;
}
#endif

float3 VelvetMaterial_ConstEvaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 kdVal,
		const float A1, const float A2, const float A3,
		const float delta) {
	if (directPdfW)
		*directPdfW = fabs(lightDir.z * M_1_PI_F);

	*event = DIFFUSE | REFLECT;

	const float3 kd = Spectrum_Clamp(kdVal);
	const float cosv = -dot(lightDir, eyeDir);

	// Compute phase function

	const float B = 3.0f * cosv;

	float p = 1.0f + A1 * cosv + A2 * 0.5f * (B * cosv - 1.0f) + A3 * 0.5 * (5.0f * cosv * cosv * cosv - B);
	p = p / (4.0f * M_PI_F);
 
	p = (p * delta) / fabs(eyeDir.z);

	// Clamp the BRDF (page 7)
	if (p > 1.0f)
		p = 1.0f;
	else if (p < 0.0f)
		p = 0.0f;

	return p * kd;
}

float3 VelvetMaterial_ConstSample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const float3 kdVal,
		const float A1, const float A2, const float A3,
		const float delta) {
	if (!(requestedEvent & (DIFFUSE | REFLECT)) ||
			(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	const float3 kd = Spectrum_Clamp(kdVal);
	const float cosv = dot(-fixedDir, *sampledDir);;

	// Compute phase function

	const float B = 3.0f * cosv;

	float p = 1.0f + A1 * cosv + A2 * 0.5f * (B * cosv - 1.0f) + A3 * 0.5 * (5.0f * cosv * cosv * cosv - B);
	p = p / (4.0f * M_PI_F);
 
	p = (p * delta) / fabs(fixedDir.z);

	// Clamp the BRDF (page 7)
	if (p > 1.0f)
		p = 1.0f;
	else if (p < 0.0f)
		p = 0.0f;

	return kd * (p / *pdfW);
}

#if defined(PARAM_DISABLE_MAT_DYNAMIC_EVALUATION)
float3 VelvetMaterial_Evaluate(__global Material *material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	const float3 kdVal = Texture_GetSpectrumValue(material->velvet.kdTexIndex, hitPoint
			TEXTURES_PARAM);
	const float A1 = Texture_GetFloatValue(material->velvet.p1TexIndex, hitPoint
			TEXTURES_PARAM);
	const float A2 = Texture_GetFloatValue(material->velvet.p2TexIndex, hitPoint
			TEXTURES_PARAM);
	const float A3 = Texture_GetFloatValue(material->velvet.p3TexIndex, hitPoint
			TEXTURES_PARAM);
	const float delta = Texture_GetFloatValue(material->velvet.thicknessTexIndex, hitPoint
			TEXTURES_PARAM);

	return VelvetMaterial_ConstEvaluate(hitPoint, lightDir, eyeDir,
			event, directPdfW,
			kdVal, A1, A2, A3, delta);
}

float3 VelvetMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	const float3 kdVal = Texture_GetSpectrumValue(material->velvet.kdTexIndex, hitPoint
			TEXTURES_PARAM);
	const float A1 = Texture_GetFloatValue(material->velvet.p1TexIndex, hitPoint
			TEXTURES_PARAM);
	const float A2 = Texture_GetFloatValue(material->velvet.p2TexIndex, hitPoint
			TEXTURES_PARAM);
	const float A3 = Texture_GetFloatValue(material->velvet.p3TexIndex, hitPoint
			TEXTURES_PARAM);
	const float delta = Texture_GetFloatValue(material->velvet.thicknessTexIndex, hitPoint
			TEXTURES_PARAM);

	return VelvetMaterial_ConstSample(hitPoint, fixedDir, sampledDir,
			u0, u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
			passThroughEvent,
#endif
			pdfW, cosSampledDir, event, requestedEvent,
			kdVal, A1, A2, A3, delta);
}
#endif

#endif
