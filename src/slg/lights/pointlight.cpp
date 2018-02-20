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

#include <math.h>

#include "luxrays/core/epsilon.h"
#include "slg/lights/pointlight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PointLight
//------------------------------------------------------------------------------

PointLight::PointLight() : localPos(0.f), radius(0.f), color(1.f), power(0.f), efficency(0.f) {
}

PointLight::~PointLight() {
}

void PointLight::Preprocess() {
	emittedFactor = gain * color * (power * efficency / color.Y());
	if (radius == 0.f)
		emittedFactor *= 1.f / (4.f * M_PI);
	else
		emittedFactor *= 1.f / M_PI;

	if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN())
		emittedFactor = gain * color;

	absolutePos = lightToWorld * localPos;
	
	radiusSquared = radius * radius;
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
	if (radius == 0.f)
		return emittedFactor.Y() * 4.f * M_PI;
	else
		return emittedFactor.Y() * M_PI;
}

bool PointLight::SphereIntersect(const Ray &ray, float &hitT) const {
	const Vector op = absolutePos - ray.o;
	const float b = Dot(op, ray.d);

	float det = b * b - Dot(op, op) + radiusSquared;
	if (det < 0.f)
		return false;
	else
		det = sqrtf(det);

	float t = b - det;
	if ((t > ray.mint) && ((t < ray.maxt)))
		hitT = t;
	else {
		t = b + det;

		if ((t > ray.mint) && ((t < ray.maxt)))
			hitT = t;
		else
			return false;
	}

	return true;
}

Spectrum PointLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	if (radius == 0.f) {
		*orig = absolutePos;
		*dir = UniformSampleSphere(u0, u1);
		*emissionPdfW = 1.f / (4.f * M_PI);

		if (directPdfA)
			*directPdfA = 1.f;
		if (cosThetaAtLight)
			*cosThetaAtLight = 1.f;

		return emittedFactor;
	} else {
		const Vector normal = UniformSampleSphere(u0, u1);
		*orig = absolutePos + radius * normal;
		
		// Build a local coordinate system
		Frame localFrame(normal);

		// The direction expressed relative to local coordinate system
		Vector localDirOut = CosineSampleHemisphere(u2, u3, emissionPdfW);
		// Cannot really not emit the particle, so just bias it to the correct angle
		localDirOut.z = Max(localDirOut.z, DEFAULT_COS_EPSILON_STATIC);

		const float invArea = 1.f / (4.f * M_PI * radiusSquared);
		*emissionPdfW *= invArea;

		// The direction expressed relative to global coordinate system
		*dir = localFrame.ToWorld(localDirOut);

		if (directPdfA)
			*directPdfA =  invArea;

		if (cosThetaAtLight)
			*cosThetaAtLight = CosTheta(localDirOut);

		return emittedFactor * invArea * CosTheta(localDirOut);		
	}
}

Spectrum PointLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Vector toLight(absolutePos - p);
	const float centerDistanceSquared = toLight.LengthSquared();
	const float centerDistance = sqrtf(centerDistanceSquared);

	if (radius == 0.f) {
		*distance = centerDistance;
		*dir = toLight / centerDistance;

		if (cosThetaAtLight)
			*cosThetaAtLight = 1.f;

		*directPdfW = centerDistanceSquared;

		if (emissionPdfW)
			*emissionPdfW = UniformSpherePdf();

		return emittedFactor;
	} else {
		// Check if the point is inside the sphere
		if (centerDistanceSquared - radiusSquared < DEFAULT_EPSILON_STATIC) {
			// The point is inside the sphere, return black
			return Spectrum();
		} else {
			// The point isn't inside the sphere
			
			// Build a local coordinate system
			const Vector localZ = toLight / centerDistance;
			Frame localFrame(localZ);

			// Sample sphere uniformly inside subtended cone
			const float cosThetaMax = sqrtf(Max(0.f, 1.f - radiusSquared / centerDistanceSquared));

			const Point &rayOrig = p;
			const Vector localRayDir = UniformSampleCone(u0, u1, cosThetaMax);

			if (CosTheta(localRayDir) < DEFAULT_COS_EPSILON_STATIC)
				return Spectrum();

			const Vector rayDir = localFrame.ToWorld(localRayDir);
			const Ray ray(rayOrig, rayDir);

			// Check the intersection with the sphere
			if (!SphereIntersect(ray, *distance))
				*distance = Dot(toLight, rayDir);
			*dir = rayDir;

			if (cosThetaAtLight)
				*cosThetaAtLight = CosTheta(localRayDir);

			*directPdfW = UniformConePdf(cosThetaMax);

			const float invArea = 1.f / (4.f * M_PI * radiusSquared);
			if (emissionPdfW)
				*emissionPdfW = invArea * CosTheta(localRayDir) * INV_PI;

			return emittedFactor * invArea;
		}
	}
}

Properties PointLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache, 0);

	props.Set(Property(prefix + ".type")("point"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".power")(power));
	props.Set(Property(prefix + ".efficency")(efficency));
	props.Set(Property(prefix + ".position")(localPos));
	props.Set(Property(prefix + ".radius")(radius));

	return props;
}
