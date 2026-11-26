/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
#include "luxrays/usings.h"
#include "slg/bsdf/bsdf.h"
#include "slg/textures/texture.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// HomogeneousVolume
//------------------------------------------------------------------------------

HomogeneousVolume::HomogeneousVolume(
	TextureConstRef iorTex,
	TextureConstPtr emiTex,
	TextureConstRef a, TextureConstRef s, TextureConstRef g,
	const bool multiScat
) :
	Volume(iorTex, emiTex),
	schlickScatter(*this, g),
	multiScattering(multiScat),
	sigmaA(a),
	sigmaS(s)
{}

float HomogeneousVolume::Scatter(const float u,
		const bool scatterAllowed, const float segmentLength,
		const Spectrum &sigmaA, const Spectrum &sigmaS, const Spectrum &emission,
		Spectrum &segmentTransmittance, Spectrum &segmentEmission) {
	// This code must work also with segmentLength = INFINITY

	bool scatter = false;
	segmentTransmittance = Spectrum(1.f);
	segmentEmission = Spectrum(0.f);

	//--------------------------------------------------------------------------
	// Check if there is a scattering event
	//--------------------------------------------------------------------------

	float scatterDistance = segmentLength;
	const float sigmaSValue = sigmaS.Filter();
	if (scatterAllowed && (sigmaSValue > 0.f)) {
		// Determine scattering distance
		const float proposedScatterDistance = -logf(1.f - u) / sigmaSValue;

		scatter = (proposedScatterDistance < segmentLength);
		scatterDistance = scatter ? proposedScatterDistance : segmentLength;

		// Note: scatterDistance can not be infinity because otherwise there would
		// have been a scatter event before.
		const float tau = scatterDistance * sigmaSValue;
		const float pdf = expf(-tau) * (scatter ? sigmaSValue : 1.f);
		segmentTransmittance /= pdf;
	}

	//--------------------------------------------------------------------------
	// Volume transmittance
	//--------------------------------------------------------------------------
	
	const Spectrum sigmaT = sigmaA + sigmaS;
	if (!sigmaT.Black()) {
		if (isinf(scatterDistance)) {
			// This avoid NaN in case scatterDistance is inf
			segmentTransmittance = Spectrum(0.f);
		} else {
			const Spectrum tau = scatterDistance * sigmaT;
			segmentTransmittance *= Exp(-tau) * (scatter ? sigmaT : Spectrum(1.f));
		}
	}

	//--------------------------------------------------------------------------
	// Volume emission
	//--------------------------------------------------------------------------

	segmentEmission += segmentTransmittance * scatterDistance * emission;

	return scatter ? scatterDistance : -1.f;
}

Spectrum HomogeneousVolume::SigmaA(const HitPoint &hitPoint) const {
	return GetSigmaA().GetSpectrumValue(hitPoint).Clamp();
}

Spectrum HomogeneousVolume::SigmaS(const HitPoint &hitPoint) const {
	return GetSigmaS().GetSpectrumValue(hitPoint).Clamp();
}

float HomogeneousVolume::Scatter(const Ray &ray, const float u,
		const bool scatteredStart, Spectrum *connectionThroughput,
		Spectrum *connectionEmission) const {
	const float segmentLength = ray.maxt - ray.mint;

	// Check if I have to support multi-scattering
	const bool scatterAllowed = (!scatteredStart || multiScattering);

	// Point where to evaluate the volume
	HitPoint hitPoint;
	hitPoint.Init();
	hitPoint.fixedDir = ray.d;
	hitPoint.p = ray.o;
	hitPoint.geometryN = hitPoint.interpolatedN = hitPoint.shadeN = Normal(-ray.d);
	hitPoint.passThroughEvent = u;

	const Spectrum sigmaA = SigmaA(hitPoint);
	const Spectrum sigmaS = SigmaS(hitPoint);
	const Spectrum emission = Emission(hitPoint);

	Spectrum segmentTransmittance, segmentEmission;
	const float scatterDistance = HomogeneousVolume::Scatter(u, scatterAllowed,
			segmentLength, sigmaA, sigmaS, emission,
			segmentTransmittance, segmentEmission);

	// I need to update first connectionEmission and than connectionThroughput
	*connectionEmission += *connectionThroughput * emission;
	*connectionThroughput *= segmentTransmittance;

	return (scatterDistance == -1.f) ? -1.f : (ray.mint + scatterDistance);
}

Spectrum HomogeneousVolume::Albedo(const HitPoint &hitPoint) const {
	return schlickScatter.Albedo(hitPoint);
}

Spectrum HomogeneousVolume::Evaluate(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW, float *reversePdfW) const {
	return schlickScatter.Evaluate(hitPoint, localLightDir, localEyeDir, event, directPdfW, reversePdfW);
}

Spectrum HomogeneousVolume::Sample(const HitPoint &hitPoint,
		const Vector &localFixedDir, Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, BSDFEvent *event) const {
	return schlickScatter.Sample(hitPoint, localFixedDir, localSampledDir,
			u0, u1, passThroughEvent, pdfW, event);
}

void HomogeneousVolume::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	schlickScatter.Pdf(hitPoint, localLightDir, localEyeDir, directPdfW, reversePdfW);
}

void HomogeneousVolume::AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexs) const {
	Volume::AddReferencedTextures(referencedTexs);

	GetSigmaA().AddReferencedTextures(referencedTexs);
	GetSigmaS().AddReferencedTextures(referencedTexs);
	schlickScatter.GetG().AddReferencedTextures(referencedTexs);
}

void HomogeneousVolume::UpdateTextureReferences(
	TextureConstRef oldTex, TextureRef newTex
) {
	Volume::UpdateTextureReferences(oldTex, newTex);

	updtex(sigmaA, oldTex, newTex);
	updtex(sigmaS, oldTex, newTex);
	if (&schlickScatter.GetG() == &oldTex)
		schlickScatter.SetG(newTex);
}

PropertiesUPtr HomogeneousVolume::ToProperties() const {
	PropertiesUPtr props = std::make_unique<Properties>();

	const string name = GetName();
	props->Set(Property("scene.volumes." + name + ".type")("homogeneous"));
	props->Set(Property("scene.volumes." + name + ".absorption")(GetSigmaA().GetSDLValue()));
	props->Set(Property("scene.volumes." + name + ".scattering")(GetSigmaS().GetSDLValue()));
	props->Set(Property("scene.volumes." + name + ".asymmetry")(schlickScatter.GetG().GetSDLValue()));
	props->Set(Property("scene.volumes." + name + ".multiscattering")(multiScattering));
	props->Set(Volume::ToProperties());

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
