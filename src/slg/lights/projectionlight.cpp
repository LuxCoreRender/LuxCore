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

#include <boost/format.hpp>

#include "slg/bsdf/bsdf.h"
#include "slg/lights/projectionlight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ProjectionLight
//------------------------------------------------------------------------------

ProjectionLight::ProjectionLight() : color(1.f), power(0.f), efficiency(0.f),
		emittedPowerNormalize(true), localPos(), localTarget(0.f, 0.f, 1.f),
		fov(45.f), imageMap(NULL) {
}

ProjectionLight::~ProjectionLight() {
}

void ProjectionLight::Preprocess() {
	NotIntersectableLightSource::Preprocess();

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

	absolutePos = alignedLight2World * Point();
	lightNormal = Normalize(alignedLight2World * Normal(0.f, 0.f, 1.f));

	const float aspect = imageMap ? (imageMap->GetWidth() / (float)imageMap->GetHeight()) : 1.f;
	if (aspect > 1.f)  {
		screenX0 = -aspect;
		screenX1 = aspect;
		screenY0 = -1.f;
		screenY1 = 1.f;
	} else {
		screenX0 = -1.f;
		screenX1 = 1.f;
		screenY0 = -1.f / aspect;
		screenY1 = 1.f / aspect;
	}

	const float hither = DEFAULT_EPSILON_STATIC;
	const float yon = 1e30f;
	lightProjection = Perspective(fov, hither, yon);

	// Compute cosine of cone surrounding projection directions
	const float opposite = tanf(Radians(fov) / 2.f);
	const float tanDiag = opposite * sqrtf(1.f + 1.f / (aspect * aspect));
	cosTotalWidth = cosf(atanf(tanDiag));
	if (aspect <= 1.f)
		area = 4.f * opposite * opposite / aspect;
	else
		area = 4.f * opposite * opposite * aspect;

	const float normalizeFactor = emittedPowerNormalize ? (1.f / Max(imageMap ? imageMap->GetSpectrumMeanY() : 1.f, 0.f)) : 1.f;

	emittedFactor = temperatureScale * gain * color * (power * efficiency * normalizeFactor /
			(2.f * M_PI *  (1.f - .5f * (1.f - cosTotalWidth))));
	if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN())
		emittedFactor = temperatureScale * gain * color;
}

void ProjectionLight::GetPreprocessedData(float *emittedFactorData, float *absolutePosData, float *lightNormalData,
		float *screenX0Data, float *screenX1Data, float *screenY0Data, float *screenY1Data,
		const luxrays::Transform **alignedLight2WorldData, const luxrays::Transform **lightProjectionData) const {
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

	if (lightNormalData) {
		lightNormalData[0] = lightNormal.x;
		lightNormalData[1] = lightNormal.y;
		lightNormalData[2] = lightNormal.z;
	}

	if (screenX0Data)
		*screenX0Data = screenX0;
	if (screenX1Data)
		*screenX1Data = screenX1;
	if (screenY0Data)
		*screenY0Data = screenY0;
	if (screenY1Data)
		*screenY1Data = screenY1;

	if (alignedLight2WorldData)
		*alignedLight2WorldData = &alignedLight2World;
	if (lightProjectionData)
		*lightProjectionData = &lightProjection;
}

float ProjectionLight::GetPower(const Scene &scene) const {
	return emittedFactor.Y() *
			(imageMap ? imageMap->GetSpectrumMeanY() : 1.f) *
			2.f * M_PI * (1.f - cosTotalWidth);
}

Spectrum ProjectionLight::Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Ray &ray, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	const Point rayOrig = absolutePos;
	const Point ps = Inverse(lightProjection) *
		Point(u0 * (screenX1 - screenX0) + screenX0, u1 * (screenY1 - screenY0) + screenY0, 0.f);
	const Vector rayDir = Normalize(alignedLight2World * Vector(ps.x, ps.y, ps.z));
	const float cos = Dot(rayDir, lightNormal);
	const float cos2 = cos * cos;
	emissionPdfW = 1.f / (area * cos2 * cos);

	if (directPdfA)
		*directPdfA = 1.f;
	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	Spectrum c = emittedFactor;
	if (imageMap)
		c *= imageMap->GetSpectrum(UV(u0, u1));

	ray.Update(rayOrig, rayDir, time);

	return c;
}

Spectrum ProjectionLight::Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        Ray &shadowRay, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Point shadowRayOrig = bsdf.GetRayOrigin(absolutePos - bsdf.hitPoint.p);
	const Vector toLight(absolutePos - shadowRayOrig);
	const float shadowRayDistanceSquared = toLight.LengthSquared();
	const float shadowRayDistance = sqrtf(shadowRayDistanceSquared);
	const Vector shadowRayDir = toLight / shadowRayDistance;

	// Check the side
	if (Dot(-shadowRayDir, lightNormal) < 0.f)
		return Spectrum();

	// Check if the point is inside the image plane
	const Vector localFromLight = Normalize(Inverse(alignedLight2World) * (-shadowRayDir));
	const Point p0 = lightProjection * Point(localFromLight.x, localFromLight.y, localFromLight.z);
	if ((p0.x < screenX0) || (p0.x >= screenX1) || (p0.y < screenY0) || (p0.y >= screenY1))
		return Spectrum();

	directPdfW = shadowRayDistanceSquared;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	if (emissionPdfW)
		*emissionPdfW = 0.f;

	Spectrum c = emittedFactor;
	if (imageMap) {
		const float u = (p0.x - screenX0) / (screenX1 - screenX0);
		const float v = (p0.y - screenY0) / (screenY1 - screenY0);
		c *= imageMap->GetSpectrum(UV(u, v));
	}

	shadowRay = Ray(shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return c;
}

bool ProjectionLight::IsAlwaysInShadow(const Scene &scene,
			const luxrays::Point &p, const luxrays::Normal &n) const {
	const Vector toLight(absolutePos - p);
	const float distance = toLight.Length();
	const Vector dir = toLight / distance;

	// Check the side
	return (Dot(-dir, lightNormal) < 0.f);
}

Properties ProjectionLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property(prefix + ".type")("projection"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".power")(power));
	props.Set(Property(prefix + ".normalizebycolor")(emittedPowerNormalize));
	props.Set(Property(prefix + ".efficiency")(efficiency));
	props.Set(Property(prefix + ".position")(localPos));
	props.Set(Property(prefix + ".target")(localTarget));
	props.Set(Property(prefix + ".fov")(fov));

	const string fileName = useRealFileName ?
		imageMap->GetName() : imgMapCache.GetSequenceFileName(imageMap);
	props.Set(Property(prefix + ".mapfile")(fileName));
	props.Set(imageMap->ToProperties(prefix, false));

	return props;
}
