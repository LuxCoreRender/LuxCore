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

#ifndef _VOLUME_H
#define	_VOLUME_H

#include <vector>

#include "smalllux.h"
#include "sampler.h"

class Scene;

class VolumeIntegrator {
public:
	virtual ~VolumeIntegrator() { }

	virtual Spectrum Transmittance(const Ray &ray) const = 0;
	virtual void GenerateLiRays(const Scene *scene, Sample *sample,	const Ray &ray,
		vector<Ray> *volumeRays, vector<Spectrum> *volumeScatteredLight,
		Spectrum *emittedLight) const = 0;
};

class SingleScatteringIntegrator : public VolumeIntegrator {
public:
	SingleScatteringIntegrator(const BBox &bbox, const float step,
			Spectrum absorption, Spectrum scattering, Spectrum emission, const float gg) {
		region = bbox;
		stepSize = step;
		sig_a = absorption;
		sig_s = scattering;
		lightEmission = emission;
		g = gg;
	}

	Spectrum Transmittance(const Ray &ray) const {
		return Exp(-HomogenousTau(ray));
	}

	void GenerateLiRays(const Scene *scene, Sample *sample,	const Ray &ray,
		vector<Ray> *volumeRays, vector<Spectrum> *volumeScatteredLight,
		Spectrum *emittedLight) const;

private:
	Spectrum HomogenousTau(const Ray &ray) const {
		float t0, t1;
        if (!region.IntersectP(ray, &t0, &t1))
			return 0.f;

		// I intentionally leave scattering out because it is a lot easier to use
        return Distance(ray(t0), ray(t1)) * (sig_a /*+ sig_s*/);
	}

	float PhaseHG(const Vector &w, const Vector &wp, float g) const {
		float costheta = Dot(w, wp);
		return 1.f / (4.f * M_PI) * (1.f - g*g) /
				powf(1.f + g * g - 2.f * g * costheta, 1.5f);
	}

	BBox region;
	float stepSize, g;
	Spectrum sig_a, sig_s, lightEmission;
};

#endif	/* _VOLUME_H */
