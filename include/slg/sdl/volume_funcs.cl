#line 2 "volume_funcs.cl"

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

#if defined(PARAM_HAS_VOLUMES)

//------------------------------------------------------------------------------
// ClearVolume scatter
//------------------------------------------------------------------------------

float3 ClearVolume_SigmaA(__global Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	const float3 sigmaA = Texture_GetSpectrumValue(&texs[vol->volume.clear.sigmaATexIndex], hitPoint
		TEXTURES_PARAM);
			
	return clamp(sigmaA, 0.f, INFINITY);
}

float3 ClearVolume_SigmaS(__global Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	return BLACK;
}

float3 ClearVolume_SigmaT(__global Volume *vol, __global HitPoint *hitPoint
	TEXTURES_PARAM_DECL) {
	return
			ClearVolume_SigmaA(vol, hitPoint
				TEXTURES_PARAM) +
			ClearVolume_SigmaS(vol, hitPoint
				TEXTURES_PARAM);
}

float ClearVolume_Scatter(__global Volume *vol,
#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
		__global
#endif
		Ray *ray, const float hitT,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		const bool scatteredStart, float3 *connectionThroughput,
		float3 *connectionEmission, __global HitPoint *tmpHitPoint
		TEXTURES_PARAM_DECL) {
	// Initialize tmpHitPoint
#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);
#else
	const float3 rayOrig = (float3)(ray->o.x, ray->o.y, ray->o.z);
	const float3 rayDir = (float3)(ray->d.x, ray->d.y, ray->d.z);
#endif
	VSTORE3F(rayDir, &tmpHitPoint->fixedDir.x);
	VSTORE3F(rayOrig, &tmpHitPoint->p.x);
	VSTORE2F((float2)(0.f, 0.f), &tmpHitPoint->uv.u);
	VSTORE3F(-rayDir, &tmpHitPoint->geometryN.x);
	VSTORE3F(-rayDir, &tmpHitPoint->shadeN.x);
#if defined(PARAM_ENABLE_TEX_HITPOINTCOLOR) || defined(PARAM_ENABLE_TEX_HITPOINTGREY)
	VSTORE2F(WHITE, &tmpHitPoint->color.c);
#endif
#if defined(PARAM_ENABLE_TEX_HITPOINTALPHA)
	VSTORE2F(1.f, &tmpHitPoint->alpha);
#endif
#if defined(PARAM_HAS_PASSTHROUGH)
	tmpHitPoint->passThroughEvent = passThroughEvent;
#endif
	tmpHitPoint->interiorVolumeIndex = NULL_INDEX;
	tmpHitPoint->exteriorVolumeIndex = NULL_INDEX;
	tmpHitPoint->intoObject = true;

	const float distance = hitT - ray->mint;	
	float3 transmittance = WHITE;

	const float3 sigma = ClearVolume_SigmaT(vol, tmpHitPoint
			TEXTURES_PARAM);
	if (!Spectrum_IsBlack(sigma)) {
		const float3 tau = clamp(distance * sigma, 0.f, INFINITY);
		transmittance = Spectrum_Exp(-tau);
	}

	// Apply volume transmittance
	*connectionThroughput *= transmittance;

	// Apply volume emission
	const uint emiTexIndex = vol->volume.volumeEmissionTexIndex;
	if (emiTexIndex != NULL_INDEX) {
		const float3 emiTex = Texture_GetSpectrumValue(&texs[emiTexIndex], tmpHitPoint
			TEXTURES_PARAM);
		*connectionEmission += *connectionThroughput * distance * clamp(emiTex, 0.f, INFINITY);
	}

	return -1.f;
}

//------------------------------------------------------------------------------
// Volume scatter
//------------------------------------------------------------------------------

float Volume_Scatter(__global Volume *vol,
#if !defined(RENDER_ENGINE_BIASPATHOCL) && !defined(RENDER_ENGINE_RTBIASPATHOCL)
		__global
#endif
		Ray *ray, const float hitT, const float passThrough,
		const bool scatteredStart, float3 *connectionThroughput,
		float3 *connectionEmission, __global HitPoint *tmpHitPoint
		TEXTURES_PARAM_DECL) {
	switch (vol->type) {
		case CLEAR_VOL:
			ClearVolume_Scatter(vol, ray, hitT, passThrough, scatteredStart,
					connectionThroughput, connectionEmission, tmpHitPoint
					TEXTURES_PARAM);
		default:
			return -1.f;
	}
}

#endif
