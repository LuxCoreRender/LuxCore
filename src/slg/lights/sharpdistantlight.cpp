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
#include "slg/scene/scene.h"
#include "slg/lights/sharpdistantlight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SharpDistantLight
//------------------------------------------------------------------------------

SharpDistantLight::SharpDistantLight() :
		color(1.f), localLightDir(0.f, 0.f, 1.f) {
}

SharpDistantLight::~SharpDistantLight() {
}

void SharpDistantLight::Preprocess() {
	InfiniteLightSource::Preprocess();

	absoluteLightDir = Normalize(lightToWorld * localLightDir);
	CoordinateSystem(absoluteLightDir, &x, &y);
}

void SharpDistantLight::GetPreprocessedData(float *absoluteLightDirData,
		float *xData, float *yData) const {
	if (absoluteLightDirData) {
		absoluteLightDirData[0] = absoluteLightDir.x;
		absoluteLightDirData[1] = absoluteLightDir.y;
		absoluteLightDirData[2] = absoluteLightDir.z;
	}

	if (xData) {
		xData[0] = x.x;
		xData[1] = x.y;
		xData[2] = x.z;
	}

	if (yData) {
		yData[0] = y.x;
		yData[1] = y.y;
		yData[2] = y.z;
	}
}

float SharpDistantLight::GetPower(const Scene &scene) const {
	const float envRadius = GetEnvRadius(scene);

	return temperatureScale.Y() * gain.Y() * color.Y() * M_PI * envRadius * envRadius;
}

Spectrum SharpDistantLight::Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Ray &ray, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	float d1, d2;
	ConcentricSampleDisk(u0, u1, &d1, &d2);
	const Point rayOrig = worldCenter - envRadius * (absoluteLightDir + d1 * x + d2 * y);

	emissionPdfW = 1.f / (M_PI * envRadius * envRadius);

	if (directPdfA)
		*directPdfA = 1.f;

	ray.Update(rayOrig, absoluteLightDir, time);

	return temperatureScale * gain * color;
}

Spectrum SharpDistantLight::Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        Ray &shadowRay, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Vector shadowRayDir = -absoluteLightDir;

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	const Point shadowRayOrig = bsdf.GetRayOrigin(shadowRayDir);
	const Vector toCenter(worldCenter - shadowRayOrig);
	const float centerDistanceSquared = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, shadowRayDir);
	const float shadowRayDistance = approach + sqrtf(Max(0.f, envRadius * envRadius -
		centerDistanceSquared + approach * approach));

	directPdfW = 1.f;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	if (emissionPdfW)
		*emissionPdfW = 1.f / (M_PI * envRadius * envRadius);

	shadowRay = Ray(shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return temperatureScale * gain * color;
}

Properties SharpDistantLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property(prefix + ".type")("sharpdistant"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".direction")(localLightDir));

	return props;
}
