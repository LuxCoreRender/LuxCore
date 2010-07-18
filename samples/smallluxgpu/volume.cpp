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

#include "smalllux.h"
#include "volume.h"
#include "slgscene.h"

VolumeComputation::VolumeComputation(VolumeIntegrator *vol) {
	float maxRayLen = vol->GetMaxRayLength();
	const unsigned int maxSampleCount = Ceil2Int(maxRayLen / vol->GetStepSize()) + 1;

	rays = new Ray[maxSampleCount];
	currentRayIndex = new unsigned int[maxSampleCount];
	scatteredLight = new Spectrum[maxSampleCount];

	Reset();
}

VolumeComputation::~VolumeComputation() {
	delete rays;
	delete currentRayIndex;
	delete scatteredLight;
}

void SingleScatteringIntegrator::GenerateLiRays(const SLGScene *scene, Sample *sample,
		const Ray &ray, VolumeComputation *comp) const {
	comp->Reset();

	float t0, t1;
	if (!region.IntersectP(ray, &t0, &t1))
		return;

	if (sig_s.Black() || (scene->lights.size() < 1)) {
		const float distance = t1 - t0;
		comp->emittedLight = lightEmission * distance;
	} else {
		// Prepare for volume integration stepping
		const float distance = t1 - t0;
		const unsigned int nSamples = Ceil2Int(distance / stepSize);

		const float step = distance / nSamples;
		Spectrum Tr = Spectrum(1.f, 1.f, 1.f);
		Spectrum Lv = Spectrum();
		Point p = ray(t0);
		const float offset = sample->GetLazyValue();
		t0 += offset * step;

		Point pPrev;
		for (unsigned int i = 0; i < nSamples; ++i, t0 += step) {
			pPrev = p;
			p = ray(t0);

			Lv += lightEmission;
			if (!sig_s.Black() && (scene->lights.size() > 0)) {
				// Select the light to sample
				const unsigned int currentLightIndex = scene->SampleLights(sample->GetLazyValue());
				const LightSource *light = scene->lights[currentLightIndex];

				// Select a point on the light surface
				float lightPdf;
				Spectrum lightColor = light->Sample_L(scene, p, NULL,
						sample->GetLazyValue(), sample->GetLazyValue(), sample->GetLazyValue(),
						&lightPdf, &(comp->rays[comp->rayCount]));

				if ((lightPdf > 0.f) && !lightColor.Black()) {
					comp->scatteredLight[comp->rayCount] = Tr * sig_s * lightColor *
							(scene->lights.size() * step / (4.f * M_PI * lightPdf));
					comp->rayCount++;
				}
			}

			comp->emittedLight = Lv * step;
		}
	}
}
