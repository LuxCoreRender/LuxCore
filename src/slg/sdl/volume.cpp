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

#include <cstddef>

#include "slg/sdl/volume.h"
#include "slg/sdl/bsdf.h"

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

//------------------------------------------------------------------------------
// ClearVolume
//------------------------------------------------------------------------------

ClearVolume::ClearVolume(const Texture *iorTex, const Texture *emiTex,
		const Texture *a) : Volume(iorTex, emiTex) {
	sigmaA = a;
}

Spectrum ClearVolume::SigmaA(const HitPoint &hitPoint) const {
	return sigmaA->GetSpectrumValue(hitPoint).Clamp();
}

Spectrum ClearVolume::SigmaS(const HitPoint &hitPoint) const {
	return Spectrum();
}

float ClearVolume::Scatter(const Ray &ray, const float u,
		const bool scatteredStart, Spectrum *connectionThroughput,
		Spectrum *connectionEmission) const {
	const HitPoint hitPoint =  {
		ray.d,
		ray.o,
		UV(),
		Normal(-ray.d),
		Normal(-ray.d),
		Spectrum(1.f),
		1.f,
		0.f, // It doesn't matter here
		NULL, NULL, // It doesn't matter here
		true // It doesn't matter here
	};
	
	const float distance = ray.maxt - ray.mint;	
	Spectrum transmittance(1.f);

	const Spectrum sigma = SigmaT(hitPoint);
	if (!sigma.Black()) {
		const Spectrum tau = (distance * sigma).Clamp();
		transmittance = Exp(-tau);
	}

	// Apply volume transmittance
	*connectionThroughput *= transmittance;

	// Apply volume emission
	if (volumeEmissionTex)
		*connectionEmission += *connectionThroughput * distance * volumeEmissionTex->GetSpectrumValue(hitPoint).Clamp();
	
	return -1.f;
}

Spectrum ClearVolume::Evaluate(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW, float *reversePdfW) const {
	throw runtime_error("Internal error: called ClearVolume::Evaluate()");
}

Spectrum ClearVolume::Sample(const HitPoint &hitPoint,
		const Vector &localFixedDir, Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const {
	throw runtime_error("Internal error: called ClearVolume::Sample()");
}

void ClearVolume::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	throw runtime_error("Internal error: called ClearVolume::Pdf()");
}

void ClearVolume::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	sigmaA->AddReferencedTextures(referencedTexs);
}

void ClearVolume::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (sigmaA == oldTex)
		sigmaA = newTex;
}

Properties ClearVolume::ToProperties() const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.volumes." + name + ".type")("clear"));
	props.Set(Property("scene.volumes." + name + ".absorption")(sigmaA->GetName()));
	props.Set(Volume::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// HomogeneousVolume
//------------------------------------------------------------------------------

HomogeneousVolume::HomogeneousVolume(const Texture *iorTex, const Texture *emiTex,
		const Texture *a, const Texture *s, const Texture *g, const bool multiScat) :
		Volume(iorTex, emiTex), schlickScatter(this, g), multiScattering(multiScat) {
	sigmaA = a;
	sigmaS = s;
}

Spectrum HomogeneousVolume::SigmaA(const HitPoint &hitPoint) const {
	return sigmaA->GetSpectrumValue(hitPoint).Clamp();
}

Spectrum HomogeneousVolume::SigmaS(const HitPoint &hitPoint) const {
	return sigmaS->GetSpectrumValue(hitPoint).Clamp();
}

float HomogeneousVolume::Scatter(const Ray &ray, const float u,
		const bool scatteredStart, Spectrum *connectionThroughput,
		Spectrum *connectionEmission) const {
	const float maxDistance = ray.maxt - ray.mint;

	// Check if I have to support multi-scattering
	const bool scatterAllowed = (!scatteredStart || multiScattering);

	bool scatter = false;
	float distance = maxDistance;
	const float k = sigmaS->Filter();
	if (scatterAllowed && (k > 0.f)) {
		// Determine scattering distance
		const float scatterDistance = -logf(1.f - u) / k;

		scatter = scatterAllowed && (scatterDistance < maxDistance);
		distance = scatter ? scatterDistance : maxDistance;

		// Note: distance can not be infinity because otherwise there would
		// have been a scatter event before.
		const float pdf = expf(-distance * k) * (scatter ? k : 1.f);
		*connectionThroughput /= pdf;
	}

	const HitPoint hitPoint =  {
		ray.d,
		ray.o,
		UV(),
		Normal(-ray.d),
		Normal(-ray.d),
		Spectrum(1.f),
		1.f,
		0.f, // It doesn't matter here
		NULL, NULL, // It doesn't matter here
		true // It doesn't matter here
	};

	const Spectrum sigmaT = SigmaT(hitPoint);
	Spectrum transmittance(1.f);
	if (!sigmaT.Black()) {
		const Spectrum tau = (distance * sigmaT).Clamp();
		transmittance = Exp(-tau);

		// Apply volume transmittance
		*connectionThroughput *= transmittance * (scatter ? sigmaT : Spectrum(1.f));
	}

	// Apply volume emission
	if (volumeEmissionTex)
		*connectionEmission += *connectionThroughput * distance * volumeEmissionTex->GetSpectrumValue(hitPoint).Clamp();

	return scatter ? (ray.mint + distance) : -1.f;
}

Spectrum HomogeneousVolume::Evaluate(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW, float *reversePdfW) const {
	return schlickScatter.Evaluate(hitPoint, localLightDir, localEyeDir, event, directPdfW, reversePdfW);
}

Spectrum HomogeneousVolume::Sample(const HitPoint &hitPoint,
		const Vector &localFixedDir, Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const {
	return schlickScatter.Sample(hitPoint, localFixedDir, localSampledDir,
			u0, u1, passThroughEvent, pdfW, absCosSampledDir, event, requestedEvent);
}

void HomogeneousVolume::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	schlickScatter.Pdf(hitPoint, localLightDir, localEyeDir, directPdfW, reversePdfW);
}

void HomogeneousVolume::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	sigmaA->AddReferencedTextures(referencedTexs);
	sigmaS->AddReferencedTextures(referencedTexs);
	schlickScatter.g->AddReferencedTextures(referencedTexs);
}

void HomogeneousVolume::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (sigmaA == oldTex)
		sigmaA = newTex;
	if (sigmaS == oldTex)
		sigmaS = newTex;
	if (schlickScatter.g == oldTex)
		schlickScatter.g = newTex;
}

Properties HomogeneousVolume::ToProperties() const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.volumes." + name + ".type")("homogeneous"));
	props.Set(Property("scene.volumes." + name + ".absorption")(sigmaA->GetName()));
	props.Set(Property("scene.volumes." + name + ".scattering")(sigmaS->GetName()));
	props.Set(Property("scene.volumes." + name + ".asymmetry")(schlickScatter.g->GetName()));
	props.Set(Property("scene.volumes." + name + ".multiscattering")(multiScattering));
	props.Set(Volume::ToProperties());

	return props;
}

//------------------------------------------------------------------------------
// HeterogeneousVolume
//------------------------------------------------------------------------------

HeterogeneousVolume::HeterogeneousVolume(const Texture *iorTex, const Texture *emiTex,
		const Texture *a, const Texture *s, const Texture *g,
		const float ss, const u_int maxStepC,
		const bool multiScat) : Volume(iorTex, emiTex),
		schlickScatter(this, g), stepSize(ss), maxStepsCount(maxStepC),
		multiScattering(multiScat) {
	sigmaA = a;
	sigmaS = s;
}

Spectrum HeterogeneousVolume::SigmaA(const HitPoint &hitPoint) const {
	return sigmaA->GetSpectrumValue(hitPoint).Clamp();
}

Spectrum HeterogeneousVolume::SigmaS(const HitPoint &hitPoint) const {
	return sigmaS->GetSpectrumValue(hitPoint).Clamp();
}

float HeterogeneousVolume::Scatter(const Ray &ray, const float initialU,
		const bool scatteredStart, Spectrum *connectionThroughput,
		Spectrum *connectionEmission) const {
	// Compute the number of steps to evaluate the volume
	// Integrates in steps of at most stepSize
	// unless stepSize is too small compared to the total length
	const float rayLen = ray.maxt - ray.mint;

	//--------------------------------------------------------------------------
	// Handle the case when ray.maxt is infinity or a very large number
	//--------------------------------------------------------------------------

	u_int steps;
	float ss;
	if (rayLen == numeric_limits<float>::infinity()) {
		steps = maxStepsCount;
		ss = stepSize;
	} else {
		// Note: Ceil2UInt() of an out of range number is 0
		const float fsteps = rayLen / Max(MachineEpsilon::E(rayLen), stepSize);
		if (fsteps >= maxStepsCount)
			steps = maxStepsCount;
		else
			steps = Ceil2UInt(fsteps);

		ss = rayLen / steps; // Effective step size
	}

	const float totalDistance = ss * steps;

	// Evaluate the scattering at the path origin
	HitPoint hitPoint =  {
		ray.d,
		ray(ray.mint),
		UV(),
		Normal(-ray.d),
		Normal(-ray.d),
		Spectrum(1.f),
		1.f,
		0.f, // It doesn't matter here
		NULL, NULL, // It doesn't matter here
		true // It doesn't matter here
	};

	const bool scatterAllowed = (!scatteredStart || multiScattering);

	//--------------------------------------------------------------------------
	// Find the scattering point if there is one
	//--------------------------------------------------------------------------

	float oldSigmaS = SigmaS(hitPoint).Filter();
	float u = initialU;
	float scatterDistance = totalDistance;
	float t = -1.f;
	float pdf = 1.f;
	for (u_int s = 1; s <= steps; ++s) {
		// Compute the mean scattering over the current step
		hitPoint.p = ray(ray.mint + s * ss);

		// Check if there is a scattering event
		const float newSigmaS = SigmaS(hitPoint).Filter();
		const float halfWaySigmaS = (oldSigmaS + newSigmaS) * .5f;
		oldSigmaS = newSigmaS;

		// Skip the step if no scattering can occur
		if (halfWaySigmaS <= 0.f)
			continue;

		// Determine scattering distance
		const float d = logf(1.f - u) / halfWaySigmaS; // The real distance is ray.mint-d
		const bool scatter = scatterAllowed && (d > (s - 1U) * ss - totalDistance);
		if (!scatter) {
			if (scatterAllowed)
				pdf *= expf(-ss * halfWaySigmaS);

			// Update the random variable to account for
			// the current step
			u -= (1.f - u) * (expf(oldSigmaS * ss) - 1.f);
			continue;
		}

		// The ray is scattered
		scatterDistance = (s - 1U) * ss - d;
		t = ray.mint + scatterDistance;
		pdf *= expf(d * halfWaySigmaS) * oldSigmaS;

		hitPoint.p = ray(t);
		*connectionThroughput *= SigmaT(hitPoint);
		break;
	}

	//--------------------------------------------------------------------------
	// Now I know the distance of the scattering point (if there is one) and
	// I can calculate transmittance and emission
	//--------------------------------------------------------------------------
	
	steps = Ceil2UInt(scatterDistance / Max(MachineEpsilon::E(scatterDistance), stepSize));
	ss = scatterDistance / steps;

	Spectrum tau, emission;
	hitPoint.p = ray(ray.mint);
	Spectrum oldSigmaT = SigmaT(hitPoint);
	for (u_int s = 1; s <= steps; ++s) {
		hitPoint.p = ray(ray.mint + s * ss);

		// Accumulate tau values
		const Spectrum newSigmaT = SigmaT(hitPoint);
		const Spectrum halfWaySigmaT = (oldSigmaT + newSigmaT) * .5f;
		tau += (ss * halfWaySigmaT).Clamp();
		oldSigmaT = newSigmaT;

		// Accumulate volume emission
		if (volumeEmissionTex)
			emission += Exp(-tau) * (ss * volumeEmissionTex->GetSpectrumValue(hitPoint)).Clamp();
	}
	
	// Apply volume transmittance
	const Spectrum transmittance = Exp(-tau);
	*connectionThroughput *= transmittance / pdf;

	// Add volume emission
	if (volumeEmissionTex)
		*connectionEmission += *connectionThroughput * emission;

	return t;
}

Spectrum HeterogeneousVolume::Evaluate(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW, float *reversePdfW) const {
	return schlickScatter.Evaluate(hitPoint, localLightDir, localEyeDir, event, directPdfW, reversePdfW);
}

Spectrum HeterogeneousVolume::Sample(const HitPoint &hitPoint,
		const Vector &localFixedDir, Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const {
	return schlickScatter.Sample(hitPoint, localFixedDir, localSampledDir,
			u0, u1, passThroughEvent, pdfW, absCosSampledDir, event, requestedEvent);
}

void HeterogeneousVolume::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	schlickScatter.Pdf(hitPoint, localLightDir, localEyeDir, directPdfW, reversePdfW);
}

void HeterogeneousVolume::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	sigmaA->AddReferencedTextures(referencedTexs);
	sigmaS->AddReferencedTextures(referencedTexs);
	schlickScatter.g->AddReferencedTextures(referencedTexs);
}

void HeterogeneousVolume::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (sigmaA == oldTex)
		sigmaA = newTex;
	if (sigmaS == oldTex)
		sigmaS = newTex;
	if (schlickScatter.g == oldTex)
		schlickScatter.g = newTex;
}

Properties HeterogeneousVolume::ToProperties() const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.volumes." + name + ".type")("heterogeneous"));
	props.Set(Property("scene.volumes." + name + ".absorption")(sigmaA->GetName()));
	props.Set(Property("scene.volumes." + name + ".scattering")(sigmaS->GetName()));
	props.Set(Property("scene.volumes." + name + ".asymmetry")(schlickScatter.g->GetName()));
	props.Set(Property("scene.volumes." + name + ".multiscattering")(multiScattering));
	props.Set(Property("scene.volumes." + name + ".steps.size")(stepSize));
	props.Set(Property("scene.volumes." + name + ".steps.maxcount")(maxStepsCount));
	props.Set(Volume::ToProperties());

	return props;
}
