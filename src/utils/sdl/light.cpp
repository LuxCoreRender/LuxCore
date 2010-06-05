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

#include "luxrays/utils/sdl/light.h"
#include "luxrays/utils/sdl/spd.h"
#include "luxrays/utils/sdl/data/sun_spect.h"

using namespace luxrays;
using namespace luxrays::sdl;

//------------------------------------------------------------------------------
// SkyLight
//------------------------------------------------------------------------------

float PerezBase(const float lam[6], float theta, float gamma) {
	return (1.f + lam[1] * expf(lam[2] / cosf(theta))) *
		(1.f + lam[3] * expf(lam[4] * gamma)  + lam[5] * cosf(gamma) * cosf(gamma));
}

inline float RiAngleBetween(float thetav, float phiv, float theta, float phi) {
	const float cospsi = sinf(thetav) * sinf(theta) * cosf(phi - phiv) + cosf(thetav) * cosf(theta);
	if (cospsi >= 1.f)
		return 0.f;
	if (cospsi <= -1.f)
		return M_PI;
	return acosf(cospsi);
}

void ChromaticityToSpectrum(float Y, float x, float y, Spectrum *s) {
	float X, Z;
	
	if (y != 0.0)
		X = (x / y) * Y;
	else
		X = 0.0;
	
	if (y != 0.0 && Y != 0.0)
		Z = (1.0 - x - y) / y * Y;
	else
		Z = 0.0;

	// Assuming sRGB (D65 illuminant)
	s->r =  3.2410 * X - 1.5374 * Y - 0.4986 * Z;
	s->g = -0.9692 * X + 1.8760 * Y + 0.0416 * Z;
	s->b =  0.0556 * X - 0.2040 * Y + 1.0570 * Z;
}

SkyLight::SkyLight(float turb, const Vector &sd) : InfiniteLight(NULL) {
	turbidity = turb;
	sundir = Normalize(sd);
	gain = Spectrum(1.0f, 1.0f, 1.0f);
	
	thetaS = SphericalTheta(sundir);
	phiS = SphericalPhi(sundir);

	float aconst = 1.0f;
	float bconst = 1.0f;
	float cconst = 1.0f;
	float dconst = 1.0f;
	float econst = 1.0f;
	
	float theta2 = thetaS*thetaS;
	float theta3 = theta2*thetaS;
	float T = turb;
	float T2 = turb*turb;

	float chi = (4.f / 9.f - T / 120.f) * (M_PI - 2.0f * thetaS);
	zenith_Y = (4.0453f * T - 4.9710f) * tan(chi) - 0.2155f * T + 2.4192f;
	zenith_Y *= 0.06;

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

Spectrum SkyLight::Le(const Vector &dir) const {
	const float theta = SphericalTheta(dir);
	const float phi = SphericalPhi(dir);

	Spectrum s;
	GetSkySpectralRadiance(theta, phi, &s);

	return gain * s;
}

void SkyLight::GetSkySpectralRadiance(const float theta, const float phi, Spectrum * const spect) const
{
	// add bottom half of hemisphere with horizon colour
	const float theta_fin = Min<float>(theta, (M_PI * 0.5f) - 0.001f);
	const float gamma = RiAngleBetween(theta,phi,thetaS,phiS);

	// Compute xyY values
	const float x = zenith_x * PerezBase(perez_x, theta_fin, gamma);
	const float y = zenith_y * PerezBase(perez_y, theta_fin, gamma);
	const float Y = zenith_Y * PerezBase(perez_Y, theta_fin, gamma);

	ChromaticityToSpectrum(Y, x, y, spect);
}

SunLight::SunLight(float turb, float relSize, const Vector &sd) : LightSource() {
	turbidity = turb;
	sundir = Normalize(sd);
	gain = Spectrum(1.0f, 1.0f, 1.0f);
	
	CoordinateSystem(sundir, &x, &y);

	// Values from NASA Solar System Exploration page
	// http://solarsystem.nasa.gov/planets/profile.cfm?Object=Sun&Display=Facts&System=Metric
	const float sunRadius = 695500.f;
	const float sunMeanDistance = 149600000.f;

	if (relSize * sunRadius <= sunMeanDistance) {
		sin2ThetaMax = relSize * sunRadius / sunMeanDistance;
		sin2ThetaMax *= sin2ThetaMax;
		cosThetaMax = sqrtf(1.f - sin2ThetaMax);
	} else {
		cosThetaMax = 0.f;
		sin2ThetaMax = 1.f;
	}

	Vector wh = Normalize(sundir);
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
	suncolor = LSPD.ToRGB() / 1000000000.0f;
}

void SunLight::SetGain(const Spectrum &g) {
	gain = g;
	suncolor *= gain;
}

Spectrum SunLight::Le(const Vector &dir) const {
	if(cosThetaMax < 1.f && Dot(dir,-sundir) > cosThetaMax)
		return suncolor;
	else
		return Spectrum();
}

Spectrum SunLight::Sample_L(const std::vector<ExtTriangleMesh *> &objs, const Point &p, const Normal *N,
	const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	
	float d1, d2;
	float worldRadius = 100.0f;

	if (N && Dot(*N,-sundir) > 0.0f) {
		*pdf = 0.0f;
		return Spectrum();
	}

	ConcentricSampleDisk(u1, u2, &d1, &d2);

	//Vector op = (d1 * x + d2 * y) + worldRadius * sundir;
	
	Vector wi = UniformSampleCone(u1, u2, cosThetaMax, x, y, sundir);

	*shadowRay = Ray(p, wi, RAY_EPSILON, INFINITY);

	*pdf = UniformConePdf(cosThetaMax) / (M_PI * worldRadius * worldRadius);

	return suncolor;
}

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

InfiniteLight::InfiniteLight(TexMapInstance *tx) {
	tex = tx;
	shiftU = 0.f;
	shiftV = 0.f;
}

Spectrum InfiniteLight::Le(const Vector &dir) const {
	const float theta = SphericalTheta(dir);
	const UV uv(SphericalPhi(dir) * INV_TWOPI + shiftU, theta * INV_PI + shiftV);

	return gain * tex->GetTexMap()->GetColor(uv);
}

Spectrum InfiniteLight::Sample_L(const std::vector<ExtTriangleMesh *> &objs, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	if (N) {
		Vector wi = CosineSampleHemisphere(u0, u1);
		*pdf = wi.z * INV_PI;

		Vector v1, v2;
		CoordinateSystem(Vector(*N), &v1, &v2);

		wi = Vector(
				v1.x * wi.x + v2.x * wi.y + N->x * wi.z,
				v1.y * wi.x + v2.y * wi.y + N->y * wi.z,
				v1.z * wi.x + v2.z * wi.y + N->z * wi.z);
		*shadowRay = Ray(p, wi, RAY_EPSILON, INFINITY);

		return Le(wi);
	} else {
		Vector wi = UniformSampleSphere(u0, u1);

		*shadowRay = Ray(p, wi, RAY_EPSILON, INFINITY);
		*pdf = 1.f / (4.f * M_PI);

		return Le(wi);
	}
}

//------------------------------------------------------------------------------
// InfiniteLight with portals
//------------------------------------------------------------------------------

InfiniteLightPortal::InfiniteLightPortal(Context *ctx, TexMapInstance *tx, const std::string &portalFileName) :
	InfiniteLight(tx) {
	// Read portals
	LR_LOG(ctx, "Portal PLY objects file name: " << portalFileName);
	portals = ExtTriangleMesh::LoadExtTriangleMesh(ctx, portalFileName);
	const Triangle *tris = portals->GetTriangles();
	for (unsigned int i = 0; i < portals->GetTotalTriangleCount(); ++i)
		portalAreas.push_back(tris[i].Area(portals->GetVertices()));
}

InfiniteLightPortal::~InfiniteLightPortal() {
	portals->Delete();
	delete portals;
}

Spectrum InfiniteLightPortal::Sample_L(const std::vector<ExtTriangleMesh *> &objs, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	// Select one of the portals
	const unsigned int portalCount = portals->GetTotalTriangleCount();
	unsigned int portalIndex = Min<unsigned int>(Floor2UInt(portalCount * u2), portalCount - 1);

	// Looks for a valid portal
	const Triangle *tris = portals->GetTriangles();
	const Normal *normals = portals->GetNormal();
	for (unsigned int i = 0; i < portalCount; ++i) {
		// Sample the triangle
		const Triangle &tri = tris[portalIndex];
		Point samplePoint;
		float b0, b1, b2;
		tri.Sample(portals->GetVertices(), u0, u1, &samplePoint, &b0, &b1, &b2);
		const Normal &sampleN = normals[tri.v[0]];

		// Check if the portal is visible
		Vector wi = samplePoint - p;
		const float distanceSquared = wi.LengthSquared();
		wi /= sqrtf(distanceSquared);

		const float sampleNdotMinusWi = Dot(sampleN, -wi);
		if ((sampleNdotMinusWi > 0.f) && (N && Dot(*N, wi) > 0.f)) {
			*shadowRay = Ray(p, wi, RAY_EPSILON, INFINITY);
			*pdf = distanceSquared / (sampleNdotMinusWi * portalAreas[portalIndex] * portalCount);

			// Using 0.1 instead of 0.0 to cut down fireflies
			if (*pdf <= 0.1f)
				continue;

			return Le(wi);
		}

		if (++portalIndex >= portalCount)
			portalIndex = 0;
	}

	*pdf = 0.f;
	return Spectrum();
}

//------------------------------------------------------------------------------
// InfiniteLight with importance sampling
//------------------------------------------------------------------------------

InfiniteLightIS::InfiniteLightIS(TexMapInstance *tx) : InfiniteLight(tx) {
	uvDistrib = NULL;
}

void InfiniteLightIS::Preprocess() {
	// Build the importance map

	// Cale down the texture map
	const TextureMap *tm = tex->GetTexMap();
	const unsigned int nu = tm->GetWidth() / 2;
	const unsigned int nv = tm->GetHeight() / 2;
	//cerr << "Building importance sampling map for InfiniteLightIS: "<< nu << "x" << nv << endl;

	float *img = new float[nu * nv];
	UV uv;
	for (unsigned int v = 0; v < nv; ++v) {
		uv.v = (v + .5f) / nv + shiftV;

		for (unsigned int u = 0; u < nu; ++u) {
			uv.u = (u + .5f) / nu + shiftV;

			float pdf;
			LatLongMappingMap(uv.u, uv.v, NULL, &pdf);

			if (pdf > 0.f)
				img[u + v * nu] = tm->GetColor(uv).Filter() / pdf;
			else
				img[u + v * nu] = 0.f;
		}
	}

	uvDistrib = new Distribution2D(img, nu, nv);
	delete[] img;
}

Spectrum InfiniteLightIS::Sample_L(const std::vector<ExtTriangleMesh *> &objs, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	float uv[2];
	uvDistrib->SampleContinuous(u0, u1, uv, pdf);

	// Convert sample point to direction on the unit sphere
	const float phi = uv[0] * 2.f * M_PI;
	const float theta = uv[1] * M_PI;
	const float costheta = cosf(theta);
	const float sintheta = sinf(theta);
	const Vector wi = SphericalDirection(sintheta, costheta, phi);

	if (N && (Dot(wi, *N) <= 0.f)) {
		*pdf = 0.f;
		return Spectrum();
	}

	*shadowRay = Ray(p, wi, RAY_EPSILON, INFINITY);
	*pdf /= (2.f * M_PI * M_PI * sintheta);

	UV texUV(uv[0], uv[1]);
	return gain * tex->GetTexMap()->GetColor(texUV);
}

//------------------------------------------------------------------------------
// Triangle Area Light
//------------------------------------------------------------------------------

TriangleLight::TriangleLight(const AreaLightMaterial *mat, const unsigned int mshIndex,
		const unsigned int triangleIndex, const std::vector<ExtTriangleMesh *> &objs) {
	lightMaterial = mat;
	meshIndex = mshIndex;
	triIndex = triangleIndex;

	const ExtTriangleMesh *mesh = objs[meshIndex];
	area = (mesh->GetTriangles()[triIndex]).Area(mesh->GetVertices());
}

Spectrum TriangleLight::Le(const std::vector<ExtTriangleMesh *> &objs, const Vector &wo) const {
	const ExtTriangleMesh *mesh = objs[meshIndex];
	const Triangle &tri = mesh->GetTriangles()[triIndex];
	Normal sampleN = mesh->GetNormal()[tri.v[0]]; // Light sources are supposed to be flat

	if (Dot(sampleN, wo) <= 0.f)
		return Spectrum();

	const Spectrum *colors = mesh->GetColors();
	if (colors)
		return mesh->GetColors()[tri.v[0]] * lightMaterial->GetGain(); // Light sources are supposed to have flat color
	else
		return lightMaterial->GetGain(); // Light sources are supposed to have flat color
}

Spectrum TriangleLight::Sample_L(const std::vector<ExtTriangleMesh *> &objs, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	const ExtTriangleMesh *mesh = objs[meshIndex];
	const Triangle &tri = mesh->GetTriangles()[triIndex];

	Point samplePoint;
	float b0, b1, b2;
	tri.Sample(mesh->GetVertices(), u0, u1, &samplePoint, &b0, &b1, &b2);
	const Normal &sampleN = mesh->GetNormal()[tri.v[0]]; // Light sources are supposed to be flat

	Vector wi = samplePoint - p;
	const float distanceSquared = wi.LengthSquared();
	const float distance = sqrtf(distanceSquared);
	wi /= distance;

	const float sampleNdotMinusWi = Dot(sampleN, -wi);
	if ((sampleNdotMinusWi <= 0.f) || (N && Dot(*N, wi) <= 0.f)) {
		*pdf = 0.f;
		return Spectrum();
	}

	*shadowRay = Ray(p, wi, RAY_EPSILON, distance - RAY_EPSILON);
	*pdf = distanceSquared / (sampleNdotMinusWi * area);

	// Using 0.1 instead of 0.0 to cut down fireflies
	if (*pdf <= 0.1f) {
		*pdf = 0.f;
		return Spectrum();
	}

	const Spectrum *colors = mesh->GetColors();
	if (colors)
		return mesh->GetColors()[tri.v[0]] * lightMaterial->GetGain(); // Light sources are supposed to have flat color
	else
		return lightMaterial->GetGain(); // Light sources are supposed to have flat color
}

Spectrum TriangleLight::Sample_L(const std::vector<ExtTriangleMesh *> &objs,
		const float u0, const float u1, const float u2, const float u3, float *pdf, Ray *ray) const {
	const ExtTriangleMesh *mesh = objs[meshIndex];
	const Triangle &tri = mesh->GetTriangles()[triIndex];

	// Ray origin
	float b0, b1, b2;
	Point orig;
	tri.Sample(mesh->GetVertices(), u0, u1, &orig, &b0, &b1, &b2);

	// Ray direction
	const Normal &sampleN = mesh->GetNormal()[tri.v[0]]; // Light sources are supposed to be flat
	Vector dir = UniformSampleSphere(u2, u3);
	float RdotN = Dot(dir, sampleN);
	if (RdotN < 0.f) {
		dir *= -1.f;
		RdotN = -RdotN;
	}

	*ray = Ray(orig, dir);

	*pdf = INV_TWOPI / area;

	const Spectrum *colors = mesh->GetColors();
	if (colors)
		return mesh->GetColors()[tri.v[0]] * lightMaterial->GetGain() * RdotN; // Light sources are supposed to have flat color
	else
		return lightMaterial->GetGain() * RdotN; // Light sources are supposed to have flat color
}
