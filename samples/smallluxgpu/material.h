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

	virtual Spectrum f(const Vector &wi, const Vector &wo, const Normal &N) const = 0;
	virtual Spectrum Sample_f(const Vector &wi, Vector *wo, const Normal &N,
		const float u0, const float u1,  const float u2, float *pdf) const = 0;
};

class MatteMaterial : public SurfaceMaterial {
public:
	MatteMaterial(const Spectrum col) {
		Kd = col;
		KdOverPI = Kd * INV_PI;
	}

	Spectrum f(const Vector &wi, const Vector &wo, const Normal &N) const {
		return KdOverPI;
	}

	Spectrum Sample_f(const Vector &wi, Vector *wo, const Normal &N,
		const float u0, const float u1,  const float u2, float *pdf) const {
		float r1 = 2.f * M_PI * u0;
		float r2 = u1;
		float r2s = sqrt(r2);
		const Vector w(N);

		Vector u;
		if (fabsf(N.x) > .1f) {
			const Vector a(0.f, 1.f, 0.f);
			u = Cross(a, w);
		} else {
			const Vector a(1.f, 0.f, 0.f);
			u = Cross(a, w);
		}
		u = Normalize(u);

		Vector v = Cross(w, u);

		(*wo) = Normalize(u * (cosf(r1) * r2s) + v * (sinf(r1) * r2s) + w * sqrtf(1.f - r2));

		*pdf = AbsDot(wi, N) * INV_PI;

		return KdOverPI;
	}

private:
	Spectrum Kd, KdOverPI;
};

#endif	/* _MATERIAL_H */
