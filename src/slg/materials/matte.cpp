/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "slg/materials/matte.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

Spectrum MatteMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * INV_PI);

	*event = DIFFUSE | REFLECT;
	return Kd->GetSpectrumValue(hitPoint).Clamp() * (INV_PI * fabsf(localLightDir.z));
}

Spectrum MatteMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, float *absCosSampledDir, BSDFEvent *event,
	const BSDFEvent requestedEvent) const {
	if (!(requestedEvent & (DIFFUSE | REFLECT)) ||
			(fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	*localSampledDir = Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, pdfW);

	*absCosSampledDir = fabsf(localSampledDir->z);
	if (*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*event = DIFFUSE | REFLECT;
	if (hitPoint.fromLight)
		return Kd->GetSpectrumValue(hitPoint).Clamp() * fabsf(localFixedDir.z / localSampledDir->z);
	else
		return Kd->GetSpectrumValue(hitPoint).Clamp();
}

void MatteMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * INV_PI);
}

void MatteMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kd->AddReferencedTextures(referencedTexs);
}

void MatteMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kd == oldTex)
		Kd = newTex;
}

Properties MatteMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("matte"));
	props.Set(Property("scene.materials." + name + ".kd")(Kd->GetName()));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}
