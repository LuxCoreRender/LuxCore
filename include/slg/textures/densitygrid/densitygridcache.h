/***************************************************************************
 * Copyright 1998-2016 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_DENSITYGRIDCACHE_H
#define	_SLG_DENSITYGRIDCACHE_H

#include <string>
#include <vector>

#include <boost/unordered_map.hpp>

#include "slg/textures/densitygrid/densitygrid.h"

namespace slg {

//------------------------------------------------------------------------------
// DensityGridCache
//------------------------------------------------------------------------------

class DensityGridCache {
public:
	DensityGridCache();
	~DensityGridCache();

	void DefineDensityGrid(const std::string &name, DensityGrid *dg);

	DensityGrid *GetDensityGrid(const std::string &name, const u_int nx, const u_int ny, const u_int nz, const float *dt, const std::string wm);
	void DeleteDensityGrid(const DensityGrid *dg) {
		for (boost::unordered_map<std::string, DensityGrid *>::iterator it = mapByName.begin(); it != mapByName.end(); ++it) {
			if (it->second == dg) {
				delete it->second;

				maps.erase(std::find(maps.begin(), maps.end(), it->second));
				mapByName.erase(it);
				return;
			}
		}
	}

	u_int GetDensityGridIndex(const DensityGrid *dg) const;

	void GetDensityGrids(std::vector<const DensityGrid *> &dgs);
	u_int GetSize()const { return static_cast<u_int>(mapByName.size()); }
	bool IsDensityGridDefined(const std::string &name) const { return mapByName.find(name) != mapByName.end(); }

private:	
	std::string GetCacheKey(const std::string &fileName) const;

	boost::unordered_map<std::string, DensityGrid *> mapByName;
	// Used to preserve insertion order and to retrieve insertion index
	std::vector<DensityGrid *> maps;
};

}

#endif	/* _SLG_DENSITYGRIDCACHE_H */
