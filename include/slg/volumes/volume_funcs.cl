#line 2 "volume_funcs.cl"

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

#if defined(PARAM_HAS_VOLUMES)

OPENCL_FORCE_NOT_INLINE void Volume_InitializeTmpHitPoint(__global HitPoint *tmpHitPoint,
		const float3 rayOrig, const float3 rayDir, const float passThroughEvent) {
	// Initialize tmpHitPoint
	VSTORE3F(rayDir, &tmpHitPoint->fixedDir.x);
	VSTORE3F(rayOrig, &tmpHitPoint->p.x);
	VSTORE2F((float2)(0.f, 0.f), &tmpHitPoint->uv.u);
	VSTORE3F(-rayDir, &tmpHitPoint->geometryN.x);
	VSTORE3F(-rayDir, &tmpHitPoint->shadeN.x);
#if defined(PARAM_HAS_BUMPMAPS)
	VSTORE3F((float3)(0.f, 0.f, 0.f), &tmpHitPoint->dpdu.x);
	VSTORE3F((float3)(0.f, 0.f, 0.f), &tmpHitPoint->dpdv.x);
	VSTORE3F((float3)(0.f, 0.f, 0.f), &tmpHitPoint->dndu.x);
	VSTORE3F((float3)(0.f, 0.f, 0.f), &tmpHitPoint->dndv.x);
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR) || defined(PARAM_ENABLE_TEX_HITPOINTGREY) || defined(PARAM_TRIANGLE_LIGHT_HAS_VERTEX_COLOR)
	VSTORE3F(WHITE, tmpHitPoint->color.c);
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
	tmpHitPoint->alpha = 0.f;
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
	tmpHitPoint->passThroughEvent = passThroughEvent;
#endif
	Matrix4x4_IdentityGlobal(&tmpHitPoint->worldToLocal);
	tmpHitPoint->interiorVolumeIndex = NULL_INDEX;
	tmpHitPoint->exteriorVolumeIndex = NULL_INDEX;
	tmpHitPoint->intoObject = true;
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
		const float3 tau = scatterDistance * sigmaT;
		*segmentTransmittance *= Spectrum_Exp(-tau) * (scatter ? sigmaT : WHITE);
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

#if defined (PARAM_ENABLE_MAT_CLEAR_VOL)
OPENCL_FORCE_NOT_INLINE float3 ClearVolume_SigmaA(__global const Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const float3 sigmaA = Texture_GetSpectrumValue(vol->volume.clear.sigmaATexIndex, hitPoint
		TEXTURES_PARAM);
			
	return clamp(sigmaA, 0.f, INFINITY);
}

OPENCL_FORCE_INLINE float3 ClearVolume_SigmaS(__global const Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	return BLACK;
}

OPENCL_FORCE_INLINE float3 ClearVolume_SigmaT(__global const Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	return
			ClearVolume_SigmaA(vol, hitPoint
				TEXTURES_PARAM) +
			ClearVolume_SigmaS(vol, hitPoint
				TEXTURES_PARAM);
}

OPENCL_FORCE_NOT_INLINE float ClearVolume_Scatter(__global const Volume *vol,
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
#endif

//------------------------------------------------------------------------------
// HomogeneousVolume scatter
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_HOMOGENEOUS_VOL)
OPENCL_FORCE_NOT_INLINE float3 HomogeneousVolume_SigmaA(__global const Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const float3 sigmaA = Texture_GetSpectrumValue(vol->volume.homogenous.sigmaATexIndex, hitPoint
		TEXTURES_PARAM);
			
	return clamp(sigmaA, 0.f, INFINITY);
}

OPENCL_FORCE_NOT_INLINE float3 HomogeneousVolume_SigmaS(__global const Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const float3 sigmaS = Texture_GetSpectrumValue(vol->volume.homogenous.sigmaSTexIndex, hitPoint
		TEXTURES_PARAM);
			
	return clamp(sigmaS, 0.f, INFINITY);
}

OPENCL_FORCE_NOT_INLINE float3 HomogeneousVolume_Emission(__global const Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const uint emiTexIndex = vol->volume.volumeEmissionTexIndex;
	if (emiTexIndex != NULL_INDEX) {
		const float3 emiTex = Texture_GetSpectrumValue(emiTexIndex, hitPoint
			TEXTURES_PARAM);
		return clamp(emiTex, 0.f, INFINITY);
	} else
		return BLACK;
}

OPENCL_FORCE_NOT_INLINE float HomogeneousVolume_Scatter(__global const Volume *vol,
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
	const float3 emission = HomogeneousVolume_Emission(vol, tmpHitPoint
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
#endif

//------------------------------------------------------------------------------
// HomogeneousVolume scatter
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_HETEROGENEOUS_VOL)
OPENCL_FORCE_NOT_INLINE float3 HeterogeneousVolume_SigmaA(__global const Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const float3 sigmaA = Texture_GetSpectrumValue(vol->volume.heterogenous.sigmaATexIndex, hitPoint
		TEXTURES_PARAM);
			
	return clamp(sigmaA, 0.f, INFINITY);
}

OPENCL_FORCE_NOT_INLINE float3 HeterogeneousVolume_SigmaS(__global const Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const float3 sigmaS = Texture_GetSpectrumValue(vol->volume.heterogenous.sigmaSTexIndex, hitPoint
		TEXTURES_PARAM);
			
	return clamp(sigmaS, 0.f, INFINITY);
}

OPENCL_FORCE_INLINE float3 HeterogeneousVolume_SigmaT(__global const Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	return HeterogeneousVolume_SigmaA(vol, hitPoint TEXTURES_PARAM) +
			HeterogeneousVolume_SigmaS(vol, hitPoint TEXTURES_PARAM);
}

OPENCL_FORCE_NOT_INLINE float HeterogeneousVolume_Scatter(__global const Volume *vol,
		__global Ray *ray, const float hitT,
		const float passThroughEvent,
		const bool scatteredStart, float3 *connectionThroughput,
		float3 *connectionEmission, __global HitPoint *tmpHitPoint
		TEXTURES_PARAM_DECL) {
	// Compute the number of steps to evaluate the volume
	// Integrates in steps of at most stepSize
	// unless stepSize is too small compared to the total length
	const float mint = ray->mint;
	const float rayLen = hitT - mint;

	//--------------------------------------------------------------------------
	// Handle the case when hitT is infinity or a very large number
	//--------------------------------------------------------------------------

	const float stepSize = vol->volume.heterogenous.stepSize;
	const uint maxStepsCount = vol->volume.heterogenous.maxStepsCount;
	uint steps;
	float ss;
	if (rayLen == INFINITY) {
		steps = maxStepsCount;
		ss = stepSize;
	} else {
		// Note: Ceil2UInt() of an out of range number is 0
		const float fsteps = rayLen / fmax(MachineEpsilon_E(rayLen), stepSize);
		if (fsteps >= maxStepsCount)
			steps = maxStepsCount;
		else
			steps = Ceil2UInt(fsteps);

		ss = rayLen / steps; // Effective step size
	}

	const float totalDistance = ss * steps;

	// Evaluate the scattering at the path origin

	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);

	// Initialize tmpHitPoint
	Volume_InitializeTmpHitPoint(tmpHitPoint, rayOrig, rayDir, passThroughEvent);

	const bool scatterAllowed = (!scatteredStart || vol->volume.heterogenous.multiScattering);

	//--------------------------------------------------------------------------
	// Find the scattering point if there is one
	//--------------------------------------------------------------------------

	float oldSigmaS = Spectrum_Filter(HeterogeneousVolume_SigmaS(vol, tmpHitPoint
			TEXTURES_PARAM));
	float u = passThroughEvent;
	float scatterDistance = totalDistance;
	float t = -1.f;
	float pdf = 1.f;
	for (uint s = 1; s <= steps; ++s) {
		// Compute the mean scattering over the current step
		VSTORE3F(rayOrig + (mint + s * ss) * rayDir, &tmpHitPoint->p.x);

		// Check if there is a scattering event
		const float newSigmaS = Spectrum_Filter(HeterogeneousVolume_SigmaS(vol, tmpHitPoint
			TEXTURES_PARAM));
		const float halfWaySigmaS = (oldSigmaS + newSigmaS) * .5f;
		oldSigmaS = newSigmaS;

		// Skip the step if no scattering can occur
		if (halfWaySigmaS <= 0.f)
			continue;

		// Determine scattering distance
		const float d = log(1.f - u) / halfWaySigmaS; // The real distance is ray.mint-d
		const bool scatter = scatterAllowed && (d > (s - 1U) * ss - totalDistance);
		if (!scatter) {
			if (scatterAllowed)
				pdf *= exp(-ss * halfWaySigmaS);

			// Update the random variable to account for
			// the current step
			u -= (1.f - u) * (exp(oldSigmaS * ss) - 1.f);
			continue;
		}

		// The ray is scattered
		scatterDistance = (s - 1U) * ss - d;
		t = mint + scatterDistance;
		pdf *= exp(d * halfWaySigmaS) * oldSigmaS;

		VSTORE3F(rayOrig + t * rayDir, &tmpHitPoint->p.x);
		*connectionThroughput *= HeterogeneousVolume_SigmaT(vol, tmpHitPoint
				TEXTURES_PARAM);
		break;
	}

	//--------------------------------------------------------------------------
	// Now I know the distance of the scattering point (if there is one) and
	// I can calculate transmittance and emission
	//--------------------------------------------------------------------------
	
	steps = Ceil2UInt(scatterDistance / fmax(MachineEpsilon_E(scatterDistance), stepSize));
	ss = scatterDistance / steps;

	float3 tau = BLACK;
	float3 emission = BLACK;
	VSTORE3F(rayOrig + mint * rayDir, &tmpHitPoint->p.x);
	float3 oldSigmaT = HeterogeneousVolume_SigmaT(vol, tmpHitPoint
			TEXTURES_PARAM);
	const uint emiTexIndex = vol->volume.volumeEmissionTexIndex;
	for (uint s = 1; s <= steps; ++s) {
		VSTORE3F(rayOrig + (mint + s * ss) * rayDir, &tmpHitPoint->p.x);

		// Accumulate tau values
		const float3 newSigmaT = HeterogeneousVolume_SigmaT(vol, tmpHitPoint
				TEXTURES_PARAM);
		const float3 halfWaySigmaT = (oldSigmaT + newSigmaT) * .5f;
		tau += clamp(ss * halfWaySigmaT, 0.f, INFINITY);
		oldSigmaT = newSigmaT;

		// Accumulate volume emission
		if (emiTexIndex != NULL_INDEX) {
			const float3 emiTex = Texture_GetSpectrumValue(emiTexIndex, tmpHitPoint
				TEXTURES_PARAM);
			emission += Spectrum_Exp(-tau) * (ss * clamp(emiTex, 0.f, INFINITY));
		}
	}
	
	// Apply volume transmittance
	const float3 transmittance = Spectrum_Exp(-tau);
	*connectionThroughput *= transmittance / pdf;

	// Add volume emission
	*connectionEmission += *connectionThroughput * emission;

	return t;
}
#endif

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
#if defined (PARAM_ENABLE_MAT_CLEAR_VOL)
		case CLEAR_VOL:
			return ClearVolume_Scatter(vol, ray, hitT,
					passThrough, scatteredStart,
					connectionThroughput, connectionEmission, tmpHitPoint
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_HOMOGENEOUS_VOL)
		case HOMOGENEOUS_VOL:
			return HomogeneousVolume_Scatter(vol, ray, hitT,
					passThrough, scatteredStart,
					connectionThroughput, connectionEmission, tmpHitPoint
					TEXTURES_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_HETEROGENEOUS_VOL)
		case HETEROGENEOUS_VOL:
			return HeterogeneousVolume_Scatter(vol, ray, hitT,
					passThrough, scatteredStart,
					connectionThroughput, connectionEmission, tmpHitPoint
					TEXTURES_PARAM);
#endif
		default:
			return -1.f;
	}
}

#endif
