/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include "slg/textures/densitygrid/densitygridcache.h"
#include "slg/core/sdl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// DensityGridCache
//------------------------------------------------------------------------------

DensityGridCache::DensityGridCache() {	}

DensityGridCache::~DensityGridCache() {
	BOOST_FOREACH(DensityGrid *m, maps)
		delete m;
}

string DensityGridCache::GetCacheKey(const string &name) const {
	return name;
}


void DensityGridCache::DefineDensityGrid(const string &name, DensityGrid *dg) {
	SDL_LOG("Define DensityGrid: " << name);

	// Compose the cache key
	const string key = GetCacheKey(name);

	boost::unordered_map<std::string, DensityGrid *>::const_iterator it = mapByName.find(key);
	if (it == mapByName.end()) {
		// Add the new image definition
		mapByName.insert(make_pair(key, dg));
		maps.push_back(dg);
	} else {
		// Overwrite the existing densityGrid definition
		const u_int index = GetDensityGridIndex(it->second);
		delete maps[index];
		maps[index] = dg;

		// I have to modify mapByName for last or it iterator would be modified
		// otherwise (it->second would point to the new DensityGrid and not to the old one)
		mapByName.erase(key);
		mapByName.insert(make_pair(key, dg));
	}
}

u_int DensityGridCache::GetDensityGridIndex(const DensityGrid *dg) const {
	for (u_int i = 0; i < maps.size(); ++i) {
		if (maps[i] == dg)
			return i;
	}

	throw runtime_error("Unknown density grid: " + boost::lexical_cast<string>(dg));
}

DensityGrid *DensityGridCache::GetDensityGrid(const std::string &name, const u_int nx, const u_int ny, const u_int nz, const float *dt, std::string wm) {
	// Compose the cache key
	string key = GetCacheKey(name);

	// Check if the density grid has been already defined
	boost::unordered_map<std::string, DensityGrid *>::const_iterator it = mapByName.find(key);

	if (it != mapByName.end()) {
		//SDL_LOG("Cached defined image map: " << name);
		DensityGrid *dg = (it->second);
		return dg;
	}

	// I haven't yet loaded the density grid
	DensityGrid *dg = new DensityGrid(nx, ny, nz, dt, wm);


	mapByName.insert(make_pair(key, dg));
	maps.push_back(dg);

	return dg;
}

void DensityGridCache::GetDensityGrids(vector<const DensityGrid *> &dgs) {
	dgs.reserve(maps.size());

	BOOST_FOREACH(DensityGrid *dg, maps)
		dgs.push_back(dg);
}
