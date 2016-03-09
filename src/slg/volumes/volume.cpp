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

#include <cstddef>

#include "slg/volumes/volume.h"
#include "slg/bsdf/bsdf.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Volume
//------------------------------------------------------------------------------

Properties Volume::ToProperties() const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.volumes." + name + ".priority")(priority));
	props.Set(Property("scene.volumes." + name + ".ior")(iorTex->GetName()));
	if (volumeEmissionTex) {
		props.Set(Property("scene.volumes." + name + ".emission")(volumeEmissionTex->GetName()));
		props.Set(Property("scene.volumes." + name + ".emission.id")(volumeLightID));
	}
	props.Set(Property("scene.volumes." + name + ".id")(matID));

	return props;
}

//------------------------------------------------------------------------------
// PathVolumeInfo
//------------------------------------------------------------------------------

PathVolumeInfo::PathVolumeInfo() {
	currentVolume = NULL;
	volumeListSize = 0;

	scatteredStart = false;
}

void PathVolumeInfo::AddVolume(const Volume *vol) {
	if ((!vol) || (volumeListSize == PATHVOLUMEINFO_SIZE)) {
		// NULL volume or out of space, I just ignore the volume
		return;
	}

	// Update the current volume. ">=" because I want to catch the last added volume.
	if (!currentVolume || (vol->GetPriority() >= currentVolume->GetPriority()))
		currentVolume = vol;

	// Add the volume to the list
	volumeList[volumeListSize++] = vol;
}

void PathVolumeInfo::RemoveVolume(const Volume *vol) {
	if ((!vol) || (volumeListSize == 0)) {
		// NULL volume or empty volume list
		return;
	}

	// Update the current volume and the list
	bool found = false;
	currentVolume = NULL;
	for (u_int i = 0; i < volumeListSize; ++i) {
		if (found) {
			// Re-compact the list
			volumeList[i - 1] = volumeList[i];
		} else if (volumeList[i] == vol) {
			// Found the volume to remove
			found = true;
			continue;
		}

		// Update currentVolume. ">=" because I want to catch the last added volume.
		if (!currentVolume || (volumeList[i]->GetPriority() >= currentVolume->GetPriority()))
			currentVolume = volumeList[i];
	}

	// Update the list size
	--volumeListSize;
}

const Volume *PathVolumeInfo::SimulateAddVolume(const Volume *vol) const {
	// A volume wins over current if and only if it is the same volume or has an
	// higher priority

	if (currentVolume) {
		if (vol) {
			return (currentVolume->GetPriority() > vol->GetPriority()) ? currentVolume : vol;
		} else
			return currentVolume;
	} else
		return vol;
}

const Volume *PathVolumeInfo::SimulateRemoveVolume(const Volume *vol) const {
	if ((!vol) || (volumeListSize == 0)) {
		// NULL volume or empty volume list
		return currentVolume;
	}

	// Update the current volume
	bool found = false;
	const Volume *newCurrentVolume = NULL;
	for (u_int i = 0; i < volumeListSize; ++i) {
		if (!found && volumeList[i] == vol) {
			// Found the volume to remove
			found = true;
			continue;
		}

		// Update newCurrentVolume. ">=" because I want to catch the last added volume.
		if (!newCurrentVolume || (volumeList[i]->GetPriority() >= newCurrentVolume->GetPriority()))
			newCurrentVolume = volumeList[i];
	}

	return newCurrentVolume;
}

void PathVolumeInfo::Update(const BSDFEvent eventType, const BSDF &bsdf) {
	// Update only if it isn't a volume scattering and the material can TRANSMIT
	if (bsdf.IsVolume())
		scatteredStart = true;
	else {
		scatteredStart = false;

		if(eventType & TRANSMIT) {
			if (bsdf.hitPoint.intoObject)
				AddVolume(bsdf.GetMaterialInteriorVolume());
			else
				RemoveVolume(bsdf.GetMaterialInteriorVolume());
		}
	}
}

bool PathVolumeInfo::CompareVolumePriorities(const Volume *vol1, const Volume *vol2) {
	// A volume wins over another if and only if it is the same volume or has an
	// higher priority

	if (vol1) {
		if (vol2) {
			if (vol1 == vol2)
				return true;
			else
				return (vol1->GetPriority() > vol2->GetPriority());
		} else
			return false;
	} else
		return false;
}

bool PathVolumeInfo::ContinueToTrace(const BSDF &bsdf) const {
	// Check if the volume priority system has to be applied
	if (bsdf.GetEventTypes() & TRANSMIT) {
		// Ok, the surface can transmit so check if volume priority
		// system is telling me to continue to trace the ray

		// I have to continue to trace the ray if:
		//
		// 1) I'm entering an object and the interior volume has a
		// lower priority than the current one (or is the same volume).
		//
		// 2) I'm exiting an object and I'm not leaving the current volume.

		const Volume *bsdfInteriorVol = bsdf.GetMaterialInteriorVolume();

		// Condition #1
		if (bsdf.hitPoint.intoObject && CompareVolumePriorities(currentVolume, bsdfInteriorVol))
			return true;

		// Condition #2
		//
		// I have to calculate the potentially new currentVolume in order
		// to check if I'm leaving the current one
		if ((!bsdf.hitPoint.intoObject) && currentVolume && (SimulateRemoveVolume(bsdfInteriorVol) == currentVolume))
			return true;
	}

	return false;
}

void  PathVolumeInfo::SetHitPointVolumes(HitPoint &hitPoint,
		const Volume *matInteriorVolume,
		const Volume *matExteriorVolume,
		const Volume *defaultWorldVolume) const {
	// Set interior and exterior volumes

	if (hitPoint.intoObject) {
		// From outside to inside the object

		hitPoint.interiorVolume = SimulateAddVolume(matInteriorVolume);

		if (!currentVolume)
			hitPoint.exteriorVolume = matExteriorVolume;
		else {
			// if (!material->GetExteriorVolume()) there may be conflict here
			// between the material definition and the currentVolume value.
			// The currentVolume value wins.
			hitPoint.exteriorVolume = currentVolume;
		}
		
		if (!hitPoint.exteriorVolume) {
			// No volume information, I use the default volume
			hitPoint.exteriorVolume = defaultWorldVolume;
		}
	} else {
		// From inside to outside the object

		if (!currentVolume)
			hitPoint.interiorVolume = matInteriorVolume;
		else {
			// if (!material->GetInteriorVolume()) there may be conflict here
			// between the material definition and the currentVolume value.
			// The currentVolume value wins.
			hitPoint.interiorVolume = currentVolume;
		}
		
		if (!hitPoint.interiorVolume) {
			// No volume information, I use the default volume
			hitPoint.interiorVolume = defaultWorldVolume;
		}

		hitPoint.exteriorVolume = SimulateRemoveVolume(matInteriorVolume);
	}
}

//------------------------------------------------------------------------------
// SchlickScatter
//------------------------------------------------------------------------------

SchlickScatter::SchlickScatter(const Volume *vol, const Texture *gTex) :
	volume(vol), g(gTex) {
}

Spectrum SchlickScatter::Evaluate(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW, float *reversePdfW) const {
	Spectrum r = volume->SigmaS(hitPoint);
	const Spectrum sigmaA = volume->SigmaA(hitPoint);
	for (u_int i = 0; i < COLOR_SAMPLES; ++i) {
		if (r.c[i] > 0.f)
			r.c[i] /= r.c[i] + sigmaA.c[i];
		else
			r.c[i] = 1.f;
	}

	const Spectrum gValue = g->GetSpectrumValue(hitPoint).Clamp(-1.f, 1.f);
	const Spectrum k = gValue * (Spectrum(1.55f) - .55f * gValue * gValue);

	*event = DIFFUSE | REFLECT;

	const float dotEyeLight = Dot(localEyeDir, localLightDir);
	const float kFilter = k.Filter();
	// 1+k*cos instead of 1-k*cos because localEyeDir is reversed compared to the
	// standard phase function definition
	const float compcostFilter = 1.f + kFilter * dotEyeLight;
	const float pdf = (1.f - kFilter * kFilter) / (compcostFilter * compcostFilter * (4.f * M_PI));

	if (directPdfW)
		*directPdfW = pdf;

	if (reversePdfW)
		*reversePdfW = pdf;

	// 1+k*cos instead of 1-k*cos because localEyeDir is reversed compared to the
	// standard phase function definition
	const Spectrum compcostValue = Spectrum(1.f) + k * dotEyeLight;

	return r * (Spectrum(1.f) - k * k) / (compcostValue * compcostValue * (4.f * M_PI));
}

Spectrum SchlickScatter::Sample(const HitPoint &hitPoint,
		const Vector &localFixedDir, Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const {
	if (!(requestedEvent & (DIFFUSE | REFLECT)))
		return Spectrum();

	const Spectrum gValue = g->GetSpectrumValue(hitPoint).Clamp(-1.f, 1.f);
	const Spectrum k = gValue * (Spectrum(1.55f) - .55f * gValue * gValue);
	const float gFilter = k.Filter();

	// Add a - because localEyeDir is reversed compared to the standard phase
	// function definition
	const float cost = -(2.f * u0 + gFilter - 1.f) / (2.f * gFilter * u0 - gFilter + 1.f);

	Vector x, y;
	CoordinateSystem(localFixedDir, &x, &y);
	*localSampledDir = SphericalDirection(sqrtf(Max(0.f, 1.f - cost * cost)), cost,
			2.f * M_PI * u1, x, y, localFixedDir);

	// The - becomes a + because cost has been reversed above
	const float compcost = 1.f + gFilter * cost;
	*pdfW = (1.f - gFilter * gFilter) / (compcost * compcost * (4.f * M_PI));
	if (*pdfW <= 0.f)
		return Spectrum();
	
	*absCosSampledDir = fabsf(localSampledDir->z);
	*event = DIFFUSE | REFLECT;

	Spectrum r = volume->SigmaS(hitPoint);
	const Spectrum sigmaA = volume->SigmaA(hitPoint);
	for (u_int i = 0; i < COLOR_SAMPLES; ++i) {
		if (r.c[i] > 0.f)
			r.c[i] /= r.c[i] + sigmaA.c[i];
		else
			r.c[i] = 1.f;
	}

	return r;
}

void SchlickScatter::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const Spectrum gValue = g->GetSpectrumValue(hitPoint).Clamp(-1.f, 1.f);
	const Spectrum k = gValue * (Spectrum(1.55f) - .55f * gValue * gValue);
	const float gFilter = k.Filter();

	const float dotEyeLight = Dot(localEyeDir, localLightDir);

	// 1+k*cos instead of 1-k*cos because localEyeDir is reversed compared to the
	// standard phase function definition
	const float compcostFilter = 1.f + gFilter * dotEyeLight;
	const float pdf = (1.f - gFilter * gFilter) / (compcostFilter * compcostFilter * (4.f * M_PI));

	if (directPdfW)
		*directPdfW = pdf;

	if (reversePdfW)
		*reversePdfW = pdf;
}
