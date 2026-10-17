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
#include "slg/utils/filenameresolver.h"

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

//------------------------------------------------------------------------------
// CIE 1931 2 degree standard observer color matching functions (x, y, z),
// from 360nm to 830nm in 5nm steps. Used, as in the reference implementation
// of the model, to convert the spectral radiance to RGB.
//------------------------------------------------------------------------------

static const float CIE_1931_2DEG[95][3] = {
{0.000129900000, 0.000003917000, 0.000606100000},
{0.000232100000, 0.000006965000, 0.001086000000},
{0.000414900000, 0.000012390000, 0.001946000000},
{0.000741600000, 0.000022020000, 0.003486000000},
{0.001368000000, 0.000039000000, 0.006450001000},
{0.002236000000, 0.000064000000, 0.010549990000},
{0.004243000000, 0.000120000000, 0.020050010000},
{0.007650000000, 0.000217000000, 0.036210000000},
{0.014310000000, 0.000396000000, 0.067850010000},
{0.023190000000, 0.000640000000, 0.110200000000},
{0.043510000000, 0.001210000000, 0.207400000000},
{0.077630000000, 0.002180000000, 0.371300000000},
{0.134380000000, 0.004000000000, 0.645600000000},
{0.214770000000, 0.007300000000, 1.039050100000},
{0.283900000000, 0.011600000000, 1.385600000000},
{0.328500000000, 0.016840000000, 1.622960000000},
{0.348280000000, 0.023000000000, 1.747060000000},
{0.348060000000, 0.029800000000, 1.782600000000},
{0.336200000000, 0.038000000000, 1.772110000000},
{0.318700000000, 0.048000000000, 1.744100000000},
{0.290800000000, 0.060000000000, 1.669200000000},
{0.251100000000, 0.073900000000, 1.528100000000},
{0.195360000000, 0.090980000000, 1.287640000000},
{0.142100000000, 0.112600000000, 1.041900000000},
{0.095640000000, 0.139020000000, 0.812950100000},
{0.057950010000, 0.169300000000, 0.616200000000},
{0.032010000000, 0.208020000000, 0.465180000000},
{0.014700000000, 0.258600000000, 0.353300000000},
{0.004900000000, 0.323000000000, 0.272000000000},
{0.002400000000, 0.407300000000, 0.212300000000},
{0.009300000000, 0.503000000000, 0.158200000000},
{0.029100000000, 0.608200000000, 0.111700000000},
{0.063270000000, 0.710000000000, 0.078249990000},
{0.109600000000, 0.793200000000, 0.057250010000},
{0.165500000000, 0.862000000000, 0.042160000000},
{0.225749900000, 0.914850100000, 0.029840000000},
{0.290400000000, 0.954000000000, 0.020300000000},
{0.359700000000, 0.980300000000, 0.013400000000},
{0.433449900000, 0.994950100000, 0.008749999000},
{0.512050100000, 1.000000000000, 0.005749999000},
{0.594500000000, 0.995000000000, 0.003900000000},
{0.678400000000, 0.978600000000, 0.002749999000},
{0.762100000000, 0.952000000000, 0.002100000000},
{0.842500000000, 0.915400000000, 0.001800000000},
{0.916300000000, 0.870000000000, 0.001650001000},
{0.978600000000, 0.816300000000, 0.001400000000},
{1.026300000000, 0.757000000000, 0.001100000000},
{1.056700000000, 0.694900000000, 0.001000000000},
{1.062200000000, 0.631000000000, 0.000800000000},
{1.045600000000, 0.566800000000, 0.000600000000},
{1.002600000000, 0.503000000000, 0.000340000000},
{0.938400000000, 0.441200000000, 0.000240000000},
{0.854449900000, 0.381000000000, 0.000190000000},
{0.751400000000, 0.321000000000, 0.000100000000},
{0.642400000000, 0.265000000000, 0.000049999990},
{0.541900000000, 0.217000000000, 0.000030000000},
{0.447900000000, 0.175000000000, 0.000020000000},
{0.360800000000, 0.138200000000, 0.000010000000},
{0.283500000000, 0.107000000000, 0.000000000000},
{0.218700000000, 0.081600000000, 0.000000000000},
{0.164900000000, 0.061000000000, 0.000000000000},
{0.121200000000, 0.044580000000, 0.000000000000},
{0.087400000000, 0.032000000000, 0.000000000000},
{0.063600000000, 0.023200000000, 0.000000000000},
{0.046770000000, 0.017000000000, 0.000000000000},
{0.032900000000, 0.011920000000, 0.000000000000},
{0.022700000000, 0.008210000000, 0.000000000000},
{0.015840000000, 0.005723000000, 0.000000000000},
{0.011359160000, 0.004102000000, 0.000000000000},
{0.008110916000, 0.002929000000, 0.000000000000},
{0.005790346000, 0.002091000000, 0.000000000000},
{0.004109457000, 0.001484000000, 0.000000000000},
{0.002899327000, 0.001047000000, 0.000000000000},
{0.002049190000, 0.000740000000, 0.000000000000},
{0.001439971000, 0.000520000000, 0.000000000000},
{0.000999949300, 0.000361100000, 0.000000000000},
{0.000690078600, 0.000249200000, 0.000000000000},
{0.000476021300, 0.000171900000, 0.000000000000},
{0.000332301100, 0.000120000000, 0.000000000000},
{0.000234826100, 0.000084800000, 0.000000000000},
{0.000166150500, 0.000060000000, 0.000000000000},
{0.000117413000, 0.000042400000, 0.000000000000},
{0.000083075270, 0.000030000000, 0.000000000000},
{0.000058706520, 0.000021200000, 0.000000000000},
{0.000041509940, 0.000014990000, 0.000000000000},
{0.000029353260, 0.000010600000, 0.000000000000},
{0.000020673830, 0.000007465700, 0.000000000000},
{0.000014559770, 0.000005257800, 0.000000000000},
{0.000010253980, 0.000003702900, 0.000000000000},
{0.000007221456, 0.000002607800, 0.000000000000},
{0.000005085868, 0.000001836600, 0.000000000000},
{0.000003581652, 0.000001293400, 0.000000000000},
{0.000002522525, 0.000000910930, 0.000000000000},
{0.000001776509, 0.000000641530, 0.000000000000},
{0.000001251141, 0.000000451810, 0.000000000000}
};
static const int CIE_1931_2DEG_START = 360;
static const int CIE_1931_2DEG_STEP = 5;

static Spectrum ComputeSpectrumFromModel(const PragueSkyModel &model,
		const PragueSkyModel::Parameters &params,
		const u_int channels, const double channelStart, const double channelWidth) {
	// Spectrum to XYZ
	double X = 0.0, Y = 0.0, Z = 0.0;
	for (u_int i = 0; i < channels; ++i) {
		const double wavelength = channelStart + i * channelWidth;
		const double radiance = model.skyRadiance(params, wavelength) +
				model.sunRadiance(params, wavelength);

		const int responseIndex = int((wavelength - CIE_1931_2DEG_START) / CIE_1931_2DEG_STEP);
		if ((responseIndex >= 0) &&
				(responseIndex < (int)(sizeof(CIE_1931_2DEG) / sizeof(CIE_1931_2DEG[0])))) {
			X += CIE_1931_2DEG[responseIndex][0] * radiance;
			Y += CIE_1931_2DEG[responseIndex][1] * radiance;
			Z += CIE_1931_2DEG[responseIndex][2] * radiance;
		}
	}
	X *= channelWidth;
	Y *= channelWidth;
	Z *= channelWidth;

	// XYZ to sRGB
	const double rgb[3] = {
		 3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z,
		-0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z,
		 0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z
	};

	// Negative values (out of gamut spectral colors) are clamped to 0.
	//
	// 683 is a scaling factor to convert W to lm
	return Spectrum(Max(0.f, (float)rgb[0]),
			Max(0.f, (float)rgb[1]), Max(0.f, (float)rgb[2])) * 683.f;
}

SkyLight3::SkyLight3() : localSunDir(0.f, 0.f, 1.f), visibility(59.4f),
		altitude(0.f), groundAlbedo(0.f), groundColor(0.f, 0.f, 0.f),
		hasGround(false), hasGroundAutoScale(true),
		mapWidth(2048), mapHeight(1024),
		useVisibilityMapCache(false), radianceMap(nullptr),
		model(std::make_unique<PragueSkyModel>()),
		localSunElevation(0.0), localSunAzimuth(0.0),
		channels(0), channelStart(0.0), channelWidth(0.0),
		isGroundBlack(false),
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

	// Sky radiance + direct sun radiance
	return ComputeSpectrumFromModel(*model, params, channels, channelStart, channelWidth);
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

	// Load the dataset. It is done only once because a light source is
	// re-created, and not updated, when its properties are edited.
	if (!model->isInitialized()) {
		const string resolvedFileName = SLG_FileNameResolver.ResolveFile(datasetFile);
		SDL_LOG("Sky3 light '" << GetName() << "': loading Prague Sky Model dataset " <<
				resolvedFileName);
		model->initialize(resolvedFileName);
	}

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
	// Bake the dome radiance (direct sun included) into a lat-long
	// ImageMap. The map is used by OpenCL/CUDA rendering and, as
	// luminance, for the light Distribution2D.
	//----------------------------------------------------------------------

	ImageMapConfig imgMapConfig;
	// The direct sun radiance is several orders of magnitude above the sky
	// one: half float storage is not enough
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
		for (u_int x = 0; x < mapWidth; ++x) {
			const u_int index = x + y * mapWidth;

			Spectrum radiance;
			if (skipGroundRows && belowHorizonRow) {
				// Black ground: nothing to evaluate
			} else
				radiance = ComputeRadiance(UniformSampleSphere(
						(y + .5f) / mapHeight,
						(x + .5f) / mapWidth));

			pixels[index * 3 + 0] = radiance.c[0];
			pixels[index * 3 + 1] = radiance.c[1];
			pixels[index * 3 + 2] = radiance.c[2];

			distributionData[index] = radiance.Y();
		}
	}

	radianceImageMap->Preprocess();

	skyDistribution = std::make_unique<Distribution2D>(distributionData, mapWidth, mapHeight);
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

	float power = 0.f;
	for (u_int y = 0; y < mapHeight; ++y) {
		for (u_int x = 0; x < mapWidth; ++x)
			power += ComputeRadiance(UniformSampleSphere(
					(y + .5f) / mapHeight,
					(x + .5f) / mapWidth)).Y();
	}
	power /= mapWidth * mapHeight;

	return power * (4.f * M_PI * envRadius * envRadius) * 2.f * M_PI;
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
