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

#include <boost/format.hpp>

#include "slg/bsdf/bsdf.h"
#include "slg/lights/mapspherelight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// MapSphereLight
//------------------------------------------------------------------------------

MapSphereLight::MapSphereLight() : imageMap(NULL), func(NULL) {
}

MapSphereLight::~MapSphereLight() {
	delete func;
}

void MapSphereLight::Preprocess() {
	SphereLight::Preprocess();

	delete func;
	func = new SampleableSphericalFunction(new ImageMapSphericalFunction(imageMap));
}

void MapSphereLight::GetPreprocessedData(float *localPosData, float *absolutePosData,
		float *emittedFactorData, const SampleableSphericalFunction **funcData) const {
	SphereLight::GetPreprocessedData(localPosData, absolutePosData, emittedFactorData);

	if (funcData)
		*funcData = func;
}

float MapSphereLight::GetPower(const Scene &scene) const {
	return imageMap->GetSpectrumMeanY() * SphereLight::GetPower(scene);
}

Spectrum MapSphereLight::Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Point &rayOrig, Vector &rayDir, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	// I sample as SphereLight::Emit() instead of func->Sample() because I would
	// have to reject half of the samples due to emitting below the sphere surface
	const Spectrum result = SphereLight::Emit(scene,
			time, u0, u1, u2, u3, passThroughEvent,
			rayOrig, rayDir, emissionPdfW,
			directPdfA, cosThetaAtLight);

	const Vector localFromLight = Normalize(Inverse(lightToWorld) * rayDir);

	return result *	((SphericalFunction *)func)->Evaluate(localFromLight) /
			func->Average();
}

Spectrum MapSphereLight::Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        Vector &shadowRayDir, float &shadowRayDistance, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Spectrum result = SphereLight::Illuminate(scene, bsdf, time, u0, u1,
			passThroughEvent, shadowRayDir, shadowRayDistance, directPdfW,
			emissionPdfW, cosThetaAtLight);

	const Vector localFromLight = Normalize(Inverse(lightToWorld) * (-shadowRayDir));
	const float funcPdf = func->Pdf(localFromLight);
	if (funcPdf == 0.f)
		return Spectrum();

	return result * ((SphericalFunction *)func)->Evaluate(localFromLight) / func->Average();
}

Properties MapSphereLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = SphereLight::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property(prefix + ".type")("mapsphere"));
	const string fileName = useRealFileName ?
		imageMap->GetName() : imgMapCache.GetSequenceFileName(imageMap);
	props.Set(Property(prefix + ".mapfile")(fileName));
	props.Set(imageMap->ToProperties(prefix, false));

	return props;
}
