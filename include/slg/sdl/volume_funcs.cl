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
// PathVolumeInfo
//------------------------------------------------------------------------------

void PathVolumeInfo_Init(__global PathVolumeInfo *pvi) {
	pvi->currentVolumeIndex = NULL_INDEX;
	pvi->volumeIndexListSize = 0;

	pvi->scatteredStart = false;
}

void PathVolumeInfo_AddVolume(__global PathVolumeInfo *pvi, const uint volIndex
		MATERIALS_PARAM_DECL) {
	if ((volIndex == NULL_INDEX) || (pvi->volumeIndexListSize == OPENCL_PATHVOLUMEINFO_SIZE)) {
		// NULL volume or out of space, I just ignore the volume
		return;
	}

	// Update the current volume. ">=" because I want to catch the last added volume.
	if ((pvi->currentVolumeIndex == NULL_INDEX) ||
			(mats[volIndex].volume.priority >= mats[pvi->currentVolumeIndex].volume.priority))
		pvi->currentVolumeIndex = volIndex;

	// Add the volume to the list
	pvi->volumeIndexList[(pvi->volumeIndexListSize)++] = volIndex;
}

void PathVolumeInfo_RemoveVolume(__global PathVolumeInfo *pvi, const uint volIndex
		MATERIALS_PARAM_DECL) {
	if ((volIndex == NULL_INDEX) || (pvi->volumeIndexListSize == 0)) {
		// NULL volume or empty volume list
		return;
	}

	// Update the current volume and the list
	bool found = false;
	uint currentVolume = NULL_INDEX;
	for (uint i = 0; i < pvi->volumeIndexListSize; ++i) {
		if (found) {
			// Re-compact the list
			pvi->volumeIndexList[i - 1] = pvi->volumeIndexList[i];
		} else if (pvi->volumeIndexList[i] == volIndex) {
			// Found the volume to remove
			found = true;
			continue;
		}

		// Update currentVolume. ">=" because I want to catch the last added volume.
		if ((currentVolume == NULL_INDEX) ||
				(mats[pvi->volumeIndexList[i]].volume.priority >= mats[currentVolume].volume.priority))
			currentVolume = pvi->volumeIndexList[i];
	}
	pvi->currentVolumeIndex = currentVolume;

	// Update the list size
	--(pvi->volumeIndexListSize);
}

void PathVolumeInfo_Update(__global PathVolumeInfo *pvi, const BSDFEvent eventType,
		__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	// Update only if it isn't a volume scattering and the material can TRANSMIT
	if (bsdf->isVolume)
		pvi->scatteredStart = true;
	else {
		pvi->scatteredStart = false;

		if(eventType  & TRANSMIT) {
			const uint volIndex = BSDF_GetMaterialInteriorVolume(bsdf
					MATERIALS_PARAM);

			if (bsdf->hitPoint.intoObject)
				PathVolumeInfo_AddVolume(pvi, volIndex
						MATERIALS_PARAM);
			else
				PathVolumeInfo_RemoveVolume(pvi, volIndex
						MATERIALS_PARAM);
		}
	}
}

bool PathVolumeInfo_CompareVolumePriorities(const uint vol1Index, const uint vol2Index
	MATERIALS_PARAM_DECL) {
	// A volume wins over another if and only if it is the same volume or has a
	// higher priority

	if (vol1Index != NULL_INDEX) {
		if (vol2Index != NULL_INDEX) {
			if (vol1Index == vol2Index)
				return true;
			else
				return (mats[vol1Index].volume.priority > mats[vol2Index].volume.priority);
		} else
			return false;
	} else
		return false;
}

bool PathVolumeInfo_ContinueToTrace(__global PathVolumeInfo *pvi, __global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	// Check if the volume priority system has to be applied
	if (BSDF_GetEventTypes(bsdf
			MATERIALS_PARAM) & TRANSMIT) {
		// Ok, the surface can transmit so check if volume priority
		// system is telling me to continue to trace the ray

		// I have to continue to trace the ray if:
		//
		// 1) I'm entering an object and the interior volume has a
		// higher priority than the current one.
		//
		// 2) I'm exiting an object and I'm leaving the current volume

		const uint volIndex = BSDF_GetMaterialInteriorVolume(bsdf
			MATERIALS_PARAM);
		if (
			// Condition #1
			(bsdf->hitPoint.intoObject && PathVolumeInfo_CompareVolumePriorities(pvi->currentVolumeIndex, volIndex
				MATERIALS_PARAM)) ||
			// Condition #2
			(!bsdf->hitPoint.intoObject && (pvi->currentVolumeIndex != volIndex))) {
			// Ok, green light for continuing to trace the ray
			return true;
		}
	}

	return false;
}

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
		__global Ray *ray, const float hitT,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		const bool scatteredStart, float3 *connectionThroughput,
		float3 *connectionEmission, __global HitPoint *tmpHitPoint
		TEXTURES_PARAM_DECL) {
	// Initialize tmpHitPoint
	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);
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
#if defined(PARAM_HAS_VOLUMES)
	tmpHitPoint->interiorVolumeIndex = NULL_INDEX;
	tmpHitPoint->exteriorVolumeIndex = NULL_INDEX;
	tmpHitPoint->intoObject = true;
#endif

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
		__global Ray *ray, const float hitT, const float passThrough,
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
