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

#ifndef _MATERIAL_H
#define	_MATERIAL_H

#include <vector>

#include "smalllux.h"
#include "luxrays/utils/core/exttrianglemesh.h"

class Material {
public:
	virtual ~Material() { }

	virtual bool IsLightSource() const = 0;
	virtual bool IsLambertian() const = 0;
	virtual bool IsSpecular() const = 0;
};

class LightMaterial : public Material {
public:
	bool IsLightSource() const { return true; }
	bool IsLambertian() const { return false; }
	bool IsSpecular() const { return false; }
};

class AreaLightMaterial : public LightMaterial {
public:
	AreaLightMaterial(const Spectrum col) { gain = col; }

	const Spectrum &GetGain() const { return gain; }

private:
	Spectrum gain;
};

class SurfaceMaterial : public Material {
public:
	bool IsLightSource() const { return false; }
	bool IsLambertian() const { return true; }
	bool IsSpecular() const { return false; }

	virtual Spectrum f(const Vector &wi, const Vector &wo) const = 0;
	virtual Spectrum Sample_f(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1,  const float u2, float *pdf, Ray *outRay) const = 0;
};

class MatteMaterial : public SurfaceMaterial {
public:
	MatteMaterial(const Spectrum col) { Kd = col; }

	Spectrum f(const Vector &wi, const Vector &wo) const { return Kd; }

	Spectrum Sample_f(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1,  const float u2, float *pdf, Ray *outRay) const {
		return 0.f;
	}

private:
	Spectrum Kd;
};

#endif	/* _MATERIAL_H */
