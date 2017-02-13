/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#include "slg/volumes/clear.h"
#include "slg/bsdf/bsdf.h"

using namespace std;
using namespace luxrays;
using namespace slg;

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
		Vector(0.f, 0.f, 0.f), Vector(0.f, 0.f, 0.f),
		Normal(0.f, 0.f, 0.f), Normal(0.f, 0.f, 0.f),
		1.f,
		0.f, // It doesn't matter here
		Transform(),
		this, this, // It doesn't matter here
		true, true // It doesn't matter here
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
