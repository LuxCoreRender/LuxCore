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

#include "renderconfig.h"

#include "luxrays/utils/film/film.h"

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

Sampler *RenderConfig::AllocSampler(RandomGenerator *rndGen, Film *film) const {
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

			return new MetropolisSampler(rndGen, film, reject, rate, mutationrate);
		}
		default:
			throw std::runtime_error("Unknown sampler.type: " + samplerType);
	}
}
