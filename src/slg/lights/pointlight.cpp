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

#include <math.h>

#include "luxrays/core/epsilon.h"
#include "slg/bsdf/bsdf.h"
#include "slg/lights/pointlight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PointLight
//------------------------------------------------------------------------------

PointLight::PointLight() : localPos(0.f), color(1.f),
		power(0.f), efficency(0.f),
		emittedPowerNormalize(true) {
}

PointLight::~PointLight() {
}

void PointLight::Preprocess() {
	NotIntersectableLightSource::Preprocess();

	const float normalizeFactor = emittedPowerNormalize ? (1.f / Max(color.Y(), 0.f)) : 1.f;

	emittedFactor = temperatureScale * gain * color * (power * efficency * normalizeFactor);

	if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN())
		emittedFactor = temperatureScale * gain * color;

	absolutePos = lightToWorld * localPos;
}

void PointLight::GetPreprocessedData(float *localPosData, float *absolutePosData,
		float *emittedFactorData) const {
	if (localPosData) {
		localPosData[0] = localPos.x;
		localPosData[1] = localPos.y;
		localPosData[2] = localPos.z;
	}

	if (absolutePosData) {
		absolutePosData[0] = absolutePos.x;
		absolutePosData[1] = absolutePos.y;
		absolutePosData[2] = absolutePos.z;
	}

	if (emittedFactorData) {
		emittedFactorData[0] = emittedFactor.c[0];
		emittedFactorData[1] = emittedFactor.c[1];
		emittedFactorData[2] = emittedFactor.c[2];
	}
}

float PointLight::GetPower(const Scene &scene) const {
	return emittedFactor.Y();
}

Spectrum PointLight::Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Ray &ray, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	const Point rayOrig = absolutePos;
	const Vector rayDir = UniformSampleSphere(u0, u1);
	emissionPdfW = 1.f / (4.f * M_PI);

	if (directPdfA)
		*directPdfA = 1.f;
	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	ray.Update(rayOrig, rayDir, time);

	return emittedFactor * (1.f / (4.f * M_PI));
}

Spectrum PointLight::Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        Ray &shadowRay, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Point shadowRayOrig = bsdf.GetRayOrigin(absolutePos - bsdf.hitPoint.p);
	const Vector toLight(absolutePos - shadowRayOrig);
	const float shadowRayDistanceSquared = toLight.LengthSquared();
	const float shadowRayDistance = sqrtf(shadowRayDistanceSquared);

	const Vector shadowRayDir = toLight / shadowRayDistance;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	directPdfW = shadowRayDistanceSquared;

	if (emissionPdfW)
		*emissionPdfW = UniformSpherePdf();

	shadowRay = Ray(shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return emittedFactor * (1.f / (4.f * M_PI));
}

Properties PointLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property(prefix + ".type")("point"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".power")(power));
	props.Set(Property(prefix + ".normalizebycolor")(emittedPowerNormalize));
	props.Set(Property(prefix + ".efficency")(efficency));
	props.Set(Property(prefix + ".position")(localPos));

	return props;
}
