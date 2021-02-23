#line 2 "volume_funcs.cl"

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

OPENCL_FORCE_INLINE float3 Volume_Emission(__global const Volume *vol, __global const HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const uint emiTexIndex = vol->volume.volumeEmissionTexIndex;
	if (emiTexIndex != NULL_INDEX) {
		const float3 emiTex = Texture_GetSpectrumValue(emiTexIndex, hitPoint
			TEXTURES_PARAM);
		return clamp(emiTex, 0.f, INFINITY);
	} else
		return BLACK;
}

OPENCL_FORCE_INLINE void Volume_InitializeTmpHitPoint(__global HitPoint *tmpHitPoint,
		const float3 rayOrig, const float3 rayDir, const float passThroughEvent) {
	// Initialize tmpHitPoint
	HitPoint_InitDefault(tmpHitPoint);

	VSTORE3F(rayDir, &tmpHitPoint->fixedDir.x);
	VSTORE3F(rayOrig, &tmpHitPoint->p.x);
	VSTORE3F(-rayDir, &tmpHitPoint->geometryN.x);
	VSTORE3F(-rayDir, &tmpHitPoint->interpolatedN.x);
	VSTORE3F(-rayDir, &tmpHitPoint->shadeN.x);
	tmpHitPoint->passThroughEvent = passThroughEvent;
	Transform_Init(&tmpHitPoint->localToWorld);
}

OPENCL_FORCE_INLINE float HomogeneousVolume_SegmentScatter(const float u, 
		const bool scatterAllowed, const float segmentLength,
		const float3 *sigmaA, const float3 *sigmaS, const float3 *emission,
		float3 *segmentTransmittance, float3 *segmentEmission) {
	// This code must work also with segmentLength = INFINITY

	bool scatter = false;
	*segmentTransmittance = WHITE;
	*segmentEmission = BLACK;

	//--------------------------------------------------------------------------
	// Check if there is a scattering event
	//--------------------------------------------------------------------------

	float scatterDistance = segmentLength;
	const float sigmaSValue = Spectrum_Filter(*sigmaS);
	if (scatterAllowed && (sigmaSValue > 0.f)) {
		// Determine scattering distance
		const float proposedScatterDistance = -log(1.f - u) / sigmaSValue;

		scatter = (proposedScatterDistance < segmentLength);
		scatterDistance = scatter ? proposedScatterDistance : segmentLength;

		// Note: scatterDistance can not be infinity because otherwise there would
		// have been a scatter event before.
		const float tau = scatterDistance * sigmaSValue;
		const float pdf = exp(-tau) * (scatter ? sigmaSValue : 1.f);
		*segmentTransmittance *= 1.f / pdf;
	}

	//--------------------------------------------------------------------------
	// Volume transmittance
	//--------------------------------------------------------------------------
	
	const float3 sigmaT = *sigmaA + *sigmaS;
	if (!Spectrum_IsBlack(sigmaT)) {
		if (isinf(scatterDistance)) {
			// This avoid NaN in case scatterDistance is inf
			*segmentTransmittance = BLACK;
		} else {
			const float3 tau = scatterDistance * sigmaT;
			*segmentTransmittance *= Spectrum_Exp(-tau) * (scatter ? sigmaT : WHITE);
		}
	}

	//--------------------------------------------------------------------------
	// Volume emission
	//--------------------------------------------------------------------------

	*segmentEmission += (*segmentTransmittance) * scatterDistance * (*emission);

	return scatter ? scatterDistance : -1.f;
}

//------------------------------------------------------------------------------
// ClearVolume scatter
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 ClearVolume_SigmaA(__global const Volume *vol, __global const HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const float3 sigmaA = Texture_GetSpectrumValue(vol->volume.clear.sigmaATexIndex, hitPoint
		TEXTURES_PARAM);
			
	return clamp(sigmaA, 0.f, INFINITY);
}

OPENCL_FORCE_INLINE float3 ClearVolume_SigmaS(__global const Volume *vol, __global const HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	return BLACK;
}

OPENCL_FORCE_INLINE float3 ClearVolume_SigmaT(__global const Volume *vol, __global const HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	return
			ClearVolume_SigmaA(vol, hitPoint
				TEXTURES_PARAM) +
			ClearVolume_SigmaS(vol, hitPoint
				TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE float ClearVolume_Scatter(__global const Volume *vol,
		__global Ray *ray, const float hitT,
		const float passThroughEvent,
		const bool scatteredStart, float3 *connectionThroughput,
		float3 *connectionEmission, __global HitPoint *tmpHitPoint
		TEXTURES_PARAM_DECL) {
	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);

	// Initialize tmpHitPoint
	Volume_InitializeTmpHitPoint(tmpHitPoint, rayOrig, rayDir, passThroughEvent);

	const float distance = hitT - ray->mint;	
	float3 transmittance = WHITE;

	const float3 sigmaT = ClearVolume_SigmaT(vol, tmpHitPoint
			TEXTURES_PARAM);
	if (!Spectrum_IsBlack(sigmaT)) {
		const float3 tau = clamp(distance * sigmaT, 0.f, INFINITY);
		transmittance = Spectrum_Exp(-tau);
	}

	// Apply volume transmittance
	*connectionThroughput *= transmittance;

	// Apply volume emission
	const uint emiTexIndex = vol->volume.volumeEmissionTexIndex;
	if (emiTexIndex != NULL_INDEX) {
		const float3 emiTex = Texture_GetSpectrumValue(emiTexIndex, tmpHitPoint
			TEXTURES_PARAM);
		*connectionEmission += *connectionThroughput * distance * clamp(emiTex, 0.f, INFINITY);
	}

	return -1.f;
}

//------------------------------------------------------------------------------
// HomogeneousVolume scatter
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 HomogeneousVolume_SigmaA(__global const Volume *vol, __global const HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const float3 sigmaA = Texture_GetSpectrumValue(vol->volume.homogenous.sigmaATexIndex, hitPoint
		TEXTURES_PARAM);
			
	return clamp(sigmaA, 0.f, INFINITY);
}

OPENCL_FORCE_INLINE float3 HomogeneousVolume_SigmaS(__global const Volume *vol, __global const HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const float3 sigmaS = Texture_GetSpectrumValue(vol->volume.homogenous.sigmaSTexIndex, hitPoint
		TEXTURES_PARAM);
			
	return clamp(sigmaS, 0.f, INFINITY);
}

OPENCL_FORCE_INLINE float HomogeneousVolume_Scatter(__global const Volume *vol,
		__global Ray *ray, const float hitT,
		const float passThroughEvent,
		const bool scatteredStart, float3 *connectionThroughput,
		float3 *connectionEmission, __global HitPoint *tmpHitPoint
		TEXTURES_PARAM_DECL) {
	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);

	// Initialize tmpHitPoint
	Volume_InitializeTmpHitPoint(tmpHitPoint, rayOrig, rayDir, passThroughEvent);

	const float segmentLength = hitT - ray->mint;

	// Check if I have to support multi-scattering
	const bool scatterAllowed = (!scatteredStart || vol->volume.homogenous.multiScattering);
	
	const float3 sigmaA = HomogeneousVolume_SigmaA(vol, tmpHitPoint
			TEXTURES_PARAM);
	const float3 sigmaS = HomogeneousVolume_SigmaS(vol, tmpHitPoint
			TEXTURES_PARAM);
	const float3 emission = Volume_Emission(vol, tmpHitPoint
			TEXTURES_PARAM);

	float3 segmentTransmittance, segmentEmission;
	const float scatterDistance = HomogeneousVolume_SegmentScatter(passThroughEvent, scatterAllowed,
			segmentLength, &sigmaA, &sigmaS, &emission,
			&segmentTransmittance, &segmentEmission);

	// I need to update first connectionEmission and than connectionThroughput
	*connectionEmission += *connectionThroughput * emission;
	*connectionThroughput *= segmentTransmittance;

	return (scatterDistance == -1.f) ? -1.f : (ray->mint + scatterDistance);
}

//------------------------------------------------------------------------------
// HeterogeneousVolume scatter
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 HeterogeneousVolume_SigmaA(__global const Volume *vol, __global const HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const float3 sigmaA = Texture_GetSpectrumValue(vol->volume.heterogenous.sigmaATexIndex, hitPoint
		TEXTURES_PARAM);
			
	return clamp(sigmaA, 0.f, INFINITY);
}

OPENCL_FORCE_INLINE float3 HeterogeneousVolume_SigmaS(__global const Volume *vol, __global const HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const float3 sigmaS = Texture_GetSpectrumValue(vol->volume.heterogenous.sigmaSTexIndex, hitPoint
		TEXTURES_PARAM);
			
	return clamp(sigmaS, 0.f, INFINITY);
}

OPENCL_FORCE_INLINE float3 HeterogeneousVolume_SigmaT(__global const Volume *vol, __global const HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	return HeterogeneousVolume_SigmaA(vol, hitPoint TEXTURES_PARAM) +
			HeterogeneousVolume_SigmaS(vol, hitPoint TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE float HeterogeneousVolume_Scatter(__global const Volume *vol,
		__global Ray *ray, const float hitT,
		const float passThroughEvent,
		const bool scatteredStart, float3 *connectionThroughput,
		float3 *connectionEmission, __global HitPoint *tmpHitPoint
		TEXTURES_PARAM_DECL) {
	// I need a sequence of pseudo-random numbers starting form a floating point
	// pseudo-random number
	Seed seed;
	Rnd_InitFloat(passThroughEvent, &seed);

	const float stepSize = vol->volume.heterogenous.stepSize;
	const uint maxStepsCount = vol->volume.heterogenous.maxStepsCount;

	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);

	// Compute the number of steps to evaluate the volume
	const float segmentLength = hitT - ray->mint;

	// Handle the case when segmentLength is infinity or a very large number
	//
	// Note: the old code"Min(maxStepsCount, Ceil2UInt(segmentLength / stepSize))"
	// can overflow for large values of segmentLength so I have to use
	// "Ceil2UInt(Min((float)maxStepsCount, segmentLength / stepSize))"
	const uint steps = Ceil2UInt(fmin((float)maxStepsCount, segmentLength / stepSize));

	const float currentStepSize = fmin(segmentLength / steps, maxStepsCount * stepSize);

	// Check if I have to support multi-scattering
	const bool scatterAllowed = (!scatteredStart || vol->volume.heterogenous.multiScattering);

	// Initialize tmpHitPoint
	Volume_InitializeTmpHitPoint(tmpHitPoint, rayOrig, rayDir, passThroughEvent);

	for (uint s = 0; s < steps; ++s) {
		// Compute the scattering over the current step
		const float evaluationPoint = (s + Rnd_FloatValue(&seed)) * currentStepSize;

		VSTORE3F(rayOrig + (ray->mint + evaluationPoint) * rayDir, &tmpHitPoint->p.x);

		// Volume segment values
		const float3 sigmaA = HeterogeneousVolume_SigmaA(vol, tmpHitPoint
				TEXTURES_PARAM);
		const float3 sigmaS = HeterogeneousVolume_SigmaS(vol, tmpHitPoint
				TEXTURES_PARAM);
		const float3 emission = Volume_Emission(vol, tmpHitPoint
				TEXTURES_PARAM);

		// Evaluate the current segment like if it was an homogenous volume
		//
		// This could be optimized by inlining the code and exploiting
		// exp(a) * exp(b) = exp(a + b) in order to evaluate a single exp() at
		// the end instead of one each step.
		// However the code would be far less simple and readable.
		float3 segmentTransmittance, segmentEmission;
		const float scatterDistance = HomogeneousVolume_SegmentScatter(Rnd_FloatValue(&seed), scatterAllowed,
				currentStepSize, &sigmaA, &sigmaS, &emission,
				&segmentTransmittance, &segmentEmission);

		// I need to update first connectionEmission and than connectionThroughput
		*connectionEmission += *connectionThroughput * emission;
		*connectionThroughput *= segmentTransmittance;

		if (scatterDistance >= 0.f)
			return ray->mint + s * currentStepSize + scatterDistance;
	}

	return -1.f;
}

//------------------------------------------------------------------------------
// Volume scatter
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float Volume_Scatter(__global const Volume *vol,
		__global Ray *ray, const float hitT,
		const float passThrough,
		const bool scatteredStart, float3 *connectionThroughput,
		float3 *connectionEmission, __global HitPoint *tmpHitPoint
		TEXTURES_PARAM_DECL) {
	switch (vol->type) {
		case CLEAR_VOL:
			return ClearVolume_Scatter(vol, ray, hitT,
					passThrough, scatteredStart,
					connectionThroughput, connectionEmission, tmpHitPoint
					TEXTURES_PARAM);
		case HOMOGENEOUS_VOL:
			return HomogeneousVolume_Scatter(vol, ray, hitT,
					passThrough, scatteredStart,
					connectionThroughput, connectionEmission, tmpHitPoint
					TEXTURES_PARAM);
		case HETEROGENEOUS_VOL:
			return HeterogeneousVolume_Scatter(vol, ray, hitT,
					passThrough, scatteredStart,
					connectionThroughput, connectionEmission, tmpHitPoint
					TEXTURES_PARAM);
		default:
			return -1.f;
	}
}
