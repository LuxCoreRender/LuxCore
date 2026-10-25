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
#include "slg/lights/sky3light.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SkyLight3
//------------------------------------------------------------------------------

/* The sky radiance evaluation is based on the "Prague Sky Model":

   Wilkie et al., "A fitted radiance and attenuation model for realistic
   atmospheres", ACM Transactions on Graphics (Proceedings of SIGGRAPH 2021),
   40(4), 2021.

   Vevoda et al., "A Wide Spectral Range Sky Radiance Model", Computer Graphics
   Forum, 41(7), 2022.

   Implemented by Petr Vevoda (https://github.com/PetrVevoda/pragueskymodel),
   released under the Apache License 2.0. See
   src/slg/lights/data/pragueskymodel/ for the original implementation and its
   license.
*/

SkyLight3::SkyLight3() : localSunDir(0.f, 0.f, 1.f), visibility(59.4f),
		altitude(0.f), groundAlbedo(0.f), groundColor(0.f, 0.f, 0.f),
		hasGround(false), hasGroundAutoScale(true),
		mapWidth(2048), mapHeight(1024),
		useVisibilityMapCache(false), radianceMap(nullptr),
		localSunElevation(0.0), localSunAzimuth(0.0),
		channels(0), channelStart(0.0), channelWidth(0.0),
		isGroundBlack(false),
		meanRadianceY(0.f),
		skyDistribution(nullptr), visibilityMapCache(nullptr) {
}

SkyLight3::~SkyLight3() {
}

Spectrum SkyLight3::ComputeSkyRadiance(const Vector &w) const {
	// The model assumes a frame with the Z axis pointing up and the observer
	// at the origin: convert the world direction to the light local frame
	const Vector localDir = Normalize(Inverse(lightToWorld) * w);

	const auto params = model->computeParameters(
		PragueSkyModel::Vector3(0.0, 0.0, (double)altitude),
		PragueSkyModel::Vector3((double)localDir.x, (double)localDir.y, (double)localDir.z),
		localSunElevation, localSunAzimuth, (double)visibility, (double)groundAlbedo);

	// Sky radiance only: the direct sun disc is provided by the dedicated
	// sun3 light source
	double channelRadiance[64];
	if (channels > sizeof(channelRadiance) / sizeof(channelRadiance[0]))
		throw runtime_error("Unsupported number of spectral channels in the Prague Sky Model dataset: " + ToString(channels));
	for (u_int i = 0; i < channels; ++i) {
		const double wavelength = PragueSkyModelChannelWavelength(i, channelStart, channelWidth);
		channelRadiance[i] = model->skyRadiance(params, wavelength);
	}

	return PragueSkyModelToSpectrum(channelRadiance, channels, channelStart, channelWidth);
}

Spectrum SkyLight3::ComputeRadiance(const Vector &w) const {
	if (hasGround && (Dot(w, absoluteUpDir) < 0.f)) {
		// Lower hemisphere
		return scaledGroundColor;
	} else
		return temperatureScale * gain * ComputeSkyRadiance(w);
}

void SkyLight3::Preprocess() {
	EnvLightSource::Preprocess();

	absoluteSunDir = Normalize(lightToWorld * localSunDir);
	absoluteUpDir = Normalize(lightToWorld * Vector(0.f, 0.f, 1.f));

	// Sun elevation and azimuth in the light local frame, as expected by
	// PragueSkyModel::computeParameters()
	const Vector localSunDirN = Normalize(localSunDir);
	localSunElevation = asin(Clamp((double)localSunDirN.z, -1.0, 1.0));
	localSunAzimuth = atan2((double)localSunDirN.y, (double)localSunDirN.x);

	// Load the dataset (shared between all Prague Sky Model based lights)
	model = GetPragueSkyModel(datasetFile);

	const auto available = model->getAvailableData();
	channels = (u_int)available.channels;
	channelStart = available.channelStart;
	channelWidth = available.channelWidth;

	if (hasGroundAutoScale)
		scaledGroundColor = temperatureScale * gain * ComputeSkyRadiance(absoluteUpDir).Y() * groundColor;
	else
		scaledGroundColor = groundColor;

	isGroundBlack = (hasGround && groundColor.Black());

	//----------------------------------------------------------------------
	// Bake the dome radiance into a lat-long ImageMap. The map is used by
	// OpenCL/CUDA rendering and, as luminance, for the light
	// Distribution2D.
	//----------------------------------------------------------------------

	ImageMapConfig imgMapConfig;
	imgMapConfig.SetStorageType(ImageMapStorage::FLOAT);

	radianceImageMap = ImageMap::AllocImageMap(3, mapWidth, mapHeight, imgMapConfig);

	float *pixels = (float *)radianceImageMap->GetStorage().GetPixelsData();
	vector<float> distributionData(mapWidth * mapHeight);

	// A black ground allows to skip the model evaluation of the lower
	// hemisphere only if the light is not rotated
	const bool skipGroundRows = isGroundBlack && (absoluteUpDir.z > .999f);
	#pragma omp parallel for schedule(static, 1)
	for (int y = 0; y < (int)mapHeight; ++y) {
		const bool belowHorizonRow = (y > (int)(mapHeight / 2));
		// sin(theta) at the row center: the solid angle of a texel is
		// proportional to it (the lat-long mapping is uniform in theta,
		// not in cos(theta))
		const float sinTheta = sinf(M_PI * (y + .5f) / mapHeight);
		for (u_int x = 0; x < mapWidth; ++x) {
			const u_int index = x + y * mapWidth;

			Spectrum radiance;
			if (skipGroundRows && belowHorizonRow) {
				// Black ground: nothing to evaluate
			} else {
				// The texel (x, y) must hold the radiance of the
				// direction mapped to u=(x+.5)/W, v=(y+.5)/H by the
				// lat-long mapping: this is the convention used both by
				// the OpenCL/CUDA kernels (EnvLightSource_ToLatLongMapping
				// + ImageMap_GetSpectrum) and by the CPU lat-long pdf to
				// look the map up (a standard equirectangular mapping,
				// uniform in theta with the zenith in row 0)
				Vector dir;
				FromLatLongMapping(
						(x + .5f) / mapWidth,
						(y + .5f) / mapHeight,
						&dir);
				radiance = ComputeRadiance(dir);
			}

			pixels[index * 3 + 0] = radiance.c[0];
			pixels[index * 3 + 1] = radiance.c[1];
			pixels[index * 3 + 2] = radiance.c[2];

			// Weight the texel luminance by its solid angle so that the
			// Distribution2D pdf (per unit u/v, converted to per solid
			// angle with the lat-long mapping pdf) is proportional to the
			// radiance
			distributionData[index] = radiance.Y() * sinTheta;
		}
	}

	radianceImageMap->Preprocess();

	skyDistribution = std::make_unique<Distribution2D>(distributionData, mapWidth, mapHeight);

	// The mean dome luminance over the sphere, used by GetPower() without
	// any further model evaluation. distributionData[] holds the texel
	// luminance weighted by sin(theta) (i.e. by the texel solid angle), so
	// its sum is the integral of the luminance over the sphere up to the
	// constant factor sin(pi/(2*H))/W
	meanRadianceY = 0.f;
	for (u_int i = 0; i < mapWidth * mapHeight; ++i)
		meanRadianceY += distributionData[i];
	meanRadianceY *= sinf(M_PI / (2.f * mapHeight)) / mapWidth;
}

void SkyLight3::RegisterRadianceImageMap(ImageMapCache &imgMapCache) {
	if (!radianceImageMap)
		throw runtime_error("Sky3 light '" + GetName() +
				"': the radiance image map has not been baked");

	radianceImageMap->SetName("LUXCORE_SKY3_RADIANCE_MAP_" + GetName());

	// Add the image map to the cache
	radianceMap = ImageMapPtr(std::addressof(imgMapCache.DefineImageMap(std::move(radianceImageMap))));
}

std::tuple<luxrays::Distribution2DRef, EnvLightVisibilityCacheRPtr>
SkyLight3::GetPreprocessedData(float *absoluteUpDirData,
		float *scaledGroundColorData, int *hasGroundData) const {
	if (absoluteUpDirData) {
		absoluteUpDirData[0] = absoluteUpDir.x;
		absoluteUpDirData[1] = absoluteUpDir.y;
		absoluteUpDirData[2] = absoluteUpDir.z;
	}

	if (scaledGroundColorData) {
		scaledGroundColorData[0] = scaledGroundColor.c[0];
		scaledGroundColorData[1] = scaledGroundColor.c[1];
		scaledGroundColorData[2] = scaledGroundColor.c[2];
	}

	if (hasGroundData)
		*hasGroundData = hasGround;

	return std::make_tuple(
		std::ref(*skyDistribution), std::ref(visibilityMapCache)
	);
}

float SkyLight3::GetPower(SceneConstRef scene) const {
	const float envRadius = GetEnvRadius(scene);

	// Same value the dome baking loop computed for the light
	// Distribution2D, without any further model evaluation
	return meanRadianceY * (4.f * M_PI * envRadius * envRadius) * 2.f * M_PI;
}

Spectrum SkyLight3::GetRadiance(SceneConstRef scene,
		const BSDF *bsdf, const Vector &dir,
		float *directPdfA, float *emissionPdfW) const {
	const Vector globalDir = -dir;
	float u, v, latLongMappingPdf;
	ToLatLongMapping(globalDir, &u, &v, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return Spectrum();

	const float distPdf = skyDistribution->Pdf(u, v);
	if (directPdfA) {
		if (!bsdf)
			*directPdfA = 0.f;
		else if (visibilityMapCache && visibilityMapCache->IsCacheEnabled(*bsdf)) {
			*directPdfA =  visibilityMapCache->Pdf(*bsdf, u, v) * latLongMappingPdf;
		} else
			*directPdfA = distPdf * latLongMappingPdf;
	}

	if (emissionPdfW) {
		const float envRadius = GetEnvRadius(scene);
		*emissionPdfW = distPdf * latLongMappingPdf / (M_PI * envRadius * envRadius);
	}

	return ComputeRadiance(globalDir);
}

Spectrum SkyLight3::Emit(SceneConstRef scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Ray &ray, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	float uv[2];
	float distPdf;
	skyDistribution->SampleContinuous(u0, u1, uv, &distPdf);
	if (distPdf == 0.f)
		return Spectrum();

	Vector globalDir;
	float latLongMappingPdf;
	FromLatLongMapping(uv[0], uv[1], &globalDir, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return Spectrum();

	// Compute the ray direction
	const Vector rayDir = -globalDir;

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

	if (directPdfA)
		*directPdfA = distPdf * latLongMappingPdf;

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(Normalize(worldCenter - rayOrig), rayDir);

	const Spectrum result = ComputeRadiance(-rayDir);
	assert (!result.IsNaN() && !result.IsInf() && !result.IsNeg());

	ray.Update(rayOrig, rayDir, time);

	return result;
}

Spectrum SkyLight3::Illuminate(SceneConstRef scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
		Ray &shadowRay, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	float uv[2];
	float distPdf;
	if (visibilityMapCache && visibilityMapCache->IsCacheEnabled(bsdf))
		visibilityMapCache->Sample(bsdf, u0, u1, uv, &distPdf);
	else
		skyDistribution->SampleContinuous(u0, u1, uv, &distPdf);
	if (distPdf == 0.f)
		return Spectrum();

	Vector shadowRayDir;
	float latLongMappingPdf;
	FromLatLongMapping(uv[0], uv[1], &shadowRayDir, &latLongMappingPdf);
	if (latLongMappingPdf == 0.f)
		return Spectrum();

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

	if (emissionPdfW)
		*emissionPdfW = distPdf * latLongMappingPdf / (M_PI * envRadius * envRadius);

	shadowRay = Ray(shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return ComputeRadiance(shadowRayDir);
}

UV SkyLight3::GetEnvUV(const luxrays::Vector &dir) const {
	UV uv;
	const Vector w = -dir;
	ToLatLongMapping(w, &uv.u, &uv.v);

	return uv;
}

void SkyLight3::UpdateVisibilityMap(SceneConstRef scene, const bool useRTMode) {
	visibilityMapCache.reset();

	if (useRTMode)
		return;

	if (useVisibilityMapCache) {

		// Build a luminance map of the sky
		ImageMapUPtr luminanceMapImage(ImageMap::AllocImageMap(1,
				EnvLightVisibilityCache::defaultLuminanceMapWidth, EnvLightVisibilityCache::defaultLuminanceMapHeight,
				ImageMapConfig()));

		float *pixels = (float *)luminanceMapImage->GetStorage().GetPixelsData();
		for (u_int y = 0; y < EnvLightVisibilityCache::defaultLuminanceMapHeight; ++y) {
			for (u_int x = 0; x < EnvLightVisibilityCache::defaultLuminanceMapWidth; ++x)
				pixels[x + y * EnvLightVisibilityCache::defaultLuminanceMapWidth] = ComputeRadiance(UniformSampleSphere(
						(y + .5f) / EnvLightVisibilityCache::defaultLuminanceMapHeight,
						(x + .5f) / EnvLightVisibilityCache::defaultLuminanceMapWidth)).Y();
		}

		visibilityMapCache = std::make_unique<EnvLightVisibilityCache>(
			scene, this, std::move(luminanceMapImage), visibilityMapCacheParams
		);
		visibilityMapCache->Build();
	}
}

PropertiesUPtr SkyLight3::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	PropertiesUPtr props = EnvLightSource::ToProperties(imgMapCache, useRealFileName);

	props->Set(Property(prefix + ".type")("sky3"));
	props->Set(Property(prefix + ".dir")(localSunDir));
	props->Set(Property(prefix + ".dataset")(datasetFile));
	props->Set(Property(prefix + ".visibility")(visibility));
	props->Set(Property(prefix + ".altitude")(altitude));
	props->Set(Property(prefix + ".groundalbedo")(groundAlbedo));
	props->Set(Property(prefix + ".ground.enable")(hasGround));
	props->Set(Property(prefix + ".ground.color")(groundColor));
	props->Set(Property(prefix + ".ground.autoscale")(hasGroundAutoScale));
	props->Set(Property(prefix + ".envmap.width")(mapWidth));
	props->Set(Property(prefix + ".envmap.height")(mapHeight));

	props->Set(Property(prefix + ".visibilitymapcache.enable")(useVisibilityMapCache));
	if (useVisibilityMapCache)
		*props << EnvLightVisibilityCache::Params2Props(prefix, visibilityMapCacheParams);

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
