/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include "luxrays/core/geometry/frame.h"
#include "luxrays/utils/sdl/light.h"
#include "luxrays/utils/sdl/spd.h"
#include "luxrays/utils/sdl/data/sun_spect.h"
#include "luxrays/utils/sdl/scene.h"

using namespace luxrays;
using namespace luxrays::sdl;

//------------------------------------------------------------------------------
// InfiniteLightBase
//------------------------------------------------------------------------------

Spectrum InfiniteLightBase::Emit(const Scene *scene,
		const float u0, const float u1, const float u2, const float u3,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	// Choose two points p1 and p2 on scene bounding sphere
	const Point worldCenter = scene->dataSet->GetBSphere().center;
	const float worldRadius = scene->dataSet->GetBSphere().rad * 1.01f;

	Point p1 = worldCenter + worldRadius * UniformSampleSphere(u0, u1);
	Point p2 = worldCenter + worldRadius * UniformSampleSphere(u2, u3);

	// Construct ray between p1 and p2
	*orig = p1;
	*dir = Normalize(p2 - p1);

	// Compute InfiniteAreaLight ray weight
	*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	if (directPdfA)
		*directPdfA = 1.f / (4.f * M_PI);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(Normalize(worldCenter -  p1), *dir);

	return GetRadiance(scene, *dir, p2);
}

Spectrum InfiniteLightBase::Illuminate(const Scene *scene, const Point &p,
		const float u0, const float u1, const float u2,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Point worldCenter = scene->dataSet->GetBSphere().center;
	const float worldRadius = scene->dataSet->GetBSphere().rad * 1.01f;

	*dir = UniformSampleSphere(u0, u1);

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

	return GetRadiance(scene, -(*dir), emisPoint);
}

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

InfiniteLight::InfiniteLight(TexMapInstance *tx) {
	tex = tx;
	shiftU = 0.f;
	shiftV = 0.f;
}

Spectrum InfiniteLight::GetRadiance(const Scene *scene,
		const Vector &dir,
		const Point &hitPoint,
		float *directPdfA,
		float *emissionPdfW) const {
	if (directPdfA)
		*directPdfA = 1.f / (4.f * M_PI);

	if (emissionPdfW) {
		const float worldRadius = scene->dataSet->GetBSphere().rad * 1.01f;
		*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);
	}

	const UV uv(1.f - SphericalPhi(-dir) * INV_TWOPI + shiftU, SphericalTheta(-dir) * INV_PI + shiftV);
	return gain * tex->GetColor(uv);
}

//------------------------------------------------------------------------------
// SkyLight
//------------------------------------------------------------------------------

static float PerezBase(const float lam[6], float theta, float gamma) {
	return (1.f + lam[1] * expf(lam[2] / cosf(theta))) *
		(1.f + lam[3] * expf(lam[4] * gamma)  + lam[5] * cosf(gamma) * cosf(gamma));
}

static inline float RiAngleBetween(float thetav, float phiv, float theta, float phi) {
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
	s->r =  3.2410f * X - 1.5374f * Y - 0.4986f * Z;
	s->g = -0.9692f * X + 1.8760f * Y + 0.0416f * Z;
	s->b =  0.0556f * X - 0.2040f * Y + 1.0570f * Z;
}

SkyLight::SkyLight(float turb, const Vector &sd) {
	turbidity = turb;
	sundir = Normalize(sd);
}

void SkyLight::Preprocess() {
	thetaS = SphericalTheta(sundir);
	phiS = SphericalPhi(sundir);

	float aconst = 1.0f;
	float bconst = 1.0f;
	float cconst = 1.0f;
	float dconst = 1.0f;
	float econst = 1.0f;

	float theta2 = thetaS*thetaS;
	float theta3 = theta2*thetaS;
	float T = turbidity;
	float T2 = T * T;

	float chi = (4.f / 9.f - T / 120.f) * (M_PI - 2.0f * thetaS);
	zenith_Y = (4.0453f * T - 4.9710f) * tan(chi) - 0.2155f * T + 2.4192f;
	zenith_Y *= 0.06f;

	zenith_x =
	(0.00166f * theta3 - 0.00375f * theta2 + 0.00209f * thetaS) * T2 +
	(-0.02903f * theta3 + 0.06377f * theta2 - 0.03202f * thetaS + 0.00394f) * T +
	(0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * thetaS + 0.25886f);

	zenith_y =
	(0.00275f * theta3 - 0.00610f * theta2 + 0.00317f * thetaS) * T2 +
	(-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * thetaS  + 0.00516f) * T +
	(0.15346f * theta3 - 0.26756f * theta2 + 0.06670f * thetaS  + 0.26688f);

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

	zenith_Y /= PerezBase(perez_Y, 0, thetaS);
	zenith_x /= PerezBase(perez_x, 0, thetaS);
	zenith_y /= PerezBase(perez_y, 0, thetaS);
}

void SkyLight::GetSkySpectralRadiance(const float theta, const float phi, Spectrum * const spect) const {
	// Add bottom half of hemisphere with horizon colour
	const float theta_fin = Min<float>(theta, (M_PI * .5f) - .001f);
	const float gamma = RiAngleBetween(theta, phi, thetaS, phiS);

	// Compute xyY values
	const float x = zenith_x * PerezBase(perez_x, theta_fin, gamma);
	const float y = zenith_y * PerezBase(perez_y, theta_fin, gamma);
	const float Y = zenith_Y * PerezBase(perez_Y, theta_fin, gamma);

	ChromaticityToSpectrum(Y, x, y, spect);
}

Spectrum SkyLight::GetRadiance(const Scene *scene,
		const Vector &dir,
		const Point &hitPoint,
		float *directPdfA,
		float *emissionPdfW) const {
	const float theta = SphericalTheta(-dir);
	const float phi = SphericalPhi(-dir);
	Spectrum s;
	GetSkySpectralRadiance(theta, phi, &s);

	if (directPdfA)
		*directPdfA = INV_PI * .25f;

	if (emissionPdfW) {
		const float worldRadius = scene->dataSet->GetBSphere().rad * 1.01f;
		*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);
	}

	return gain * s;
}

//------------------------------------------------------------------------------
// SunLight
//------------------------------------------------------------------------------

SunLight::SunLight(float turb, float size, const Vector &sd) : LightSource() {
	turbidity = turb;
	sunDir = Normalize(sd);
	gain = Spectrum(1.0f, 1.0f, 1.0f);
	relSize = size;
}

void SunLight::Preprocess() {
	CoordinateSystem(sunDir, &x, &y);

	// Values from NASA Solar System Exploration page
	// http://solarsystem.nasa.gov/planets/profile.cfm?Object=Sun&Display=Facts&System=Metric
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

	Vector wh = Normalize(sunDir);
	phiS = SphericalPhi(wh);
	thetaS = SphericalTheta(wh);

	// NOTE - lordcrc - sun_k_oWavelengths contains 64 elements, while sun_k_oAmplitudes contains 65?!?
	IrregularSPD k_oCurve(sun_k_oWavelengths, sun_k_oAmplitudes, 64);
	IrregularSPD k_gCurve(sun_k_gWavelengths, sun_k_gAmplitudes, 4);
	IrregularSPD k_waCurve(sun_k_waWavelengths, sun_k_waAmplitudes, 13);

	RegularSPD solCurve(sun_solAmplitudes, 380, 750, 38);  // every 5 nm

	float beta = 0.04608365822050f * turbidity - 0.04586025928522f;
	float tauR, tauA, tauO, tauG, tauWA;

	float m = 1.f / (cosf(thetaS) + 0.00094f * powf(1.6386f - thetaS, 
		-1.253f));  // Relative Optical Mass

	int i;
	float lambda;
	// NOTE - lordcrc - SPD stores data internally, no need for Ldata to stick around
	float Ldata[91];
	for(i = 0, lambda = 350.f; i < 91; ++i, lambda += 5.f) {
			// Rayleigh Scattering
		tauR = expf( -m * 0.008735f * powf(lambda / 1000.f, -4.08f));
			// Aerosol (water + dust) attenuation
			// beta - amount of aerosols present
			// alpha - ratio of small to large particle sizes. (0:4,usually 1.3)
		const float alpha = 1.3f;
		tauA = expf(-m * beta * powf(lambda / 1000.f, -alpha));  // lambda should be in um
			// Attenuation due to ozone absorption
			// lOzone - amount of ozone in cm(NTP)
		const float lOzone = .35f;
		tauO = expf(-m * k_oCurve.sample(lambda) * lOzone);
			// Attenuation due to mixed gases absorption
		tauG = expf(-1.41f * k_gCurve.sample(lambda) * m / powf(1.f +
			118.93f * k_gCurve.sample(lambda) * m, 0.45f));
			// Attenuation due to water vapor absorbtion
			// w - precipitable water vapor in centimeters (standard = 2)
		const float w = 2.0f;
		tauWA = expf(-0.2385f * k_waCurve.sample(lambda) * w * m /
		powf(1.f + 20.07f * k_waCurve.sample(lambda) * w * m, 0.45f));

		Ldata[i] = (solCurve.sample(lambda) *
			tauR * tauA * tauO * tauG * tauWA);
	}

	RegularSPD LSPD(Ldata, 350,800,91);
	// Note: (1000000000.0f / (M_PI * 100.f * 100.f)) is for compatibility with past scene
	sunColor = gain * LSPD.ToRGB() / (1000000000.0f / (M_PI * 100.f * 100.f));
}

void SunLight::SetGain(const Spectrum &g) {
	gain = g;
}

Spectrum SunLight::Emit(const Scene *scene,
		const float u0, const float u1, const float u2, const float u3,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	const Point worldCenter = scene->dataSet->GetBSphere().center;
	const float worldRadius = scene->dataSet->GetBSphere().rad * 1.01f;

	// Set ray origin and direction for infinite light ray
	float d1, d2;
	ConcentricSampleDisk(u0, u1, &d1, &d2);
	*orig = worldCenter + worldRadius * (sunDir + d1 * x + d2 * y);
	*dir = -UniformSampleCone(u2, u3, cosThetaMax, x, y, sunDir);
	*emissionPdfW = UniformConePdf(cosThetaMax) / (M_PI * worldRadius * worldRadius);

	if (directPdfA)
		*directPdfA = UniformConePdf(cosThetaMax);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(sunDir, -(*dir));

	return sunColor;
}

Spectrum SunLight::Illuminate(const Scene *scene, const Point &p,
		const float u0, const float u1, const float u2,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	*dir = UniformSampleCone(u0, u1, cosThetaMax, x, y, sunDir);

	// Check if the point can be inside the sun cone of light
	const float cosAtLight = Dot(sunDir, *dir);
	if (cosAtLight <= cosThetaMax)
		return Spectrum();

	*distance = std::numeric_limits<float>::infinity();
	*directPdfW = UniformConePdf(cosThetaMax);

	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	if (emissionPdfW) {
		const float worldRadius = scene->dataSet->GetBSphere().rad * 1.01f;
		*emissionPdfW = UniformConePdf(cosThetaMax) / (M_PI * worldRadius * worldRadius);
	}
	
	return sunColor;
}

Spectrum SunLight::GetRadiance(const Scene *scene,
		const Vector &dir,
		const Point &hitPoint,
		float *directPdfA,
		float *emissionPdfW) const {
	// Make the sun visible only if relsize has been changed (in order
	// to avoid fireflies).
	if (relSize > 5.f) {
		if ((cosThetaMax < 1.f) && (Dot(-dir, sunDir) > cosThetaMax)) {
			if (directPdfA)
				*directPdfA = UniformConePdf(cosThetaMax);

			if (emissionPdfW) {
				const float worldRadius = scene->dataSet->GetBSphere().rad * 1.01f;
				*emissionPdfW = UniformConePdf(cosThetaMax) / (M_PI * worldRadius * worldRadius);
			}

			return sunColor;
		}
	}

	return Spectrum();
}

//------------------------------------------------------------------------------
// Triangle Area Light
//------------------------------------------------------------------------------

TriangleLight::TriangleLight(const Material *mat, const unsigned int mshIndex,
		const unsigned int triangleIndex, const std::vector<ExtMesh *> &objs) {
	lightMaterial = mat;
	meshIndex = mshIndex;
	triIndex = triangleIndex;

	Init(objs);
}

void TriangleLight::Init(const std::vector<ExtMesh *> &objs) {
	const ExtMesh *mesh = objs[meshIndex];
	area = mesh->GetTriangleArea(triIndex);
	invArea = 1.f / area;
}

Spectrum TriangleLight::Emit(const Scene *scene,
		const float u0, const float u1, const float u2, const float u3,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	const ExtMesh *mesh = scene->objects[meshIndex];

	// Origin
	float b0, b1, b2;
	mesh->Sample(triIndex, u0, u1, orig, &b0, &b1, &b2);

	// Build the local frame
	const Normal N = mesh->GetGeometryNormal(triIndex); // Light sources are supposed to be flat
	Frame frame(N);

	Vector localDirOut = CosineSampleHemisphere(u2, u3, emissionPdfW);
	if (*emissionPdfW == 0.f)
		return Spectrum();
	*emissionPdfW *= invArea;

	// Cannot really not emit the particle, so just bias it to the correct angle
	localDirOut.z = Max(localDirOut.z, DEFAULT_COS_EPSILON_STATIC);

	// Direction
	*dir = frame.ToWorld(localDirOut);

	if (directPdfA)
		*directPdfA = invArea;

	if (cosThetaAtLight)
		*cosThetaAtLight = localDirOut.z;

	return lightMaterial->GetEmittedRadiance() * localDirOut.z;
}

Spectrum TriangleLight::Illuminate(const Scene *scene, const Point &p,
		const float u0, const float u1, const float u2,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const ExtMesh *mesh = scene->objects[meshIndex];

	Point samplePoint;
	float b0, b1, b2;
	mesh->Sample(triIndex, u0, u1, &samplePoint, &b0, &b1, &b2);
	const Normal &sampleN = mesh->GetGeometryNormal(triIndex); // Light sources are supposed to be flat

	*dir = samplePoint - p;
	const float distanceSquared = dir->LengthSquared();
	*distance = sqrtf(distanceSquared);
	*dir /= (*distance);

	const float cosAtLight = Dot(sampleN, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	*directPdfW = invArea * distanceSquared / cosAtLight;

	if (emissionPdfW)
		*emissionPdfW = invArea * cosAtLight * INV_PI;

	return lightMaterial->GetEmittedRadiance();
}

Spectrum TriangleLight::GetRadiance(const Scene *scene,
		const Vector &dir,
		const Point &hitPoint,
		float *directPdfA,
		float *emissionPdfW) const {
	const ExtMesh *mesh = scene->objects[meshIndex];

	// Get the u and v coordinates of the hit point
	float b1, b2;
	if (!mesh->GetTriUV(triIndex, hitPoint, &b1, &b2))
		return Spectrum();

	const Normal geometryN = mesh->GetGeometryNormal(triIndex); // Light sources are supposed to be flat

	const float cosOutLight = Dot(geometryN, dir);
	if (cosOutLight <= 0.f)
		return Spectrum();

	if (directPdfA)
		*directPdfA = invArea;

	if (emissionPdfW)
		*emissionPdfW = cosOutLight * INV_PI * invArea;

	return lightMaterial->GetEmittedRadiance();
}
