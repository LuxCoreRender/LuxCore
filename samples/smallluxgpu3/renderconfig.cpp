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

#include <boost/lexical_cast.hpp>

#include "luxrays/utils/film/film.h"

#include "renderconfig.h"

RenderConfig::RenderConfig(const string &fileName, const Properties *additionalProperties) {
	SLG_LOG("Reading configuration file: " << fileName);
	cfg.LoadFile(fileName);
	if (additionalProperties)
		cfg.Load(*additionalProperties);

	SLG_LOG("Configuration: ");
	vector<string> keys = cfg.GetAllKeys();
	for (vector<string>::iterator i = keys.begin(); i != keys.end(); ++i)
		SLG_LOG("  " << *i << " = " << cfg.GetString(*i, ""));

	screenRefreshInterval = cfg.GetInt("screen.refresh.interval", 100);

	// Create the Scene
	const string sceneFileName = cfg.GetString("scene.file", "scenes/luxball/luxball.scn");
	const int accelType = cfg.GetInt("accelerator.type", -1);

	scene = new Scene(sceneFileName, accelType);
}

RenderConfig::~RenderConfig() {
	delete scene;
}
void RenderConfig::SetScreenRefreshInterval(const unsigned int t) {
	screenRefreshInterval = t;
}

unsigned int RenderConfig::GetScreenRefreshInterval() const {
	return screenRefreshInterval;
}

void RenderConfig::GetFilmSize(u_int *filmFullWidth, u_int *filmFullHeight,
		u_int *filmSubRegion) const {
	const u_int width = cfg.GetInt("image.width", 640);
	const u_int height = cfg.GetInt("image.height", 480);

	// Check if I'm rendering a film subregion
	u_int subRegion[4];
	if (cfg.IsDefined("image.subregion")) {
		vector<int> params = cfg.GetIntVector("image.subregion", "0 " + boost::lexical_cast<string>(width - 1) + " 0 " + boost::lexical_cast<string>(height - 1));
		if (params.size() != 4)
			throw runtime_error("Syntax error in image.subregion (required 4 parameters)");

		subRegion[0] = Max(0u, Min(width - 1, (u_int)params[0]));
		subRegion[1] = Max(0u, Min(width - 1, Max(subRegion[0] + 1, (u_int)params[1])));
		subRegion[2] = Max(0u, Min(height - 1, (u_int)params[2]));
		subRegion[3] = Max(0u, Min(height - 1, Max(subRegion[2] + 1, (u_int)params[3])));
	} else {
		subRegion[0] = 0;
		subRegion[1] = width - 1;
		subRegion[2] = 0;
		subRegion[3] = height - 1;
	}

	*filmFullWidth = width;
	*filmFullHeight = height;
	if (filmSubRegion) {
		filmSubRegion[0] = subRegion[0];
		filmSubRegion[1] = subRegion[1];
		filmSubRegion[2] = subRegion[2];
		filmSubRegion[3] = subRegion[3];
	}
}

void RenderConfig::GetScreenSize(u_int *screenWidth, u_int *screenHeight) const {
	u_int filmWidth, filmHeight, filmSubRegion[4];
	GetFilmSize(&filmWidth, &filmHeight, filmSubRegion);

	*screenWidth = filmSubRegion[1] - filmSubRegion[0] + 1;
	*screenHeight = filmSubRegion[3] - filmSubRegion[2] + 1;
}

Sampler *RenderConfig::AllocSampler(RandomGenerator *rndGen, Film *film,
		double *metropolisSharedTotalLuminance, double *metropolisSharedSampleCount) const {
	//--------------------------------------------------------------------------
	// Sampler
	//--------------------------------------------------------------------------

	const SamplerType samplerType = Sampler::String2SamplerType(cfg.GetString("sampler.type",
			cfg.GetString("path.sampler.type", "INLINED_RANDOM")));
	switch (samplerType) {
		case RANDOM:
			return new InlinedRandomSampler(rndGen, film);
		case METROPOLIS: {
			const float rate = cfg.GetFloat("sampler.largesteprate",
					cfg.GetFloat("path.sampler.largesteprate", .4f));
			const float reject = cfg.GetFloat("sampler.maxconsecutivereject",
					cfg.GetFloat("path.sampler.maxconsecutivereject", 512));
			const float mutationrate = cfg.GetFloat("sampler.imagemutationrate",
					cfg.GetFloat("path.sampler.imagemutationrate", .1f));

			return new MetropolisSampler(rndGen, film, reject, rate, mutationrate,
					metropolisSharedTotalLuminance, metropolisSharedSampleCount);
		}
		default:
			throw std::runtime_error("Unknown sampler.type: " + samplerType);
	}
}
