#line 2 "materialdefs_funcs_matte.cl"

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
// Matte material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MATTE)

BSDFEvent MatteMaterial_GetEventTypes() {
	return DIFFUSE | REFLECT;
}

bool MatteMaterial_IsDelta() {
	return false;
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 MatteMaterial_GetPassThroughTransparency(__global const Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	return BLACK;
}
#endif

float3 MatteMaterial_ConstEvaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 kdVal) {
	if (directPdfW)
		*directPdfW = fabs(lightDir.z * M_1_PI_F);

	*event = DIFFUSE | REFLECT;

	return Spectrum_Clamp(kdVal) * fabs(lightDir.z * M_1_PI_F);
}

float3 MatteMaterial_ConstSample(__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const float3 kdVal) {
	if (!(requestedEvent & (DIFFUSE | REFLECT)) ||
			(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	return Spectrum_Clamp(kdVal);
}

#endif

//------------------------------------------------------------------------------
// Rough matte material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_ROUGHMATTE)

BSDFEvent RoughMatteMaterial_GetEventTypes() {
	return DIFFUSE | REFLECT;
}

bool RoughMatteMaterial_IsDelta() {
	return false;
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 RoughMatteMaterial_GetPassThroughTransparency(__global const Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	return BLACK;
}
#endif

float3 RoughMatteMaterial_ConstEvaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
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

float3 RoughMatteMaterial_ConstSample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const float s, const float3 kdVal) {
	if (!(requestedEvent & (DIFFUSE | REFLECT)) ||
			(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
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

#endif
