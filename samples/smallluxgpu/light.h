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

#include <ImfRgbaFile.h>
#include <ImfArray.h>
#include <ImathBox.h>

#include "smalllux.h"
#include "material.h"

#include "luxrays/luxrays.h"
#include "luxrays/utils/core/exttrianglemesh.h"

class LightSource {
public:
	virtual ~LightSource() { }

	virtual Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, float *pdf, Ray *shadowRay) const = 0;
};

class InfiniteLight : public LightSource {
public:
	InfiniteLight(const string &hdrFileName) {
		// Read the EXR file
		cerr << "Reading HDR light map: " << hdrFileName << endl;
		Imf::RgbaInputFile file(hdrFileName.c_str());

		Imath::Box2i dw = file.dataWindow();
		width = dw.max.x - dw.min.x + 1;
		height = dw.max.y - dw.min.y + 1;

		Imf::Array2D<Imf::Rgba> pixels;
		pixels.resizeErase(height, width);
		file.setFrameBuffer(&pixels[0][0] - dw.min.x - dw.min.y * width, 1, width);
		file.readPixels(dw.min.y, dw.max.y);

		cerr << "HDR light map size: " << width << "x" << height << " (" << width * height *sizeof(Spectrum) / 1024 << "Kbytes)" << endl;
		texels = new Spectrum[width * height];
		for (unsigned int y = 0; y < height; ++y) {
			for (unsigned int x = 0; x < width; ++x) {
				const unsigned int idx = y * width + x;
				texels[idx].r = pixels[y][x].r;
				texels[idx].g = pixels[y][x].g;
				texels[idx].b = pixels[y][x].b;
			}
		}
	}

	void SetGain(const Spectrum &gain) {
		if ((gain.r == 1.f) && (gain.g == 1.f) && (gain.b == 1.f))
			return;

		for (unsigned int i = 0; i < width * height; ++i)
			texels[i] *= gain;
	}

	Spectrum Le(const Ray &ray) const {
		const float theta = SphericalTheta(ray.d);
        const float u = SphericalPhi(ray.d) * INV_TWOPI;
        const float v = theta * INV_PI;

		return Interpolate(u, v);
	}

	Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, float *pdf, Ray *shadowRay) const {
		*pdf = 0.f;
		return Spectrum();
	}

private:
	Spectrum Interpolate(const float u, const float v) const {
		const float s = u * width - 0.5f;
		const float t = v * height - 0.5f;

		const int s0 = Floor2Int(s);
		const int t0 = Floor2Int(t);

		const float ds = s - s0;
		const float dt = t - t0;

		const float ids = 1.f - ds;
		const float idt = 1.f - dt;

		return ids * idt * GetTexel(s0, t0) +
				ids * dt * GetTexel(s0, t0 + 1) +
				ds * idt * GetTexel(s0 + 1, t0) +
				ds * dt * GetTexel(s0 + 1, t0 + 1);
	}

	const Spectrum &GetTexel(const unsigned int s, const unsigned int t) const {
		const unsigned int u = Mod(s, width);
		const unsigned int v = Mod(t, height);

		const unsigned index = v * width + u;
		assert (index >= 0);
		assert (index < width * height);

		return texels[index];
	}

	unsigned int width, height;
	Spectrum *texels;
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
		const float u0, const float u1, float *pdf, Ray *shadowRay) const {
		const ExtTriangleMesh *mesh = objs[meshIndex];
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		Point samplePoint;
		float b0, b1, b2;
		tri.Sample(mesh->GetVertices(), u0, u1, &samplePoint, &b0, &b1, &b2);
		const Normal sampleN = mesh->GetNormal()[tri.v[0]]; // Light sources are supposed to be flat

		Vector wi = samplePoint - p;
		const float distanceSquared = wi.LengthSquared();
		const float distance = sqrtf(distanceSquared);
		wi /= distance;

		float SampleNdotMinusWi = Dot(sampleN, -wi);
		float NdotMinusWi = Dot(N, wi);
		if ((SampleNdotMinusWi <= 0.f) || (NdotMinusWi <= 0.f)) {
			*pdf = 0.f;
			return Spectrum();
		}

		*shadowRay = Ray(p, wi, RAY_EPSILON, distance - RAY_EPSILON);
		*pdf = distanceSquared / (SampleNdotMinusWi * area);

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

