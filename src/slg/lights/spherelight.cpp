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
#include "slg/lights/spherelight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SphereLight
//------------------------------------------------------------------------------

SphereLight::SphereLight() : PointLight(), radius(1.f) {
}

SphereLight::~SphereLight() {
}

void SphereLight::Preprocess() {
	PointLight::Preprocess();

	radiusSquared = radius * radius;
	invArea = 1.f / (4.f * M_PI * radiusSquared);
}

bool SphereLight::SphereIntersect(const Ray &ray, float &hitT) const {
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

Spectrum SphereLight::Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Ray &ray, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	const Vector normal = UniformSampleSphere(u0, u1);
	const Point rayOrig = absolutePos + radius * normal;

	// Build a local coordinate system
	Frame localFrame(normal);

	// The direction expressed relative to local coordinate system
	Vector localDirOut = CosineSampleHemisphere(u2, u3, &emissionPdfW);
	// Cannot really not emit the particle, so just bias it to the correct angle
	localDirOut.z = Max(localDirOut.z, DEFAULT_COS_EPSILON_STATIC);

	emissionPdfW *= invArea;

	// The direction expressed relative to global coordinate system
	const Vector rayDir = localFrame.ToWorld(localDirOut);

	if (directPdfA)
		*directPdfA =  invArea;

	if (cosThetaAtLight)
		*cosThetaAtLight = CosTheta(localDirOut);

	ray.Update(rayOrig, rayDir, time);

	return emittedFactor * invArea * CosTheta(localDirOut) * INV_PI;
}

Spectrum SphereLight::Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        Ray &shadowRay, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Vector toLight(absolutePos - bsdf.hitPoint.p);
	const float centerDistanceSquared = toLight.LengthSquared();
	const float centerDistance = sqrtf(centerDistanceSquared);

	// Check if the point is inside the sphere
	if (centerDistanceSquared - radiusSquared < DEFAULT_EPSILON_STATIC) {
		// The point is inside the sphere, return black
		return Spectrum();
	}

	// The point isn't inside the sphere

	const float cosThetaMax = sqrtf(Max(0.f, 1.f - radiusSquared / centerDistanceSquared));

	// Build a local coordinate system
	const Vector localZ = toLight / centerDistance;
	Frame localFrame(localZ);

	// Sample sphere uniformly inside subtended cone
	const Vector localRayDir = UniformSampleCone(u0, u1, cosThetaMax);

	if (CosTheta(localRayDir) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	const Vector shadowRayDir = localFrame.ToWorld(localRayDir);
	const Point shadowRayOrig = bsdf.GetRayOrigin(shadowRayDir);
	const Ray ray(shadowRayOrig, shadowRayDir);

	// Check the intersection with the sphere
	float shadowRayDistance;
	if (!SphereIntersect(ray, shadowRayDistance))
		shadowRayDistance = Dot(toLight, shadowRayDir);

	if (cosThetaMax > 1.f - DEFAULT_EPSILON_STATIC) {
		// If the subtended angle is too small, I replace the computations for
		// cosThetaAtLight, directPdfW, emissionPdfW and return factor with that of a 
		// point light source in order to avoiding banding due to (lack of) numerical precision.
		if (cosThetaAtLight)
			*cosThetaAtLight = 1.f;

		directPdfW = shadowRayDistance * shadowRayDistance;

		if (emissionPdfW)
			*emissionPdfW = UniformSpherePdf();

		shadowRay = Ray(shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

		return emittedFactor * (1.f / (4.f * M_PI));
	}
	else {
		if (cosThetaAtLight)
			*cosThetaAtLight = CosTheta(localRayDir);

		directPdfW = UniformConePdf(cosThetaMax);

		if (emissionPdfW)
			*emissionPdfW = invArea * CosTheta(localRayDir) * INV_PI;

		shadowRay = Ray(shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

		return emittedFactor * invArea * INV_PI;
	}
}

Properties SphereLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = PointLight::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property(prefix + ".type")("sphere"));
	props.Set(Property(prefix + ".radius")(radius));

	return props;
}
