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

using namespace std;
using namespace luxrays;
using namespace slg;

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
	// DEFAULT_EPSILON_STATIC is there to avoid a division by 0
	r /= r + volume->SigmaA(hitPoint) + Spectrum(DEFAULT_EPSILON_STATIC);

	const Spectrum gValue = g->GetSpectrumValue(hitPoint).Clamp(-1.f, 1.f);
	const Spectrum k = gValue * (Spectrum(1.55f) - .55f * gValue * gValue);

	*event = DIFFUSE | REFLECT;

	const float dotEyeLight = Dot(localEyeDir, localLightDir);

	const float gFilter = k.Filter();
	// 1+k*cos instead of 1-k*cos because localEyeDir is reversed compared to the
	// standard phase function definition
	const float compcostFilter = 1.f + gFilter * dotEyeLight;
	const float pdf = (1.f - gFilter * gFilter) / (compcostFilter * compcostFilter * (4.f * M_PI));

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
	// DEFAULT_EPSILON_STATIC is there to avoid a division by 0
	r /= r + volume->SigmaA(hitPoint) + Spectrum(DEFAULT_EPSILON_STATIC);

	return r;
}

void SchlickScatter::Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
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
// HomogeneousVolume
//------------------------------------------------------------------------------

HomogeneousVolume::HomogeneousVolume(const Texture *a, const Texture *s,
		const Texture *g, const bool multiScat) : schlickScatter(this, g), multiScattering(multiScat) {
	sigmaA = a;
	sigmaS = s;
}

Spectrum HomogeneousVolume::SigmaA(const HitPoint &hitPoint) const {
	//return fresnel->Evaluate(sw, dg).SigmaA(sw) + sigmaA->Evaluate(sw, dg).Clamp();
	return sigmaA->GetSpectrumValue(hitPoint).Clamp();
}

Spectrum HomogeneousVolume::SigmaS(const HitPoint &hitPoint) const {
	return sigmaS->GetSpectrumValue(hitPoint);
}

Spectrum HomogeneousVolume::Tau(const Ray &ray, const float maxt) const {
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
		false // It doesn't matter here
	};
	
	const Spectrum sigma = SigmaT(hitPoint);
	if (sigma.Black())
		return Spectrum();
	
	const float rl = maxt - ray.mint;

	return (rl * sigma).Clamp();
}

float HomogeneousVolume::Scatter(const luxrays::Ray &ray, const float u,
		 const bool scatteredPath, luxrays::Spectrum *connectionThroughput) const {
	// Check if I have to support multi-scattering
	if (scatteredPath && !multiScattering)
		return -1.f;

	// Determine scattering distance
	const float k = sigmaS->Y();
	const float d = logf(1.f - u) / k; // The real distance is ray.mint-d
	const bool scatter = (d > ray.mint - ray.maxt);
	if (scatter) {
		// The ray is scattered
		const float t  = ray.mint - d;
		*connectionThroughput *= Exp(-Tau(ray, t));

		return t;
	} else {
		*connectionThroughput *= Exp(-Tau(ray, ray.maxt));

		return -1.f;
	}
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
	props.Set(Material::ToProperties());

	return props;
}
