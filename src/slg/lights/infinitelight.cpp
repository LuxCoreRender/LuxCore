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

#include <memory>

#include "luxrays/usings.h"
#include "slg/bsdf/bsdf.h"
#include "slg/scene/scene.h"
#include "slg/usings.h"
#include "slg/lights/infinitelight.h"

using namespace std;
using namespace luxrays;
using namespace slg;
		
//------------------------------------------------------------------------------
// MIS compensation
//------------------------------------------------------------------------------

// MIS compensation (Karlik et al., "MIS Compensation: Optimizing Sampling
// Techniques in Multiple Importance Sampling", TOG 38(6), 2019): under MIS,
// BSDF sampling already covers the directions where the environment is not
// brighter than its average radiance, so Illuminate() samples the radiance
// minus its average (over the sphere, hence the sin(theta) factors that make
// the pdf proportional to it in solid angle).
//
// A fraction of the samples keeps following the radiance: every direction
// with emission keeps a non zero pdf, so the estimator stays unbiased where
// direct light sampling is used without MIS (a path depth of 1, shadow
// transparency, an environment invisible to indirect rays), in BIDIRCPU and
// in the DLSC build.
static const float MIS_COMPENSATION_DEFENSIVE_FRACTION = .1f;

static unique_ptr<Distribution2D> BuildMISCompensatedDistribution(const vector<float> &data,
		const u_int width, const u_int height) {
	vector<float> sinTheta(height);
	for (u_int y = 0; y < height; ++y)
		sinTheta[y] = sinf(M_PI * (y + .5f) / height);

	double radianceSum = 0.0;
	double sinThetaSum = 0.0;
	for (u_int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x)
			radianceSum += sinTheta[y] * data[x + y * width];
		sinThetaSum += width * sinTheta[y];
	}
	const float average = radianceSum / sinThetaSum;

	double compensatedSum = 0.0;
	for (u_int y = 0; y < height; ++y)
		for (u_int x = 0; x < width; ++x)
			compensatedSum += sinTheta[y] * Max(data[x + y * width] - average, 0.f);

	// A constant image: there is nothing to remove
	if (compensatedSum <= 0.0)
		return nullptr;

	// The mix of the two normalized densities
	const float compensatedScale = (1.f - MIS_COMPENSATION_DEFENSIVE_FRACTION) / compensatedSum;
	const float radianceScale = MIS_COMPENSATION_DEFENSIVE_FRACTION / radianceSum;
	vector<float> compensated(width * height);
	for (u_int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x) {
			const u_int index = x + y * width;
			compensated[index] = sinTheta[y] * (compensatedScale * Max(data[index] - average, 0.f) +
					radianceScale * data[index]);
		}
	}

	return std::make_unique<Distribution2D>(compensated, width, height);
}

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

InfiniteLight::InfiniteLight() :
	imageMap(nullptr), useMISCompensation(false), imageMapDistribution(nullptr),
	misCompensatedDistribution(nullptr), visibilityMapCache(nullptr) {
}

InfiniteLight::~InfiniteLight() {
}

void InfiniteLight::Preprocess() {
	EnvLightSource::Preprocess();

	auto& imageMapStorage = imageMap->GetStorage();

	std::vector<float> data(imageMap->GetWidth() * imageMap->GetHeight());
	//float maxVal = -INFINITY;
	//float minVal = INFINITY;
	for (u_int y = 0; y < imageMap->GetHeight(); ++y) {
		for (u_int x = 0; x < imageMap->GetWidth(); ++x) {
			const u_int index = x + y * imageMap->GetWidth();

			if (sampleUpperHemisphereOnly && (y > imageMap->GetHeight() / 2))
				data[index] = 0.f;
			else
				data[index] = imageMapStorage.GetFloat(index);

			if (!IsValid(data[index]))
				throw runtime_error("Pixel (" + ToString(x) + ", " + ToString(y) + ") in infinite light has an invalid value: " + ToString(data[index]));

			//maxVal = Max(data[index], maxVal);
			//minVal = Min(data[index], minVal);
		}
	}
	
	//SLG_LOG("InfiniteLight luminance  Max=" << maxVal << " Min=" << minVal);

	imageMapDistribution =
		std::make_unique<Distribution2D>(
			data,
			imageMap->GetWidth(),
			imageMap->GetHeight()
		);

	misCompensatedDistribution = useMISCompensation ?
		BuildMISCompensatedDistribution(data, imageMap->GetWidth(), imageMap->GetHeight()) :
		nullptr;
}


std::tuple<Distribution2DConstRef, Distribution2DConstRef, EnvLightVisibilityCacheRPtr>
InfiniteLight::GetPreprocessedData() const {
	return std::make_tuple(std::cref(*imageMapDistribution), std::cref(GetIlluminateDistribution()),
			std::cref(visibilityMapCache));
}

float InfiniteLight::GetPower(SceneConstRef scene) const {
	const float envRadius = GetEnvRadius(scene);

	// TODO: I should consider sampleUpperHemisphereOnly here
	return temperatureScale.Y() * gain.Y() * imageMap->GetSpectrumMeanY() *
			(4.f * M_PI * M_PI * envRadius * envRadius);
}

UV InfiniteLight::GetEnvUV(const luxrays::Vector &dir) const {
	UV uv;
	const Vector localDir = Normalize(Inverse(lightToWorld) * -dir);
	ToLatLongMapping(localDir, &uv.u, &uv.v);
	
	return uv;
}

Spectrum InfiniteLight::GetRadiance(SceneConstRef scene,
		const BSDF *bsdf, const Vector &dir,
		float *directPdfA, float *emissionPdfW) const {
	const Vector localDir = Normalize(Inverse(lightToWorld) * -dir);

	float u, v, latLongMappingPdf;
	ToLatLongMapping(localDir, &u, &v, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return Spectrum();

	if (directPdfA) {
		if (!bsdf)
			*directPdfA = 0.f;
		else if (visibilityMapCache && visibilityMapCache->IsCacheEnabled(*bsdf)) {
			*directPdfA = visibilityMapCache->Pdf(*bsdf, u, v) * latLongMappingPdf;
		} else
			*directPdfA = GetIlluminateDistribution().Pdf(u, v) * latLongMappingPdf;
	}

	if (emissionPdfW) {
		const float envRadius = GetEnvRadius(scene);
		*emissionPdfW = imageMapDistribution->Pdf(u, v) * latLongMappingPdf / (M_PI * envRadius * envRadius);
	}

	return temperatureScale * gain * imageMap->GetSpectrum(UV(u, v));
}

Spectrum InfiniteLight::Emit(SceneConstRef scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Ray &ray, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	float uv[2];
	float distPdf;
	imageMapDistribution->SampleContinuous(u0, u1, uv, &distPdf);
	if (distPdf == 0.f)
		return Spectrum();
	
	Vector localDir;
	float latLongMappingPdf;
	FromLatLongMapping(uv[0], uv[1], &localDir, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return Spectrum();

	// Compute the ray direction
	const Vector rayDir = -Normalize(lightToWorld * localDir);

	// Compute the ray origin
	Vector x, y;
    CoordinateSystem(-rayDir, &x, &y);
    float d1, d2;
    ConcentricSampleDisk(u2, u3, &d1, &d2);

	const Point worldCenter = scene.GetDataSet().GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);
	const Point pDisk = worldCenter + envRadius * (d1 * x + d2 * y);
	const Point rayOrig = pDisk - envRadius * rayDir;

	// Compute InfiniteLight ray weight
	emissionPdfW = distPdf * latLongMappingPdf / (M_PI * envRadius * envRadius);

	if (directPdfA) {
		// The pdf of Illuminate()
		*directPdfA = (misCompensatedDistribution ? misCompensatedDistribution->Pdf(uv[0], uv[1]) : distPdf) *
				latLongMappingPdf;
	}

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(Normalize(worldCenter - rayOrig), rayDir);

	const Spectrum result = temperatureScale * gain * imageMap->GetSpectrum(uv);
	assert (!result.IsNaN() && !result.IsInf() && !result.IsNeg());

	ray.Update(rayOrig, rayDir, time);

	return result;
}

Spectrum InfiniteLight::Illuminate(SceneConstRef scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        Ray &shadowRay, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	float uv[2];
	float distPdf;	
	if (visibilityMapCache && visibilityMapCache->IsCacheEnabled(bsdf))
		visibilityMapCache->Sample(bsdf, u0, u1, uv, &distPdf);
	else
		GetIlluminateDistribution().SampleContinuous(u0, u1, uv, &distPdf);
	if (distPdf == 0.f)
		return Spectrum();

	Vector localDir;
	float latLongMappingPdf;
	FromLatLongMapping(uv[0], uv[1], &localDir, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return Spectrum();

	const Vector shadowRayDir = Normalize(lightToWorld * localDir);
	
	const Point worldCenter = scene.GetDataSet().GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

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

	directPdfW = distPdf * latLongMappingPdf;
	assert (!isnan(directPdfW) && !isinf(directPdfW) && (directPdfW > 0.f));

	if (emissionPdfW) {
		// The pdf of Emit()
		const float emissionDistPdf = misCompensatedDistribution ?
			imageMapDistribution->Pdf(uv[0], uv[1]) : distPdf;
		*emissionPdfW = emissionDistPdf * latLongMappingPdf / (M_PI * envRadius * envRadius);
	}

	const Spectrum result = temperatureScale * gain * imageMap->GetSpectrum(UV(uv[0], uv[1]));
	assert (!result.IsNaN() && !result.IsInf() && !result.IsNeg());

	shadowRay = Ray(shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return result;
}

void InfiniteLight::UpdateVisibilityMap(SceneConstRef scene, const bool useRTMode) {
	visibilityMapCache.reset();

	if (useRTMode)
		return;

	if (useVisibilityMapCache) {
		// Scale the infinitelight image map to the requested size
		ImageMapUPtr luminanceMapImage(imageMap->Copy());
		// Select the image luminance
		luminanceMapImage->SelectChannel(ImageMapStorage::WEIGHTED_MEAN);
		luminanceMapImage->Preprocess();

		visibilityMapCache = std::make_unique<EnvLightVisibilityCache>(
			scene, this, std::move(luminanceMapImage), visibilityMapCacheParams
		);
		visibilityMapCache->Build();
	}
}

PropertiesUPtr InfiniteLight::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	PropertiesUPtr props = EnvLightSource::ToProperties(imgMapCache, useRealFileName);

	props->Set(Property(prefix + ".type")("infinite"));
	const string fileName = useRealFileName ?
		imageMap->GetName() : imgMapCache.GetSequenceFileName(*imageMap);
	props->Set(Property(prefix + ".file")(fileName));
	props->Set(imageMap->ToProperties(prefix, false));
	props->Set(Property(prefix + ".gamma")(1.f));
	props->Set(Property(prefix + ".sampleupperhemisphereonly")(sampleUpperHemisphereOnly));
	props->Set(Property(prefix + ".miscompensation.enable")(useMISCompensation));

	props->Set(Property(prefix + ".visibilitymapcache.enable")(useVisibilityMapCache));
	if (useVisibilityMapCache)
		*props << EnvLightVisibilityCache::Params2Props(prefix, visibilityMapCacheParams);

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
