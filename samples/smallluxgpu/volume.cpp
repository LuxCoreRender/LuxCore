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


void SingleScatteringIntegrator::GenerateLiRays(const Scene *scene, Sample *sample,
		const Ray &ray, vector<Ray> *volumeRays, vector<Spectrum> *volumeScatteredLight,
		Spectrum *emittedLight) const {
	float t0, t1;
	if (!region.IntersectP(ray, &t0, &t1)) {
		volumeRays->resize(0);
		return;
	}

	// Prepare for volume integration stepping
	const unsigned int nSamples = Ceil2Int((t1 - t0) / stepSize);

	const float step = (t1 - t0) / nSamples;
	Spectrum Tr = Spectrum(1.f, 1.f, 1.f);
	Spectrum Lv = Spectrum();
	Point p = ray(t0);
	const float offset = sample->GetLazyValue();
	t0 += offset * step;

	volumeRays->resize(0);
	volumeScatteredLight->resize(0);
	Point pPrev;
	// I intentionally leave scattering out because it is a lot easier to use
	Spectrum stepeTau = Exp(-step * (sig_a /*+ sig_s*/));
	for (unsigned int i = 0; i < nSamples; ++i, t0 += step) {
		pPrev = p;
		p = ray(t0);
		if (i == 0)
			// I intentionally leave scattering out because it is a lot easier to use
			Tr *= Exp(-offset * (sig_a /*+ sig_s*/));
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
		if (!sig_s.Black() && scene->lights.size() > 0) {
			// Select the light to sample
			const unsigned int currentLightIndex = scene->SampleLights(sample->GetLazyValue());
			const LightSource *light = scene->lights[currentLightIndex];

			// Select a point on the light surface
			float lightPdf;
			Ray lightRay;
			Spectrum lightColor = light->Sample_L(scene->objects, p, NULL,
					sample->GetLazyValue(), sample->GetLazyValue(), sample->GetLazyValue(),
					&lightPdf, &lightRay);

			if ((lightPdf > 0.f) && !lightColor.Black()) {
				volumeRays->push_back(lightRay);
				volumeScatteredLight->push_back(Tr * sig_s * lightColor * Transmittance(lightRay) *
					(PhaseHG(-ray.d, -lightRay.d, g) * scene->lights.size() * step / lightPdf));
				//volumeScatteredLight->push_back(Spectrum(1.f,1.f,1.f));
			}
		}

		*emittedLight = Lv * step;
	}
}
