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
#include "slg/lights/constantinfinitelight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ConstantInfiniteLight
//------------------------------------------------------------------------------

ConstantInfiniteLight::ConstantInfiniteLight() : color(1.f), visibilityMapCache(nullptr) {
}

ConstantInfiniteLight::~ConstantInfiniteLight() {
	delete visibilityMapCache;
}

void ConstantInfiniteLight::GetPreprocessedData(const EnvLightVisibilityCache **elvc) const {
	if (elvc)
		*elvc = visibilityMapCache;
}

float ConstantInfiniteLight::GetPower(const Scene &scene) const {
	const float envRadius = GetEnvRadius(scene);

	return temperatureScale.Y() * gain.Y() * (4.f * M_PI * M_PI * envRadius * envRadius) *
		color.Y();
}

Spectrum ConstantInfiniteLight::GetRadiance(const Scene &scene,
		const BSDF *bsdf, const Vector &dir, float *directPdfA, float *emissionPdfW) const {
	const float envRadius = GetEnvRadius(scene);

	if (visibilityMapCache && (!bsdf || visibilityMapCache->IsCacheEnabled(*bsdf))) {
		const Vector w = -dir;
		float u, v, latLongMappingPdf;
		ToLatLongMapping(w, &u, &v, &latLongMappingPdf);
		if (latLongMappingPdf == 0.f)
			return Spectrum();

		if (directPdfA) {
			if (bsdf)
				*directPdfA =  visibilityMapCache->Pdf(*bsdf, u, v) * latLongMappingPdf;
			else
				*directPdfA = 0.f;
		}

		if (emissionPdfW)
			*emissionPdfW = UniformSpherePdf() / (M_PI * envRadius * envRadius);
	} else {
		if (directPdfA)
			*directPdfA = bsdf ? UniformSpherePdf() : 0.f;

		if (emissionPdfW)
			*emissionPdfW = UniformSpherePdf() / (M_PI * envRadius * envRadius);
	}

	return temperatureScale * gain * color;
}

Spectrum ConstantInfiniteLight::Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Ray &ray, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	// Compute InfiniteLight ray weight
	emissionPdfW = UniformSpherePdf() / (M_PI * envRadius * envRadius);

	if (directPdfA)
		*directPdfA = UniformSpherePdf();

	// Choose p1 on scene bounding sphere
	Point p1 = worldCenter + envRadius * UniformSampleSphere(u0, u1);

	// Choose p2 on scene bounding sphere
	Point p2 = worldCenter + envRadius * UniformSampleSphere(u2, u3);

	// Construct ray between p1 and p2
	ray.Update(p1, Normalize(p2 - p1), time);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(Normalize(worldCenter -  p1), ray.d);

	return GetRadiance(scene, nullptr, ray.d);
}

Spectrum ConstantInfiniteLight::Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        Ray &shadowRay, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const float envRadius = GetEnvRadius(scene);

	Vector shadowRayDir;
	if (visibilityMapCache && visibilityMapCache->IsCacheEnabled(bsdf)) {
		float uv[2];
		float distPdf;

		visibilityMapCache->Sample(bsdf, u0, u1, uv, &distPdf);
		if (distPdf == 0.f)
			return Spectrum();

		float latLongMappingPdf;
		FromLatLongMapping(uv[0], uv[1], &shadowRayDir, &latLongMappingPdf);
		if (latLongMappingPdf == 0.f)
			return Spectrum();

		directPdfW = distPdf * latLongMappingPdf;

		if (emissionPdfW)
			*emissionPdfW = distPdf * latLongMappingPdf / (M_PI * envRadius * envRadius);
	} else {
		shadowRayDir = UniformSampleSphere(u0, u1);

		directPdfW = UniformSpherePdf();

		if (emissionPdfW)
			*emissionPdfW = UniformSpherePdf() / (M_PI * envRadius * envRadius);
	}

	const Point worldCenter = scene.dataSet->GetBSphere().center;

	const Point shadowRayOrig = bsdf.GetRayOrigin(shadowRayDir);
	const Vector toCenter(worldCenter - shadowRayOrig);
	const float centerDistanceSquared = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, shadowRayDir);
	const float shadowRayDistance = approach + sqrtf(Max(0.f, envRadius * envRadius -
		centerDistanceSquared + approach * approach));

	const Point emisPoint(shadowRayOrig + shadowRayDistance * shadowRayDir);
	const Normal emisNormal(Normalize(worldCenter - emisPoint));

	const float cosAtLight = Dot(emisNormal, -shadowRayDir);
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();
	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	shadowRay = Ray(shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);
	
	return temperatureScale * gain * color;
}

UV ConstantInfiniteLight::GetEnvUV(const luxrays::Vector &dir) const {
	UV uv;
	const Vector w = -dir;
	ToLatLongMapping(w, &uv.u, &uv.v);
	
	return uv;
}

void ConstantInfiniteLight::UpdateVisibilityMap(const Scene *scene, const bool useRTMode) {
	delete visibilityMapCache;
	visibilityMapCache = nullptr;
	
	if (useRTMode)
		return;

	if (useVisibilityMapCache) {
		visibilityMapCache = new EnvLightVisibilityCache(scene, this,
				EnvLightVisibilityCache::defaultLuminanceMapWidth,
				EnvLightVisibilityCache::defaultLuminanceMapHeight,
				visibilityMapCacheParams);		
		visibilityMapCache->Build();
	}
}

Properties ConstantInfiniteLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = EnvLightSource::ToProperties(imgMapCache, useRealFileName);

	props.Set(Property(prefix + ".type")("constantinfinite"));
	props.Set(Property(prefix + ".color")(color));

	props.Set(Property(prefix + ".visibilitymapcache.enable")(useVisibilityMapCache));
	if (useVisibilityMapCache)
		props << EnvLightVisibilityCache::Params2Props(prefix, visibilityMapCacheParams);

	return props;
}
