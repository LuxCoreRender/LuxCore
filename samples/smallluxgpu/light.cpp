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

#include "light.h"

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

InfiniteLight::InfiniteLight(TextureMap *tx) {
	tex = tx;
	shiftU = 0.f;
	shiftV = 0.f;
}

Spectrum InfiniteLight::Le(const Vector &dir) const {
	const float theta = SphericalTheta(dir);
	const UV uv(SphericalPhi(dir) * INV_TWOPI + shiftU, theta * INV_PI + shiftV);

	return gain * tex->GetColor(uv);
}

Spectrum InfiniteLight::Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	Vector wi = CosineSampleHemisphere(u0, u1);
	*pdf = wi.z * INV_PI;

	Vector v1, v2;
	CoordinateSystem(Vector(N), &v1, &v2);

	wi = Vector(
			v1.x * wi.x + v2.x * wi.y + N.x * wi.z,
			v1.y * wi.x + v2.y * wi.y + N.y * wi.z,
			v1.z * wi.x + v2.z * wi.y + N.z * wi.z);
	*shadowRay = Ray(p, wi, RAY_EPSILON, INFINITY);

	return Le(wi);
}

//------------------------------------------------------------------------------
// InfiniteLight with portals
//------------------------------------------------------------------------------

InfiniteLightPortal::InfiniteLightPortal(Context *ctx, TextureMap *tx, const string &portalFileName) :
	InfiniteLight(tx) {
	// Read portals
	cerr << "Portal PLY objects file name: " << portalFileName << endl;
	portals = ExtTriangleMesh::LoadExtTriangleMesh(ctx, portalFileName);
	const Triangle *tris = portals->GetTriangles();
	for (unsigned int i = 0; i < portals->GetTotalTriangleCount(); ++i)
		portalAreas.push_back(tris[i].Area(portals->GetVertices()));
}

InfiniteLightPortal::~InfiniteLightPortal() {
	portals->Delete();
	delete portals;
}

Spectrum InfiniteLightPortal::Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
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
		const float NdotWi = Dot(N, wi);
		if ((sampleNdotMinusWi > 0.f) && (NdotWi > 0.f)) {
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

InfiniteLightIS::InfiniteLightIS(TextureMap *tx) : InfiniteLight(tx) {
	uvDistrib = NULL;
}

void InfiniteLightIS::Preprocess() {
	// Build the importance map

	// Cale down the texture map
	const unsigned int nu = tex->GetWidth() / 2;
	const unsigned int nv = tex->GetHeight() / 2;
	cerr << "Building importance sampling map for InfiniteLightIS: "<< nu << "x" << nv << endl;

	float *img = new float[nu * nv];
	UV uv;
	for (unsigned int v = 0; v < nv; ++v) {
		uv.v = (v + .5f) / nv + shiftV;

		for (unsigned int u = 0; u < nu; ++u) {
			uv.u = (u + .5f) / nu + shiftV;

			float pdf;
			LatLongMappingMap(uv.u, uv.v, NULL, &pdf);

			if (pdf > 0.f)
				img[u + v * nu] = tex->GetColor(uv).Filter() / pdf;
			else
				img[u + v * nu] = 0.f;
		}
	}

	uvDistrib = new Distribution2D(img, nu, nv);
	delete[] img;
}

Spectrum InfiniteLightIS::Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	float uv[2];
	uvDistrib->SampleContinuous(u0, u1, uv, pdf);

	// Convert sample point to direction on the unit sphere
	const float phi = uv[0] * 2.f * M_PI;
	const float theta = uv[1] * M_PI;
	const float costheta = cosf(theta);
	const float sintheta = sinf(theta);
	const Vector wi = SphericalDirection(sintheta, costheta, phi);

	if (Dot(wi, N) > 0.f) {
		*shadowRay = Ray(p, wi, RAY_EPSILON, INFINITY);
		*pdf /= (2.f * M_PI * M_PI * sintheta);

		UV texUV(uv[0], uv[1]);
		return gain * tex->GetColor(texUV);
	} else {
		// Resort to random sampling of the hemisphere
		return InfiniteLight::Sample_L(objs, p, N, u0, u1, u2, pdf, shadowRay);
	}
}

//------------------------------------------------------------------------------
// Triangle Area Light
//------------------------------------------------------------------------------

TriangleLight::TriangleLight(const AreaLightMaterial *mat, const unsigned int mshIndex,
		const unsigned int triangleIndex, const vector<ExtTriangleMesh *> &objs) {
	lightMaterial = mat;
	meshIndex = mshIndex;
	triIndex = triangleIndex;

	const ExtTriangleMesh *mesh = objs[meshIndex];
	area = (mesh->GetTriangles()[triIndex]).Area(mesh->GetVertices());
}

Spectrum TriangleLight::Le(const vector<ExtTriangleMesh *> &objs, const Vector &wo) const {
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

Spectrum TriangleLight::Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
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
	const float NdotWi = Dot(N, wi);
	if ((sampleNdotMinusWi <= 0.f) || (NdotWi <= 0.f)) {
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