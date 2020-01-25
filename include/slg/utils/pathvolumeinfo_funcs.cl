#line 2 "pathvolumeinfo_funcs.cl"

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
// PathVolumeInfo
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void PathVolumeInfo_Init(__global PathVolumeInfo *pvi) {
	pvi->currentVolumeIndex = NULL_INDEX;
	pvi->volumeIndexListSize = 0;

	pvi->scatteredStart = false;
}

OPENCL_FORCE_INLINE void PathVolumeInfo_StartVolume(__global PathVolumeInfo *pvi, const uint volIndex) {
	if (volIndex == NULL_INDEX) {
		// NULL volume, I just ignore the volume
		return;
	}

	// Set the current volume
	pvi->currentVolumeIndex = volIndex;

	// Add the volume to the list
	pvi->volumeIndexList[(pvi->volumeIndexListSize)++] = volIndex;
}

OPENCL_FORCE_INLINE void PathVolumeInfo_AddVolume(__global PathVolumeInfo *pvi, const uint volIndex
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

OPENCL_FORCE_INLINE void PathVolumeInfo_RemoveVolume(__global PathVolumeInfo *pvi, const uint volIndex
		MATERIALS_PARAM_DECL) {
	if ((volIndex == NULL_INDEX) || (pvi->volumeIndexListSize == 0)) {
		// NULL volume or empty volume list
		return;
	}

	// Update the current volume and the list
	bool found = false;
	uint newCurrentVolumeIndex = NULL_INDEX;
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
		if ((newCurrentVolumeIndex == NULL_INDEX) ||
				(mats[pvi->volumeIndexList[i]].volume.priority >= mats[newCurrentVolumeIndex].volume.priority))
			newCurrentVolumeIndex = pvi->volumeIndexList[i];
	}
	pvi->currentVolumeIndex = newCurrentVolumeIndex;

	// Update the list size
	--(pvi->volumeIndexListSize);
}

OPENCL_FORCE_INLINE uint PathVolumeInfo_SimulateAddVolume(__global PathVolumeInfo *pvi, const uint volIndex
		MATERIALS_PARAM_DECL) {
	// A volume wins over current if and only if it is the same volume or has an
	// higher priority

	const uint currentVolumeIndex = pvi->currentVolumeIndex;
	if (currentVolumeIndex != NULL_INDEX) {
		if (volIndex != NULL_INDEX) {
			return (mats[currentVolumeIndex].volume.priority > mats[volIndex].volume.priority) ? currentVolumeIndex : volIndex;
		} else
			return pvi->currentVolumeIndex;
	} else
		return volIndex;
}

OPENCL_FORCE_INLINE uint PathVolumeInfo_SimulateRemoveVolume(__global PathVolumeInfo *pvi, const uint volIndex
		MATERIALS_PARAM_DECL) {
	if ((volIndex == NULL_INDEX) || (pvi->volumeIndexListSize == 0)) {
		// NULL volume or empty volume list
		return pvi->currentVolumeIndex;
	}

	// Update the current volume
	bool found = false;
	uint newCurrentVolumeIndex = NULL_INDEX;
	for (uint i = 0; i < pvi->volumeIndexListSize; ++i) {
		if ((!found) && (pvi->volumeIndexList[i] == volIndex)) {
			// Found the volume to remove
			found = true;
			continue;
		}

		// Update currentVolume. ">=" because I want to catch the last added volume.
		if ((newCurrentVolumeIndex == NULL_INDEX) ||
				(mats[pvi->volumeIndexList[i]].volume.priority >= mats[newCurrentVolumeIndex].volume.priority))
			newCurrentVolumeIndex = pvi->volumeIndexList[i];
	}

	return newCurrentVolumeIndex;
}

OPENCL_FORCE_INLINE void PathVolumeInfo_Update(__global PathVolumeInfo *pvi, const BSDFEvent eventType,
		__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	// Update only if it isn't a volume scattering and the material can TRANSMIT
	if (bsdf->isVolume)
		pvi->scatteredStart = true;
	else {
		pvi->scatteredStart = false;

		if(eventType & TRANSMIT) {
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

OPENCL_FORCE_INLINE bool PathVolumeInfo_CompareVolumePriorities(const uint vol1Index, const uint vol2Index
	MATERIALS_PARAM_DECL) {
	// A volume wins over another if and only if it is the same volume or has an
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

OPENCL_FORCE_INLINE bool PathVolumeInfo_ContinueToTrace(__global PathVolumeInfo *pvi,
		__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	// Check if the volume priority system has to be applied
	if (BSDF_GetEventTypes(bsdf
			MATERIALS_PARAM) & TRANSMIT) {
		// Ok, the surface can transmit so check if volume priority
		// system is telling me to continue to trace the ray

		// I have to continue to trace the ray if:
		//
		// 1) I'm entering an object and the interior volume has a
		// lower priority than the current one (or is the same volume).
		//
		// 2) I'm exiting an object and I'm not leaving the current volume.

		const bool intoObject = bsdf->hitPoint.intoObject;
		const uint bsdfInteriorVolIndex = BSDF_GetMaterialInteriorVolume(bsdf
			MATERIALS_PARAM);

		// Condition #1
		if (intoObject && PathVolumeInfo_CompareVolumePriorities(pvi->currentVolumeIndex, bsdfInteriorVolIndex
				MATERIALS_PARAM))
			return true;

		// Condition #2
		//
		// I have to calculate the potentially new currentVolume in order
		// to check if I'm leaving the current one
		if ((!intoObject) && (pvi->currentVolumeIndex != NULL_INDEX) &&
				(PathVolumeInfo_SimulateRemoveVolume(pvi, bsdfInteriorVolIndex
					MATERIALS_PARAM) == pvi->currentVolumeIndex))
			return true;
	}

	return false;
}

OPENCL_FORCE_INLINE void PathVolumeInfo_SetHitPointVolumes(__global PathVolumeInfo *pvi, 
		__global HitPoint *hitPoint,
		const uint matInteriorVolumeIndex,
		const uint matExteriorVolumeIndex
		MATERIALS_PARAM_DECL) {
	// Set interior and exterior volumes

	uint interiorVolumeIndex, exteriorVolumeIndex;
	const uint currentVolumeIndex = pvi->currentVolumeIndex;
	if (hitPoint->intoObject) {
		// From outside to inside the object

		interiorVolumeIndex = PathVolumeInfo_SimulateAddVolume(pvi, matInteriorVolumeIndex
				MATERIALS_PARAM);

		if (currentVolumeIndex == NULL_INDEX)
			exteriorVolumeIndex = matExteriorVolumeIndex;
		else {
			// if (!material->GetExteriorVolume()) there may be conflict here
			// between the material definition and the currentVolume value.
			// The currentVolume value wins.
			exteriorVolumeIndex = currentVolumeIndex;
		}

		if (exteriorVolumeIndex == NULL_INDEX) {
			// No volume information, I use the default volume
			exteriorVolumeIndex = scene->defaultVolumeIndex;
		}
	} else {
		// From inside to outside the object

		if (currentVolumeIndex == NULL_INDEX)
			interiorVolumeIndex = matInteriorVolumeIndex;
		else {
			// if (!material->GetInteriorVolume()) there may be conflict here
			// between the material definition and the currentVolume value.
			// The currentVolume value wins.
			interiorVolumeIndex = currentVolumeIndex;
		}
		
		if (interiorVolumeIndex == NULL_INDEX) {
			// No volume information, I use the default volume
			interiorVolumeIndex = scene->defaultVolumeIndex;
		}

		exteriorVolumeIndex = PathVolumeInfo_SimulateRemoveVolume(pvi, matInteriorVolumeIndex
				MATERIALS_PARAM);
	}

	hitPoint->interiorVolumeIndex = interiorVolumeIndex;
	hitPoint->exteriorVolumeIndex = exteriorVolumeIndex;
		
	hitPoint->interiorIorTexIndex = (interiorVolumeIndex != NULL_INDEX) ?
		mats[interiorVolumeIndex].volume.iorTexIndex : NULL_INDEX;
	hitPoint->exteriorIorTexIndex = (exteriorVolumeIndex != NULL_INDEX) ?
		mats[exteriorVolumeIndex].volume.iorTexIndex : NULL_INDEX;
}
