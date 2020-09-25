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

#include "slg/materials/null.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Null material
//------------------------------------------------------------------------------

NullMaterial::NullMaterial(const Texture *frontTransp, const Texture *backTransp) :
		Material(frontTransp, backTransp, NULL, NULL) {
}

Spectrum NullMaterial::Albedo(const HitPoint &hitPoint) const {
	return Spectrum();
}

Spectrum NullMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
		return Spectrum();
}

Spectrum NullMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, BSDFEvent *event) const {
	*localSampledDir = -localFixedDir;

	*pdfW = 1.f;
	*event = SPECULAR | TRANSMIT;
	return Spectrum(1.f);
}

void NullMaterial::UpdateAvgPassThroughTransparency() {
	avgPassThroughTransparency = 1.f;
}

Spectrum NullMaterial::GetPassThroughTransparency(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent,
		const bool backTracing) const {
	const Texture *transparencyTex = (hitPoint.intoObject != backTracing) ? frontTransparencyTex : backTransparencyTex;

	if (transparencyTex) {
		const Spectrum blendColor = transparencyTex->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
		if (blendColor.Black()) {
			// It doesn't make any sense to have a solid NULL material
			return Spectrum(.0001f);
		} else
			return blendColor;
	} else
		return Spectrum(1.f);
}

Properties NullMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("null"));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}
