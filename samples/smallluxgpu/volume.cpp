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

#include "volume.h"
#include "scene.h"

using namespace std;


bool SingleScatteringIntegrator::GenerateLiRays(const Scene *scene, Sample *sample, const unsigned int maxVectorSize,
		const Ray &ray, vector<Ray> &volumeRays, vector<unsigned int> currentVolueRayIndex,
		Spectrum *Lvolume) const {
	float t0, t1;
	if (!region.IntersectP(ray, &t0, &t1) || (scene->lights.size() < 1)) {
		volumeRays.resize(0);
		return false;
	}

	// Prepare for volume integration stepping
	const unsigned int nSamples = Ceil2Int((t1 - t0) / stepSize);

	// Check if the RayBuffer has enough space for the generated rays
	if (nSamples > maxVectorSize)
		return false;

	const float step = (t1 - t0) / nSamples;
	Spectrum Tr = Spectrum(1.f, 1.f, 1.f);
	Spectrum Lv = Spectrum();
	Point p = ray(t0);
	const float offset = sample->GetLazyValue();
	t0 += offset * step;

	volumeRays.resize(0);
	currentVolueRayIndex.resize(0);
	Point pPrev;
	Spectrum stepeTau = Exp(-step * (sig_a + sig_s));
	for (unsigned int i = 0; i < nSamples; ++i, t0 += step) {
		pPrev = p;
		p = ray(t0);
		if (i == 0)
			Tr *= Exp(-offset * (sig_a + sig_s));
		else
			Tr *= stepeTau;

		// Possibly terminate ray marching if transmittance is small
		if (Tr.Filter() < 1e-3f) {
			const float continueProb = .5f;
			if (sample->GetLazyValue() > continueProb)
				break;
			Tr /= continueProb;
		}

		Lv += Tr * lightEmission;
		/*Spectrum ss = sig_s;
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
		}*/
	}

	*Lvolume = Lv * step;
	return true;
}