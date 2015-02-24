/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include "slg/lights/skylight.h"
#include "slg/sdl/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SkyLight
//------------------------------------------------------------------------------

static float PerezBase(const float lam[6], const float theta, const float gamma) {
	return (1.f + lam[1] * expf(lam[2] / cosf(theta))) *
		(1.f + lam[3] * expf(lam[4] * gamma)  + lam[5] * cosf(gamma) * cosf(gamma));
}

static float RiAngleBetween(const float thetav, const float phiv,
		const float theta, const float phi) {
	const float cospsi = sinf(thetav) * sinf(theta) * cosf(phi - phiv) + cosf(thetav) * cosf(theta);
	if (cospsi >= 1.f)
		return 0.f;
	if (cospsi <= -1.f)
		return M_PI;
	return acosf(cospsi);
}

static void ChromaticityToSpectrum(float Y, float x, float y, Spectrum *s) {
	float X, Z;
	
	if (y != 0.f)
		X = (x / y) * Y;
	else
		X = 0.f;
	
	if (y != 0.f && Y != 0.f)
		Z = (1.f - x - y) / y * Y;
	else
		Z = 0.f;

	// Assuming sRGB (D65 illuminant)
	s->c[0] =  3.2410f * X - 1.5374f * Y - 0.4986f * Z;
	s->c[1] = -0.9692f * X + 1.8760f * Y + 0.0416f * Z;
	s->c[2] =  0.0556f * X - 0.2040f * Y + 1.0570f * Z;
}

SkyLight::SkyLight() : localSunDir(0.f, 0.f, 1.f), turbidity(2.2f) {
}

SkyLight::~SkyLight() {
}

void SkyLight::Preprocess() {
	absoluteSunDir = Normalize(lightToWorld * localSunDir);

	absoluteTheta = SphericalTheta(absoluteSunDir);
	absolutePhi = SphericalPhi(absoluteSunDir);

	float aconst = 1.f;
	float bconst = 1.f;
	float cconst = 1.f;
	float dconst = 1.f;
	float econst = 1.f;

	float theta2 = absoluteTheta * absoluteTheta;
	float theta3 = theta2 * absoluteTheta;
	float T = turbidity;
	float T2 = T * T;

	float chi = (4.f / 9.f - T / 120.f) * (M_PI - 2.0f * absoluteTheta);
	zenith_Y = (4.0453f * T - 4.9710f) * tanf(chi) - 0.2155f * T + 2.4192f;
	zenith_Y *= 1000;  // conversion from kcd/m^2 to cd/m^2

	zenith_x =
	(0.00166f * theta3 - 0.00375f * theta2 + 0.00209f * absoluteTheta) * T2 +
	(-0.02903f * theta3 + 0.06377f * theta2 - 0.03202f * absoluteTheta + 0.00394f) * T +
	(0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * absoluteTheta + 0.25886f);

	zenith_y =
	(0.00275f * theta3 - 0.00610f * theta2 + 0.00317f * absoluteTheta) * T2 +
	(-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * absoluteTheta  + 0.00516f) * T +
	(0.15346f * theta3 - 0.26756f * theta2 + 0.06670f * absoluteTheta  + 0.26688f);

	perez_Y[1] = (0.1787f * T  - 1.4630f) * aconst;
	perez_Y[2] = (-0.3554f * T  + 0.4275f) * bconst;
	perez_Y[3] = (-0.0227f * T  + 5.3251f) * cconst;
	perez_Y[4] = (0.1206f * T  - 2.5771f) * dconst;
	perez_Y[5] = (-0.0670f * T  + 0.3703f) * econst;

	perez_x[1] = (-0.0193f * T  - 0.2592f) * aconst;
	perez_x[2] = (-0.0665f * T  + 0.0008f) * bconst;
	perez_x[3] = (-0.0004f * T  + 0.2125f) * cconst;
	perez_x[4] = (-0.0641f * T  - 0.8989f) * dconst;
	perez_x[5] = (-0.0033f * T  + 0.0452f) * econst;

	perez_y[1] = (-0.0167f * T  - 0.2608f) * aconst;
	perez_y[2] = (-0.0950f * T  + 0.0092f) * bconst;
	perez_y[3] = (-0.0079f * T  + 0.2102f) * cconst;
	perez_y[4] = (-0.0441f * T  - 1.6537f) * dconst;
	perez_y[5] = (-0.0109f * T  + 0.0529f) * econst;

	zenith_Y /= PerezBase(perez_Y, 0, absoluteTheta);
	zenith_x /= PerezBase(perez_x, 0, absoluteTheta);
	zenith_y /= PerezBase(perez_y, 0, absoluteTheta);
}

void SkyLight::GetPreprocessedData(float *absoluteSunDirData,
	float *absoluteThetaData, float *absolutePhiData,
	float *zenith_YData, float *zenith_xData, float *zenith_yData,
	float *perez_YData, float *perez_xData, float *perez_yData) const {
	if (absoluteSunDirData) {
		absoluteSunDirData[0] = absoluteSunDir.x;
		absoluteSunDirData[1] = absoluteSunDir.y;
		absoluteSunDirData[2] = absoluteSunDir.z;
	}

	if (absoluteThetaData)
		*absoluteThetaData = absoluteTheta;

	if (absolutePhiData)
		*absolutePhiData = absolutePhi;

	if (zenith_YData)
		*zenith_YData = zenith_Y;

	if (zenith_xData)
		*zenith_xData = zenith_x;

	if (zenith_yData)
		*zenith_yData = zenith_y;

	for (size_t i = 0; i < 6; ++i) {
		if (perez_YData)
			perez_YData[i] = perez_Y[i];
		if (perez_xData)
			perez_xData[i] = perez_x[i];
		if (perez_yData)
			perez_yData[i] = perez_y[i];
	}
}

float SkyLight::GetPower(const Scene &scene) const {
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;
	
	const u_int steps = 100;
	const float deltaStep = 2.f / steps;
	float phi = 0.f, power = 0.f;
	for (u_int i = 0; i < steps; ++i) {
		float cosTheta = -1.f;
		for (u_int j = 0; j < steps; ++j) {
			float theta = acosf(cosTheta);
			float gamma = RiAngleBetween(theta, phi, absoluteTheta, absolutePhi);
			theta = Min<float>(theta, M_PI * .5f - .001f);
			power += zenith_Y * PerezBase(perez_Y, theta, gamma);
			cosTheta += deltaStep;
		}

		phi += deltaStep * M_PI;
	}
	power /= steps * steps;

	return gain.Y() * power * (4.f * M_PI * worldRadius * worldRadius) * 2.f * M_PI;
}

void SkyLight::GetSkySpectralRadiance(const float theta, const float phi, Spectrum * const spect) const {
	// Add bottom half of hemisphere with horizon colour
	const float theta_fin = Min<float>(theta, (M_PI * .5f) - .001f);
	const float gamma = RiAngleBetween(theta, phi, absoluteTheta, absolutePhi);

	// Compute xyY values
	const float x = zenith_x * PerezBase(perez_x, theta_fin, gamma);
	const float y = zenith_y * PerezBase(perez_y, theta_fin, gamma);
	const float Y = zenith_Y * PerezBase(perez_Y, theta_fin, gamma);

	ChromaticityToSpectrum(Y, x, y, spect);
}

Spectrum SkyLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	// Choose two points p1 and p2 on scene bounding sphere
	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	Point p1 = worldCenter + worldRadius * UniformSampleSphere(u0, u1);
	Point p2 = worldCenter + worldRadius * UniformSampleSphere(u2, u3);

	// Construct ray between p1 and p2
	*orig = p1;
	*dir = Normalize(lightToWorld * (p2 - p1));

	// Compute SkyLight ray weight
	*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	if (directPdfA)
		*directPdfA = 1.f / (4.f * M_PI);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(Normalize(worldCenter -  p1), *dir);

	return GetRadiance(scene, *dir);
}

Spectrum SkyLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	*dir = Normalize(lightToWorld * UniformSampleSphere(u0, u1));

	const Vector toCenter(worldCenter - p);
	const float centerDistance = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, *dir);
	*distance = approach + sqrtf(Max(0.f, worldRadius * worldRadius -
		centerDistance + approach * approach));

	const Point emisPoint(p + (*distance) * (*dir));
	const Normal emisNormal(Normalize(worldCenter - emisPoint));

	const float cosAtLight = Dot(emisNormal, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();
	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	*directPdfW = 1.f / (4.f * M_PI);

	if (emissionPdfW)
		*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	return GetRadiance(scene, -(*dir));
}

Spectrum SkyLight::GetRadiance(const Scene &scene,
		const Vector &dir,
		float *directPdfA,
		float *emissionPdfW) const {
	const float theta = SphericalTheta(-dir);
	const float phi = SphericalPhi(-dir);
	Spectrum s;
	GetSkySpectralRadiance(theta, phi, &s);

	if (directPdfA)
		*directPdfA = 1.f / (4.f * M_PI);

	if (emissionPdfW) {
		const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;
		*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);
	}

	return gain * s;
}

Properties SkyLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = EnvLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("sky"));
	props.Set(Property(prefix + ".dir")(localSunDir));
	props.Set(Property(prefix + ".turbidity")(turbidity));

	return props;
}
