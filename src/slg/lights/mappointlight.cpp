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

#include "slg/lights/mappointlight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// MapPointLight
//------------------------------------------------------------------------------

MapPointLight::MapPointLight() : imageMap(NULL), func(NULL) {
}

MapPointLight::~MapPointLight() {
	delete func;
}

void MapPointLight::Preprocess() {
	PointLight::Preprocess();

	delete func;
	func = new SampleableSphericalFunction(new ImageMapSphericalFunction(imageMap));
}

void MapPointLight::GetPreprocessedData(float *localPosData, float *absolutePosData,
		float *emittedFactorData, const SampleableSphericalFunction **funcData) const {
	PointLight::GetPreprocessedData(localPosData, absolutePosData, emittedFactorData);

	if (funcData)
		*funcData = func;
}

float MapPointLight::GetPower(const Scene &scene) const {
	return imageMap->GetSpectrumMeanY() * PointLight::GetPower(scene);
}

Spectrum MapPointLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	*orig = absolutePos;

	Vector localFromLight;
	func->Sample(u0, u1, &localFromLight, emissionPdfW);
	if (*emissionPdfW == 0.f)
		return Spectrum();

	*dir = Normalize(lightToWorld * localFromLight);

	if (directPdfA)
		*directPdfA = 1.f;
	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	return emittedFactor * ((SphericalFunction *)func)->Evaluate(localFromLight) /
			(4.f * M_PI * func->Average());
}

Spectrum MapPointLight::Illuminate(const Scene &scene,
		const Point &p, const Normal &n,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Vector localFromLight = Normalize(Inverse(lightToWorld) * p - localPos);
	const float funcPdf = func->Pdf(localFromLight);
	if (funcPdf == 0.f)
		return Spectrum();

	const Vector toLight(absolutePos - p);
	const float centerDistanceSquared = toLight.LengthSquared();
	*distance = sqrtf(centerDistanceSquared);
	*dir = toLight / *distance;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	*directPdfW = centerDistanceSquared;

	if (emissionPdfW)
		*emissionPdfW = funcPdf;

	return emittedFactor * ((SphericalFunction *)func)->Evaluate(localFromLight) /
			(4.f * M_PI * func->Average());
}

Properties MapPointLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = PointLight::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property(prefix + ".type")("mappoint"));
	const string fileName = useRealFileName ?
		imageMap->GetName() : imgMapCache.GetSequenceFileName(imageMap);
	props.Set(Property(prefix + ".mapfile")(fileName));
	props.Set(imageMap->ToProperties(prefix, false));

	return props;
}
