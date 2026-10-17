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
#include "slg/lights/sun3light.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SunLight3
//------------------------------------------------------------------------------

SunLight3::SunLight3() : color(1.f), datasetFile("SkyModelDataset.dat"),
		localSunDir(0.f, 0.f, 1.f), visibility(59.4f), altitude(0.f),
		groundAlbedo(0.f), relSize(1.f),
		channels(0), channelStart(0.0), channelWidth(0.0),
		cosThetaMax(1.f), sin2ThetaMax(0.f) {
}

void SunLight3::Preprocess() {
	EnvLightSource::Preprocess();

	absoluteSunDir = Normalize(lightToWorld * localSunDir);
	CoordinateSystem(absoluteSunDir, &x, &y);

	// Same sun angular size computation as the classic sun light source: the
	// result matches the sun angular radius assumed by the Prague Sky Model
	// (SUN_RADIUS = 0.004654793 rad ~ 0.2667 degrees)
	const float sunRadius = 695500.f;
	const float sunMeanDistance = 149600000.f;

	const float sunSize = relSize * sunRadius;
	if (sunSize <= sunMeanDistance) {
		sin2ThetaMax = sunSize / sunMeanDistance;
		sin2ThetaMax *= sin2ThetaMax;
		cosThetaMax = sqrtf(1.f - sin2ThetaMax);
	} else {
		cosThetaMax = 0.f;
		sin2ThetaMax = 1.f;
	}

	// Load the dataset (shared between all Prague Sky Model based lights)
	model = GetPragueSkyModel(datasetFile);

	const auto available = model->getAvailableData();
	channels = (u_int)available.channels;
	channelStart = available.channelStart;
	channelWidth = available.channelWidth;

	// Sun elevation and azimuth in the light local frame, as expected by
	// PragueSkyModel::computeParameters()
	const Vector localSunDirN = Normalize(localSunDir);
	const double localSunElevation = asin(Clamp((double)localSunDirN.z, -1.0, 1.0));
	const double localSunAzimuth = atan2((double)localSunDirN.y, (double)localSunDirN.x);

	// The model assumes a frame with the Z axis pointing up and the observer
	// at the origin: look at the center of the sun disc
	const auto params = model->computeParameters(
		PragueSkyModel::Vector3(0.0, 0.0, (double)altitude),
		PragueSkyModel::Vector3((double)localSunDirN.x, (double)localSunDirN.y, (double)localSunDirN.z),
		localSunElevation, localSunAzimuth, (double)visibility, (double)groundAlbedo);

	// Evaluate the sun radiance (disc radiance after atmospheric
	// transmittance, constant over the disc as in the model) at each
	// spectral channel and convert it to RGB
	vector<double> channelRadiance(channels);
	for (u_int i = 0; i < channels; ++i)
		channelRadiance[i] = model->sunRadiance(params,
				PragueSkyModelChannelWavelength(i, channelStart, channelWidth));

	// The radiance is divided by relSize^2 in order to keep the emitted power
	// constant when the size of the disc is changed (as in the classic sun
	// light source)
	color = (temperatureScale * gain / (relSize * relSize)) *
			PragueSkyModelToSpectrum(&channelRadiance[0], channels, channelStart, channelWidth);
}

void SunLight3::GetPreprocessedData(float *absoluteSunDirData,
		float *xData, float *yData,
		float *cosThetaMaxData, float *sin2ThetaMaxData) const {
	if (absoluteSunDirData) {
		absoluteSunDirData[0] = absoluteSunDir.x;
		absoluteSunDirData[1] = absoluteSunDir.y;
		absoluteSunDirData[2] = absoluteSunDir.z;
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

	if (cosThetaMaxData)
		*cosThetaMaxData = cosThetaMax;

	if (sin2ThetaMaxData)
		*sin2ThetaMaxData = sin2ThetaMax;
}

float SunLight3::GetPower(SceneConstRef scene) const {
	const float envRadius = GetEnvRadius(scene);

	return color.Y() * (M_PI * envRadius * envRadius) * 2.f * M_PI * sin2ThetaMax;
}

Spectrum SunLight3::Emit(SceneConstRef scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Ray &ray, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	const Point worldCenter = scene.GetDataSet().GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	// Set ray origin and direction for infinite light ray
	float d1, d2;
	ConcentricSampleDisk(u0, u1, &d1, &d2);
	const Point rayOrig = worldCenter + envRadius * (absoluteSunDir + d1 * x + d2 * y);
	const Vector rayDir = -UniformSampleCone(u2, u3, cosThetaMax, x, y, absoluteSunDir);

	const float uniformConePdf = UniformConePdf(cosThetaMax);
	emissionPdfW = uniformConePdf / (M_PI * envRadius * envRadius);

	if (directPdfA)
		*directPdfA = uniformConePdf;

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(absoluteSunDir, -rayDir);

	ray.Update(rayOrig, rayDir, time);

	return color;
}

Spectrum SunLight3::Illuminate(SceneConstRef scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        Ray &shadowRay, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Vector shadowRayDir = UniformSampleCone(u0, u1, cosThetaMax, x, y, absoluteSunDir);

	// Check if the point can be inside the sun cone of light
	const float cosAtLight = Dot(absoluteSunDir, shadowRayDir);
	if (cosAtLight <= cosThetaMax)
		return Spectrum();

	const Point worldCenter = scene.GetDataSet().GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	const Point shadowRayOrig = bsdf.GetRayOrigin(shadowRayDir);
	const Vector toCenter(worldCenter - shadowRayOrig);
	const float centerDistanceSquared = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, shadowRayDir);
	const float shadowRayDistance = approach + sqrtf(Max(0.f, envRadius * envRadius -
		centerDistanceSquared + approach * approach));

	const float uniformConePdf = UniformConePdf(cosThetaMax);
	directPdfW = uniformConePdf;

	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	if (emissionPdfW)
		*emissionPdfW =  uniformConePdf / (M_PI * envRadius * envRadius);

	shadowRay = Ray(shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return color;
}

Spectrum SunLight3::GetRadiance(SceneConstRef scene,
		const BSDF *bsdf, const Vector &dir,
		float *directPdfA, float *emissionPdfW) const {
	const float xD = Dot(-dir, x);
	const float yD = Dot(-dir, y);
	const float zD = Dot(-dir, absoluteSunDir);
	if ((cosThetaMax == 1.f) || (zD < 0.f) || ((xD * xD + yD * yD) > sin2ThetaMax))
		return Spectrum();

	const float uniformConePdf = UniformConePdf(cosThetaMax);
	if (directPdfA)
		*directPdfA = uniformConePdf;

	if (emissionPdfW) {
		const float envRadius = GetEnvRadius(scene);
		*emissionPdfW = uniformConePdf / (M_PI * envRadius * envRadius);
	}

	return color;
}

PropertiesUPtr SunLight3::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();
	PropertiesUPtr props = EnvLightSource::ToProperties(imgMapCache, useRealFileName);

	props->Set(Property(prefix + ".type")("sun3"));
	props->Set(Property(prefix + ".dir")(localSunDir));
	props->Set(Property(prefix + ".dataset")(datasetFile));
	props->Set(Property(prefix + ".visibility")(visibility));
	props->Set(Property(prefix + ".altitude")(altitude));
	props->Set(Property(prefix + ".groundalbedo")(groundAlbedo));
	props->Set(Property(prefix + ".relsize")(relSize));

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
