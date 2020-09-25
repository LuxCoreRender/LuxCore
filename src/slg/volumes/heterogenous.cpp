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

#include <cstddef>

#include "slg/volumes/homogenous.h"
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

float HeterogeneousVolume::Scatter(const Ray &ray, const float u,
		const bool scatteredStart, Spectrum *connectionThroughput,
		Spectrum *connectionEmission) const {
	// I need a sequence of pseudo-random numbers starting form a floating point
	// pseudo-random number
	TauswortheRandomGenerator rng(u);

	// Compute the number of steps to evaluate the volume
	const float segmentLength = ray.maxt - ray.mint;

	// Handle the case when segmentLength is infinity or a very large number
	//
	// Note: the old code"Min(maxStepsCount, Ceil2UInt(segmentLength / stepSize))"
	// can overflow for large values of segmentLength so I have to use
	// "Ceil2UInt(Min((float)maxStepsCount, segmentLength / stepSize))"
	const u_int steps = Ceil2UInt(Min((float)maxStepsCount, segmentLength / stepSize));

	const float currentStepSize = Min(segmentLength / steps, maxStepsCount * stepSize);

	// Check if I have to support multi-scattering
	const bool scatterAllowed = (!scatteredStart || multiScattering);

	// Point where to evaluate the volume
	HitPoint hitPoint;
	hitPoint.Init();
	hitPoint.fixedDir = ray.d;
	hitPoint.p = ray.o;
	hitPoint.geometryN = hitPoint.interpolatedN = hitPoint.shadeN = Normal(-ray.d);
	hitPoint.passThroughEvent = u;

	for (u_int s = 0; s < steps; ++s) {
		// Compute the scattering over the current step
		const float evaluationPoint = (s + rng.floatValue()) * currentStepSize;

		hitPoint.p = ray(ray.mint + evaluationPoint);

		// Volume segment values
		const Spectrum sigmaA = SigmaA(hitPoint);
		const Spectrum sigmaS = SigmaS(hitPoint);
		const Spectrum emission = Emission(hitPoint);
		
		// Evaluate the current segment like if it was an homogenous volume
		//
		// This could be optimized by inlining the code and exploiting
		// exp(a) * exp(b) = exp(a + b) in order to evaluate a single exp() at
		// the end instead of one each step.
		// However the code would be far less simple and readable.
		Spectrum segmentTransmittance, segmentEmission;
		const float scatterDistance = HomogeneousVolume::Scatter(rng.floatValue(), scatterAllowed,
				currentStepSize, sigmaA, sigmaS, emission,
				segmentTransmittance, segmentEmission);

		// I need to update first connectionEmission and than connectionThroughput
		*connectionEmission += *connectionThroughput * emission;
		*connectionThroughput *= segmentTransmittance;

		if (scatterDistance >= 0.f)
			return ray.mint + s * currentStepSize + scatterDistance;
	}

	return -1.f;
}

Spectrum HeterogeneousVolume::Albedo(const HitPoint &hitPoint) const {
	return schlickScatter.Albedo(hitPoint);
}

Spectrum HeterogeneousVolume::Evaluate(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW, float *reversePdfW) const {
	return schlickScatter.Evaluate(hitPoint, localLightDir, localEyeDir, event, directPdfW, reversePdfW);
}

Spectrum HeterogeneousVolume::Sample(const HitPoint &hitPoint,
		const Vector &localFixedDir, Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, BSDFEvent *event) const {
	return schlickScatter.Sample(hitPoint, localFixedDir, localSampledDir,
			u0, u1, passThroughEvent, pdfW, event);
}

void HeterogeneousVolume::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	schlickScatter.Pdf(hitPoint, localLightDir, localEyeDir, directPdfW, reversePdfW);
}

void HeterogeneousVolume::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Volume::AddReferencedTextures(referencedTexs);

	sigmaA->AddReferencedTextures(referencedTexs);
	sigmaS->AddReferencedTextures(referencedTexs);
	schlickScatter.g->AddReferencedTextures(referencedTexs);
}

void HeterogeneousVolume::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Volume::UpdateTextureReferences(oldTex, newTex);

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
	props.Set(Property("scene.volumes." + name + ".absorption")(sigmaA->GetSDLValue()));
	props.Set(Property("scene.volumes." + name + ".scattering")(sigmaS->GetSDLValue()));
	props.Set(Property("scene.volumes." + name + ".asymmetry")(schlickScatter.g->GetSDLValue()));
	props.Set(Property("scene.volumes." + name + ".multiscattering")(multiScattering));
	props.Set(Property("scene.volumes." + name + ".steps.size")(stepSize));
	props.Set(Property("scene.volumes." + name + ".steps.maxcount")(maxStepsCount));
	props.Set(Volume::ToProperties());

	return props;
}
