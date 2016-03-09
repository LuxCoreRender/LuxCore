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

#include "slg/lights/laserlight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LaserLight
//------------------------------------------------------------------------------

LaserLight::LaserLight() :
	color(1.f), power(0.f), efficency(0.f),
	localPos(), localTarget(0.f, 0.f, 1.f),
	radius(.01f) {
}

LaserLight::~LaserLight() {
}

void LaserLight::Preprocess() {
	emittedFactor = gain * color * (power * efficency / (M_PI * radius * radius * color.Y()));
	if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN())
		emittedFactor = gain * color;

	absoluteLightPos = lightToWorld * localPos;
	absoluteLightDir = Normalize(lightToWorld * (localTarget - localPos));
	CoordinateSystem(absoluteLightDir, &x, &y);
}

void LaserLight::GetPreprocessedData(float *emittedFactorData, float *absolutePosData,
		 float *absoluteDirData, float *xData, float *yData) const {
	if (emittedFactorData) {
		emittedFactorData[0] = emittedFactor.c[0];
		emittedFactorData[1] = emittedFactor.c[1];
		emittedFactorData[2] = emittedFactor.c[2];
	}

	if (absolutePosData) {
		absolutePosData[0] = absoluteLightPos.x;
		absolutePosData[1] = absoluteLightPos.y;
		absolutePosData[2] = absoluteLightPos.z;
	}

	if (absoluteDirData) {
		absoluteDirData[0] = absoluteLightDir.x;
		absoluteDirData[1] = absoluteLightDir.y;
		absoluteDirData[2] = absoluteLightDir.z;
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

float LaserLight::GetPower(const Scene &scene) const {
	return emittedFactor.Y() * M_PI * radius * radius;
}

Spectrum LaserLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	float d1, d2;
	ConcentricSampleDisk(u0, u1, &d1, &d2);
	*orig = absoluteLightPos - radius * (d1 * x + d2 * y);
	*dir = absoluteLightDir;
	
	*emissionPdfW = 1.f / (M_PI * radius * radius);

	if (directPdfA)
		*directPdfA = 1.f;
	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	return emittedFactor;
}

Spectrum LaserLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	*dir = -absoluteLightDir;
	
	const Point &rayOrig = p;
	const Vector &rayDir = *dir;
	const Point &planeCenter = absoluteLightPos;
	const Vector &planeNormal = absoluteLightDir;

	// Intersect the shadow ray with light plane
	const float denom = Dot(planeNormal, rayDir);
	const Vector pr = planeCenter - rayOrig;
	float d = Dot(pr, planeNormal);

	if (fabsf(denom) > DEFAULT_COS_EPSILON_STATIC) {
		// There is a valid intersection
		d /= denom; 

		if ((d <= 0.f) || (denom >= 0.f))
			return Spectrum();
	} else
		return Spectrum();

	const Point lightPoint = rayOrig + d * rayDir;

	// Check if the point is inside the emitting circle
	const float radius2 = radius * radius;
	if ((lightPoint - absoluteLightPos).LengthSquared() > radius2)
		return Spectrum();
	
	// Ok, the light is visible
	
	*distance = d;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	*directPdfW = 1.f;

	if (emissionPdfW)
		*emissionPdfW = 0.f;

	return emittedFactor;
}

Properties LaserLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("laser"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".power")(power));
	props.Set(Property(prefix + ".efficency")(efficency));
	props.Set(Property(prefix + ".position")(localPos));
	props.Set(Property(prefix + ".target")(localTarget));
	props.Set(Property(prefix + ".radius")(radius));

	return props;
}
