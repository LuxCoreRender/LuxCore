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

#include <cstdlib>
#include <istream>
#include <stdexcept>
#include <sstream>

#include <boost/detail/container_fwd.hpp>

#include "slgscene.h"

#include "luxrays/core/dataset.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/film/film.h"

SLGScene::SLGScene(Context *ctx, const string &fileName, Film *film, const int accelType) :
	Scene(ctx, fileName, accelType) {
	camera->Update(film->GetWidth(), film->GetHeight());

	//--------------------------------------------------------------------------
	// Check if there is a VolumeIntegrator defined
	//--------------------------------------------------------------------------

	if (scnProp->GetInt("scene.partecipatingmedia.singlescatering.enable", 0)) {
		vector<float> vf = GetParameters("scene.partecipatingmedia.singlescatering.bbox", 6, "-10.0 -10.0 -10.0 10.0 10.0 10.0");
		BBox region(Point(vf.at(0), vf.at(1), vf.at(2)), Point(vf.at(3), vf.at(4), vf.at(5)));

		const float stepSize = scnProp->GetFloat("scene.partecipatingmedia.singlescatering.stepsize", 0.5);

		vf = GetParameters("scene.partecipatingmedia.singlescatering.scattering", 3, "0.0 0.0 0.0");
		const Spectrum scattering(vf.at(0), vf.at(1), vf.at(2));
		if ((scattering.Filter() > 0.f) && (useInfiniteLightBruteForce))
			throw runtime_error("Partecipating media scattering is not supported with InfiniteLight brute force");

		vf = GetParameters("scene.partecipatingmedia.singlescatering.emission", 3, "0.0 0.0 0.0");
		const Spectrum emission(vf.at(0), vf.at(1), vf.at(2));

		const float rrProb = scnProp->GetFloat("scene.partecipatingmedia.singlescatering.rrprob", 0.33f);

		volumeIntegrator = new SingleScatteringIntegrator(region, stepSize, rrProb, scattering, emission);
	} else
		volumeIntegrator = NULL;
}

SLGScene::~SLGScene() {
	delete volumeIntegrator;
}
