#line 2 "materialdefs_funcs_heterogenousvol.cl"

/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#if defined(PARAM_HAS_VOLUMES)
float3 SchlickScatter_Evaluate(
		__global HitPoint *hitPoint, const float3 localEyeDir, const float3 localLightDir,
		BSDFEvent *event, float *directPdfW,
		const float3 sigmaS, const float3 sigmaA, const float3 g) {
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

	return r * (1.f - k * k) / (compcostValue * compcostValue * (4.f * M_PI_F));
}

float3 SchlickScatter_Sample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const float3 sigmaS, const float3 sigmaA, const float3 g) {
	if (!(requestedEvent & (DIFFUSE | REFLECT)))
		return BLACK;

	const float3 gValue = clamp(g, -1.f, 1.f);
	const float3 k = gValue * (1.55f - .55f * gValue * gValue);
	const float gFilter = Spectrum_Filter(k);

	// Add a - because localEyeDir is reversed compared to the standard phase
	// function definition
	const float cost = -(2.f * u0 + gFilter - 1.f) / (2.f * gFilter * u0 - gFilter + 1.f);

	float3 x, y;
	CoordinateSystem(fixedDir, &x, &y);
	*sampledDir = SphericalDirectionWithFrame(sqrt(fmax(0.f, 1.f - cost * cost)), cost,
			2.f * M_PI_F * u1, x, y, fixedDir);

	// The - becomes a + because cost has been reversed above
	const float compcost = 1.f + gFilter * cost;
	*pdfW = (1.f - gFilter * gFilter) / (compcost * compcost * (4.f * M_PI_F));
	if (*pdfW <= 0.f)
		return BLACK;

	*cosSampledDir = fabs((*sampledDir).z);
	*event = DIFFUSE | REFLECT;

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
#endif

//------------------------------------------------------------------------------
// HeterogeneousVol material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_HETEROGENEOUS_VOL)

BSDFEvent HeterogeneousVolMaterial_GetEventTypes() {
	return DIFFUSE | REFLECT;
}

float3 HeterogeneousVolMaterial_Evaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 sigmaSTexVal, const float3 sigmaATexVal, const float3 gTexVal) {
	return SchlickScatter_Evaluate(
			hitPoint, eyeDir, lightDir,
			event, directPdfW,
			clamp(sigmaSTexVal, 0.f, INFINITY), clamp(sigmaATexVal, 0.f, INFINITY), gTexVal);
}

float3 HeterogeneousVolMaterial_Sample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const float3 sigmaSTexVal, const float3 sigmaATexVal, const float3 gTexVal) {
	return SchlickScatter_Sample(
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
