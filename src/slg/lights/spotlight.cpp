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

#include "slg/bsdf/bsdf.h"
#include "slg/lights/spotlight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SpotLight
//------------------------------------------------------------------------------

SpotLight::SpotLight() :
	color(1.f), power(0.f), efficency(0.f), emittedPowerNormalize(true),
	localPos(Point()), localTarget(Point(0.f, 0.f, 1.f)),
	coneAngle(30.f), coneDeltaAngle(5.f) {
}

SpotLight::~SpotLight() {
}

void SpotLight::Preprocess() {
	NotIntersectableLightSource::Preprocess();

	cosTotalWidth = cosf(Radians(coneAngle));
	cosFalloffStart = cosf(Radians(coneAngle - coneDeltaAngle));

	const float normalizeFactor = emittedPowerNormalize ? (1.f / Max(color.Y(), 0.f)) : 1.f;

	emittedFactor = temperatureScale * gain * color * (power * efficency * normalizeFactor /
			(2.f * M_PI * (1.f - .5f * (cosFalloffStart + cosTotalWidth))));
	if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN())
		emittedFactor = temperatureScale * gain * color;

	absolutePos = lightToWorld * localPos;

	const Vector dir = Normalize(localTarget - localPos);
	Vector du, dv;
	CoordinateSystem(dir, &du, &dv);
	const Transform dirToZ(Matrix4x4(
			du.x, du.y, du.z, 0.f,
			dv.x, dv.y, dv.z, 0.f,
			dir.x, dir.y, dir.z, 0.f,
			0.f, 0.f, 0.f, 1.f));
	alignedLight2World = lightToWorld *
			Translate(Vector(localPos.x, localPos.y, localPos.z)) /
			dirToZ;
}

void SpotLight::GetPreprocessedData(float *emittedFactorData, float *absolutePosData,
		float *cosTotalWidthData, float *cosFalloffStartData,
		const luxrays::Transform **alignedLight2WorldData) const {
	if (emittedFactorData) {
		emittedFactorData[0] = emittedFactor.c[0];
		emittedFactorData[1] = emittedFactor.c[1];
		emittedFactorData[2] = emittedFactor.c[2];
	}

	if (absolutePosData) {
		absolutePosData[0] = absolutePos.x;
		absolutePosData[1] = absolutePos.y;
		absolutePosData[2] = absolutePos.z;
	}

	if (cosTotalWidthData)
		*cosTotalWidthData = cosTotalWidth;
	if (cosFalloffStartData)
		*cosFalloffStartData = cosFalloffStart;

	if (alignedLight2WorldData)
		*alignedLight2WorldData = &alignedLight2World;
}

float SpotLight::GetPower(const Scene &scene) const {
	return emittedFactor.Y() * 2.f * M_PI * (1.f - .5f * (cosFalloffStart + cosTotalWidth));
}

static float LocalFalloff(const Vector &w, const float cosTotalWidth, const float cosFalloffStart) {
	if (CosTheta(w) < cosTotalWidth)
		return 0.f;
 	if (CosTheta(w) > cosFalloffStart)
		return 1.f;

	// Compute falloff inside spotlight cone
	const float delta = (CosTheta(w) - cosTotalWidth) / (cosFalloffStart - cosTotalWidth);
	return powf(delta, 4);
}

Spectrum SpotLight::Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Ray &ray, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	const Point &rayOrig = absolutePos;
	const Vector localFromLight = UniformSampleCone(u0, u1, cosTotalWidth);
	const Vector rayDir = Normalize(alignedLight2World * localFromLight);
	emissionPdfW = UniformConePdf(cosTotalWidth);

	if (directPdfA)
		*directPdfA = 1.f;
	if (cosThetaAtLight)
		*cosThetaAtLight = CosTheta(localFromLight);

	ray.Update(rayOrig, rayDir, time);

	return emittedFactor * (LocalFalloff(localFromLight, cosTotalWidth, cosFalloffStart) / fabsf(CosTheta(localFromLight)));
}

Spectrum SpotLight::Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        Ray &shadowRay, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Point shadowRayOrig = bsdf.GetRayOrigin(absolutePos - bsdf.hitPoint.p);
	const Vector toLight(absolutePos - shadowRayOrig);
	const float shadowRayDistanceSquared = toLight.LengthSquared();
	const float shadowRayDistance = sqrtf(shadowRayDistanceSquared);
	const Vector shadowRayDir = toLight / shadowRayDistance;

	const Vector localFromLight = Normalize(Inverse(alignedLight2World) * (-shadowRayDir));
	const float falloff = LocalFalloff(localFromLight, cosTotalWidth, cosFalloffStart);
	if (falloff == 0.f)
		return Spectrum();

	if (cosThetaAtLight)
		*cosThetaAtLight = CosTheta(localFromLight);

	directPdfW = shadowRayDistanceSquared;

	if (emissionPdfW)
		*emissionPdfW = UniformConePdf(cosTotalWidth);

	shadowRay = Ray(shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return emittedFactor * (falloff / fabsf(CosTheta(localFromLight)));
}

bool SpotLight::IsAlwaysInShadow(const Scene &scene,
			const luxrays::Point &p, const luxrays::Normal &n) const {
	const Vector toLight(absolutePos - p);
	const float distance = toLight.Length();
	const Vector dir = toLight / distance;

	const Vector localFromLight = Normalize(Inverse(alignedLight2World) * (-dir));
	const float falloff = LocalFalloff(localFromLight, cosTotalWidth, cosFalloffStart);

	return (falloff == 0.f);
}

Properties SpotLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property(prefix + ".type")("spot"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".power")(power));
	props.Set(Property(prefix + ".normalizebycolor")(emittedPowerNormalize));
	props.Set(Property(prefix + ".efficency")(efficency));
	props.Set(Property(prefix + ".position")(localPos));
	props.Set(Property(prefix + ".target")(localTarget));
	props.Set(Property(prefix + ".coneangle")(coneAngle));
	props.Set(Property(prefix + ".conedeltaangle")(coneDeltaAngle));

	return props;
}
