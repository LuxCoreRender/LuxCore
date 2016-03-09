/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#include "slg/lights/sharpdistantlight.h"
#include "slg/scene/scene.h"

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

	return gain.Y() * color.Y() * M_PI * envRadius * envRadius;
}

Spectrum SharpDistantLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	*dir = absoluteLightDir;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	float d1, d2;
	ConcentricSampleDisk(u0, u1, &d1, &d2);
	*orig = worldCenter - envRadius * (absoluteLightDir + d1 * x + d2 * y);

	*emissionPdfW = 1.f / (M_PI * envRadius * envRadius);

	if (directPdfA)
		*directPdfA = 1.f;

	return gain * color;
}

Spectrum SharpDistantLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	*dir = -absoluteLightDir;

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	const Vector toCenter(worldCenter - p);
	const float centerDistance = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, *dir);
	*distance = approach + sqrtf(Max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	*directPdfW = 1.f;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	if (emissionPdfW)
		*emissionPdfW = 1.f / (M_PI * envRadius * envRadius);

	return gain * color;
}

Properties SharpDistantLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("sharpdistant"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".direction")(localLightDir));

	return props;
}
