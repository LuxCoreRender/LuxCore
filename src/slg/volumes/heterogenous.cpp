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

#include "slg/volumes/heterogenous.h"
#include "slg/bsdf/bsdf.h"

using namespace std;
using namespace luxrays;
using namespace slg;

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

	// Handle the case when ray.maxt is infinity or a very large number
	const float totalDistance = Min(stepSize * maxStepsCount, rayLen);
	const float currentStepSize = Min(stepSize, rayLen);

	// Evaluate the scattering at the path origin
	HitPoint hitPoint =  {
		ray.d,
		ray(ray.mint),
		UV(),
		Normal(-ray.d),
		Normal(-ray.d),
		Spectrum(1.f),
		Vector(0.f, 0.f, 0.f), Vector(0.f, 0.f, 0.f),
		Normal(0.f, 0.f, 0.f), Normal(0.f, 0.f, 0.f),
		1.f,
		0.f, // It doesn't matter here
		this, this, // It doesn't matter here
		true, true // It doesn't matter here
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
	float currentOffset = currentStepSize;
	for (u_int s = 1; (s <= maxStepsCount) && (currentOffset <= rayLen); ++s) {
		// Compute the mean scattering over the current step
		hitPoint.p = ray(ray.mint + currentOffset);

		// Check if there is a scattering event
		const float newSigmaS = SigmaS(hitPoint).Filter();
		const float halfWaySigmaS = (oldSigmaS + newSigmaS) * .5f;
		oldSigmaS = newSigmaS;

		// Skip the step if no scattering can occur
		if (halfWaySigmaS <= 0.f)
			continue;

		// Determine scattering distance
		const float d = logf(1.f - u) / halfWaySigmaS; // The real distance is ray.mint-d
		const bool scatter = scatterAllowed && (d > (s - 1U) * currentStepSize - totalDistance);
		if (scatter) {
			// The ray is scattered
			scatterDistance = (s - 1U) * currentStepSize - d;
			t = ray.mint + scatterDistance;
			pdf *= expf(d * halfWaySigmaS) * halfWaySigmaS;

			hitPoint.p = ray(t);
			*connectionThroughput *= SigmaT(hitPoint);
			break;
		}

		if (scatterAllowed)
			pdf *= expf(-currentStepSize * halfWaySigmaS);

		// Update the random variable to account for
		// the current step
		u -= (1.f - u) * (expf(oldSigmaS * currentStepSize) - 1.f);
		
		currentOffset += currentStepSize;
	}

	//--------------------------------------------------------------------------
	// Now I know the distance of the scattering point (if there is one) and
	// I can calculate transmittance and emission
	//--------------------------------------------------------------------------
	
//	steps = Ceil2UInt(scatterDistance / Max(MachineEpsilon::E(scatterDistance), stepSize));
//	ss = scatterDistance / steps;
//
//	Spectrum tau, emission;
//	hitPoint.p = ray(ray.mint);
//	Spectrum oldSigmaT = SigmaT(hitPoint);
//	for (u_int s = 1; s <= steps; ++s) {
//		hitPoint.p = ray(ray.mint + s * ss);
//
//		// Accumulate tau values
//		const Spectrum newSigmaT = SigmaT(hitPoint);
//		const Spectrum halfWaySigmaT = (oldSigmaT + newSigmaT) * .5f;
//		tau += (ss * halfWaySigmaT).Clamp();
//		oldSigmaT = newSigmaT;
//
//		// Accumulate volume emission
//		if (volumeEmissionTex)
//			emission += Exp(-tau) * (ss * volumeEmissionTex->GetSpectrumValue(hitPoint)).Clamp();
//	}
//	
//	// Apply volume transmittance
//	const Spectrum transmittance = Exp(-tau);
//	*connectionThroughput *= transmittance / pdf;
//
//	// Add volume emission
//	if (volumeEmissionTex)
//		*connectionEmission += *connectionThroughput * emission;

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
