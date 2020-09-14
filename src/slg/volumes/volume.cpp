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

#include "slg/volumes/volume.h"
#include "slg/bsdf/bsdf.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Volume
//------------------------------------------------------------------------------

void Volume::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	if (iorTex)
		iorTex->AddReferencedTextures(referencedTexs);
	if (volumeEmissionTex)
		volumeEmissionTex->AddReferencedTextures(referencedTexs);
}

void Volume::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (iorTex == oldTex)
		iorTex = newTex;
	if (volumeEmissionTex == oldTex)
		volumeEmissionTex = newTex;
}

Properties Volume::ToProperties() const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.volumes." + name + ".priority")(priority));
	props.Set(Property("scene.volumes." + name + ".ior")(iorTex->GetSDLValue()));
	if (volumeEmissionTex) {
		props.Set(Property("scene.volumes." + name + ".emission")(volumeEmissionTex->GetSDLValue()));
		props.Set(Property("scene.volumes." + name + ".emission.id")(volumeLightID));
	}
	props.Set(Property("scene.volumes." + name + ".id")(matID));
	
	props.Set(Property("scene.volumes." + name + ".photongi.enable")(isPhotonGIEnabled));

	return props;
}

//------------------------------------------------------------------------------
// SchlickScatter
//------------------------------------------------------------------------------

SchlickScatter::SchlickScatter(const Volume *vol, const Texture *gTex) :
	volume(vol), g(gTex) {
}

Spectrum SchlickScatter::GetColor(const HitPoint &hitPoint) const {
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

Spectrum SchlickScatter::Albedo(const HitPoint &hitPoint) const {
	return GetColor(hitPoint).Clamp(0.f, 1.f);
}

Spectrum SchlickScatter::Evaluate(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW, float *reversePdfW) const {
	const Spectrum gValue = g->GetSpectrumValue(hitPoint).Clamp(-1.f, 1.f);
	const Spectrum k = gValue * (Spectrum(1.55f) - .55f * gValue * gValue);

	*event = DIFFUSE | REFLECT;

	const float dotEyeLight = Dot(localEyeDir, localLightDir);
	const float kFilter = k.Filter();
	// 1+k*cos instead of 1-k*cos because localEyeDir is reversed compared to the
	// standard phase function definition
	const float compcostFilter = 1.f + kFilter * dotEyeLight;
	const float pdf = (1.f - kFilter * kFilter) / (compcostFilter * compcostFilter * (4.f * M_PI));
	if (pdf <= 0.f)
		return Spectrum();

	if (directPdfW)
		*directPdfW = pdf;

	if (reversePdfW)
		*reversePdfW = pdf;

	// 1+k*cos instead of 1-k*cos because localEyeDir is reversed compared to the
	// standard phase function definition
	const Spectrum compcostValue = Spectrum(1.f) + k * dotEyeLight;

	return GetColor(hitPoint) * (Spectrum(1.f) - k * k) / (compcostValue * compcostValue * (4.f * M_PI));
}

Spectrum SchlickScatter::Sample(const HitPoint &hitPoint,
		const Vector &localFixedDir, Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, BSDFEvent *event) const {
	const Spectrum gValue = g->GetSpectrumValue(hitPoint).Clamp(-1.f, 1.f);
	const Spectrum k = gValue * (Spectrum(1.55f) - .55f * gValue * gValue);
	const float kFilter = k.Filter();

	// Add a - because localEyeDir is reversed compared to the standard phase
	// function definition
	const float cost = -(2.f * u0 + kFilter - 1.f) / (2.f * kFilter * u0 - kFilter + 1.f);

	Vector x, y;
	CoordinateSystem(localFixedDir, &x, &y);
	*localSampledDir = SphericalDirection(sqrtf(Max(0.f, 1.f - cost * cost)), cost,
			2.f * M_PI * u1, x, y, localFixedDir);

	// The - becomes a + because cost has been reversed above
	const float compcost = 1.f + kFilter * cost;
	*pdfW = (1.f - kFilter * kFilter) / (compcost * compcost * (4.f * M_PI));
	if (*pdfW <= 0.f)
		return Spectrum();
	
	*event = DIFFUSE | REFLECT;

	return GetColor(hitPoint);
}

void SchlickScatter::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const Spectrum gValue = g->GetSpectrumValue(hitPoint).Clamp(-1.f, 1.f);
	const Spectrum k = gValue * (Spectrum(1.55f) - .55f * gValue * gValue);
	const float kFilter = k.Filter();

	const float dotEyeLight = Dot(localEyeDir, localLightDir);

	// 1+k*cos instead of 1-k*cos because localEyeDir is reversed compared to the
	// standard phase function definition
	const float compcostFilter = 1.f + kFilter * dotEyeLight;
	const float pdf = (1.f - kFilter * kFilter) / (compcostFilter * compcostFilter * (4.f * M_PI));

	if (directPdfW)
		*directPdfW = pdf;

	if (reversePdfW)
		*reversePdfW = pdf;
}
