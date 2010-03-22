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

#ifndef _LIGHT_H
#define	_LIGHT_H

#include "smalllux.h"
#include "material.h"
#include "texmap.h"

#include "luxrays/luxrays.h"
#include "luxrays/utils/core/exttrianglemesh.h"


class LightSource {
public:
	virtual ~LightSource() { }

	virtual Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const = 0;
};

class InfiniteLight : public LightSource {
public:
	InfiniteLight(TextureMap *tx) {
		tex = tx;
		portals = NULL;
		shiftU = 0.f;
		shiftV = 0.f;
	}

	InfiniteLight(Context *ctx, TextureMap *tx, const string &portalFileName) {
		tex = tx;

		// Read portals
		cerr << "Portal PLY objects file name: " << portalFileName << endl;
		portals = ExtTriangleMesh::LoadExtTriangleMesh(ctx, portalFileName);
		const Triangle *tris = portals->GetTriangles();
		for (unsigned int i = 0; i < portals->GetTotalTriangleCount(); ++i)
			portalAreas.push_back(tris[i].Area(portals->GetVertices()));

		shiftU = 0.f;
		shiftV = 0.f;
	}

	~InfiniteLight() {
		if (portals) {
			portals->Delete();
			delete portals;
		}
	}

	void SetGain(const Spectrum &gain) {
		tex->Scale(gain);
	}

	void SetShift(const float su, const float sv) {
		cerr<<"======================"<<su<<"="<<sv<<endl;
		shiftU = su;
		shiftV = sv;
	}

	Spectrum Le(const Vector &dir) const {
		const float theta = SphericalTheta(dir);
        const UV uv(SphericalPhi(dir) * INV_TWOPI + shiftU, theta * INV_PI + shiftV);

		return tex->GetColor(uv);
	}

	Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
		if (portals) {
			// Select one of the portals
			const unsigned int portalCount = portals->GetTotalTriangleCount();
			unsigned int portalIndex = Min<unsigned int>(Floor2UInt(portalCount * u0), portalCount - 1);

			// Looks for a valid portal
			const Triangle *tris = portals->GetTriangles();
			const Normal *normals = portals->GetNormal();
			for (unsigned int i = 0; i < portalCount; ++i) {
				// Sample the triangle
				const Triangle &tri = tris[portalIndex];
				Point samplePoint;
				float b0, b1, b2;
				tri.Sample(portals->GetVertices(), u1, u2, &samplePoint, &b0, &b1, &b2);
				const Normal &sampleN = normals[tri.v[0]]; // Light sources are supposed to be flat

				// Check if the portal is visible
				Vector wi = samplePoint - p;
				const float distanceSquared = wi.LengthSquared();
				wi /= sqrtf(distanceSquared);

				const float sampleNdotMinusWi = Dot(sampleN, -wi);
				const float NdotWi = Dot(N, wi);
				if ((sampleNdotMinusWi > 0.f) && (NdotWi > 0.f)) {
					*shadowRay = Ray(p, wi, RAY_EPSILON, INFINITY);
					*pdf = distanceSquared / (sampleNdotMinusWi * portalAreas[portalIndex]);
					return Le(wi);
				}

				if (++portalIndex >= portalCount)
					portalIndex = 0;
			}

			*pdf = 0.f;
			return Spectrum();
		} else {
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
	}

private:
	TextureMap *tex;
	ExtTriangleMesh *portals;
	vector<float> portalAreas;
	float shiftU, shiftV;
};

class TriangleLight : public LightSource, public LightMaterial {
public:
	TriangleLight() { }

	TriangleLight(const AreaLightMaterial *mat, const unsigned int mshIndex,
		const unsigned int triangleIndex, const vector<ExtTriangleMesh *> &objs) {
		lightMaterial = mat;
		meshIndex = mshIndex;
		triIndex = triangleIndex;

		const ExtTriangleMesh *mesh = objs[meshIndex];
		area = (mesh->GetTriangles()[triIndex]).Area(mesh->GetVertices());
	}

	const Material *GetMaterial() const { return lightMaterial; }

	Spectrum Le(const vector<ExtTriangleMesh *> &objs, const Vector &wo) const {
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

	Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
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

		const Spectrum *colors = mesh->GetColors();
		if (colors)
			return mesh->GetColors()[tri.v[0]] * lightMaterial->GetGain(); // Light sources are supposed to have flat color
		else
			return lightMaterial->GetGain(); // Light sources are supposed to have flat color
	}

private:
	const AreaLightMaterial *lightMaterial;
	unsigned int meshIndex, triIndex;
	float area;

};

#endif	/* _LIGHT_H */

