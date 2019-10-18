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

#include "slg/bsdf/bsdf.h"
#include "slg/lights/distantlight.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// DistantLight
//------------------------------------------------------------------------------

DistantLight::DistantLight() :
		color(1.f), localLightDir(0.f, 0.f, 1.f), theta(0.f) {
}

DistantLight::~DistantLight() {
}

void DistantLight::Preprocess() {
	if (theta == 0.f) {
		sin2ThetaMax = 2.f * MachineEpsilon::E(1.f);
		cosThetaMax = 1.f - MachineEpsilon::E(1.f);
	} else {
		const float radTheta = Radians(theta);
		sin2ThetaMax = sinf(Radians(radTheta)) * sinf(radTheta);
		cosThetaMax = cosf(radTheta);
	}

	absoluteLightDir = Normalize(lightToWorld * localLightDir);
	CoordinateSystem(absoluteLightDir, &x, &y);
}

void DistantLight::GetPreprocessedData(float *absoluteLightDirData, float *xData, float *yData,
		float *sin2ThetaMaxData, float *cosThetaMaxData) const {
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

	if (sin2ThetaMaxData)
		*sin2ThetaMaxData = sin2ThetaMax;
	if (cosThetaMaxData)
		*cosThetaMaxData = cosThetaMax;
}

float DistantLight::GetPower(const Scene &scene) const {
	const float envRadius = GetEnvRadius(scene);

	return gain.Y() * color.Y() * M_PI * envRadius * envRadius;
}

Spectrum DistantLight::Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Point &rayOrig, Vector &rayDir, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	rayDir = UniformSampleCone(u0, u1, cosThetaMax, x, y, absoluteLightDir);
	const float uniformConePdf = UniformConePdf(cosThetaMax);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(rayDir, absoluteLightDir);

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	float d1, d2;
	ConcentricSampleDisk(u2, u3, &d1, &d2);
	rayOrig = worldCenter - envRadius * (absoluteLightDir + d1 * x + d2 * y);

	emissionPdfW = uniformConePdf / (M_PI * envRadius * envRadius);

	if (directPdfA)
		*directPdfA = uniformConePdf;

	return gain * color;
}

Spectrum DistantLight::Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        Vector &shadowRayDir, float &shadowRayDistance, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	shadowRayDir = -UniformSampleCone(u0, u1, cosThetaMax, x, y, absoluteLightDir);

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	const Point &pSurface = bsdf.GetRayOrigin(worldCenter - bsdf.hitPoint.p);
	const Vector toCenter(worldCenter - pSurface);
	const float centerDistanceSquared = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, shadowRayDir);
	shadowRayDistance = approach + sqrtf(Max(0.f, envRadius * envRadius -
		centerDistanceSquared + approach * approach));

	const float uniformConePdf = UniformConePdf(cosThetaMax);
	directPdfW = uniformConePdf;

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(-absoluteLightDir, shadowRayDir);

	if (emissionPdfW)
		*emissionPdfW =  uniformConePdf / (M_PI * envRadius * envRadius);

	return gain * color;
}

Properties DistantLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property(prefix + ".type")("distant"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".direction")(localLightDir));
	props.Set(Property(prefix + ".theta")(theta));

	return props;
}
