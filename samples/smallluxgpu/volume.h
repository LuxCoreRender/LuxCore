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

#include "smalllux.h"
#include "sampler.h"

class VolumeIntegrator {
public:
	virtual ~VolumeIntegrator() { }

	virtual Spectrum Transmittance(const Ray &ray) const = 0;
	//virtual bool GenerateLiRays(RayBuffer *rayBuffer, vector<Ray> &rays) const = 0;
};

class SingleScatteringIntegrator : public VolumeIntegrator {
public:
	SingleScatteringIntegrator(const BBox &bbox, const float step,
			Spectrum absorption, Spectrum scattering) {
		region = bbox;
		stepSize = step;
		sig_a = absorption;
		sig_s = scattering;
		sig_as = sig_a + sig_s;
	}

	Spectrum Transmittance(const Ray &ray) const {
		return Exp(-HomogenousTau(ray));
	}

	/*bool GenerateLiRays(const Scene *scene, Sample *sample, RayBuffer *rayBuffer,
		const Ray &ray, Vector<Ray> &volumeRays, Vector<unsigned int> currentVolueRayIndex,
		Spectrum *Tr, Spectrum *Lv) const {
		float t0, t1;
		if (!region.IntersectP(ray, &t0, &t1) || (scene->lights.size() < 1)) {
			volumeRays.resize(0);
			return true;
		}

		// Prepare for volume integration stepping
		const unsigned int nSamples = Ceil2Int((t1 - t0) / stepSize);

		// Check if the RayBuffer has enough space for the generated rays
		if (nSamples > rayBuffer->LeftSpace())
			return false;

		const float step = (t1 - t0) / nSamples;
		*Tr = Spectrum(1.f);
		*Lv = Spectrum(0.f);
		Point p = ray(t0);
		Vector w = -ray.d;
		t0 += sample->GetLazyValue() * step;

		volumeRays.resize(0);
		currentVolueRayIndex.resize(0);
		Point pPrev;
		Spectrum stepeTau = expf(-step * (sig_a + sig_s));
		for (unsigned int i = 0; i < nSamples; ++i, t0 += step) {
			pPrev = p;
			p = ray(t0);
			Tr *= stepeTau;

			// Possibly terminate ray marching if transmittance is small
			if (Tr.Filter() < 1e-3f) {
				const float continueProb = .5f;
				if (sample->GetLazyValue() > continueProb)
					break;
				Tr /= continueProb;
			}

			// Compute single-scattering source term at p
			Spectrum ss = sig_s;
			if (!ss.IsBlack() && scene->lights.size() > 0) {
				int nLights = scene->lights.size();
				int ln = min(Floor2Int(lightNum[sampOffset] * nLights),
							 nLights-1);
				Light *light = scene->lights[ln];
				// Add contribution of _light_ due to scattering at _p_
				float pdf;
				VisibilityTester vis;
				Vector wo;
				LightSample ls(lightComp[sampOffset], lightPos[2*sampOffset],
							   lightPos[2*sampOffset+1]);
				Spectrum L = light->Sample_L(p, 0.f, ls, ray.time, &wo, &pdf, &vis);
				if (!L.IsBlack() && pdf > 0.f && vis.Unoccluded(scene)) {
					Spectrum Ld = L * vis.Transmittance(scene, renderer, NULL, rng, arena);
					Lv += Tr * ss * vr->p(p, w, -wo, ray.time) * Ld * float(nLights) / pdf;
				}
			}
			++sampOffset;
		}

	}*/

private:
	Spectrum HomogenousTau(const Ray &ray) const {
		float t0, t1;
        if (!region.IntersectP(ray, &t0, &t1))
			return 0.f;

        return Distance(ray(max(t0, ray.mint)), ray(min(t1, ray.maxt))) * sig_as;
	}

	BBox region;
	float stepSize;
	Spectrum sig_a, sig_s, sig_as;
};

#endif	/* _VOLUME_H */
