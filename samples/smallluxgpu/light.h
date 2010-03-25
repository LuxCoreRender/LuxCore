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
	InfiniteLight(TextureMap *tx);
	virtual ~InfiniteLight() { }

	void SetGain(const Spectrum &g) {
		gain = g;
	}

	void SetShift(const float su, const float sv) {
		shiftU = su;
		shiftV = sv;
	}

	virtual Spectrum Le(const Vector &dir) const;

	virtual Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;

private:
	TextureMap *tex;
	float shiftU, shiftV;
	Spectrum gain;
};

class InfiniteLightPortal : public InfiniteLight {
public:
	InfiniteLightPortal(Context *ctx, TextureMap *tx, const string &portalFileName);
	~InfiniteLightPortal();

	Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;

private:
	ExtTriangleMesh *portals;
	vector<float> portalAreas;
};

class TriangleLight : public LightSource, public LightMaterial {
public:
	TriangleLight() { }
	TriangleLight(const AreaLightMaterial *mat, const unsigned int mshIndex,
		const unsigned int triangleIndex, const vector<ExtTriangleMesh *> &objs);

	const Material *GetMaterial() const { return lightMaterial; }

	Spectrum Le(const vector<ExtTriangleMesh *> &objs, const Vector &wo) const;

	Spectrum Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;

private:
	const AreaLightMaterial *lightMaterial;
	unsigned int meshIndex, triIndex;
	float area;

};

#endif	/* _LIGHT_H */

