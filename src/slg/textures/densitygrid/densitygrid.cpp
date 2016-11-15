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

#include <sstream>
#include <algorithm>
#include <numeric>
#include <memory>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include "slg/textures/densitygrid/densitygrid.h"
#include "slg/textures/densitygrid/densitygridcache.h"
#include "slg/core/sdl.h"
#include "luxrays/utils/properties.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// DensityGridStorage
//------------------------------------------------------------------------------

const float DensityGridStorage::GetFloat(const u_int index) const {
	assert(index >= 0);
	assert(index < nx * ny * nz);

	return voxels[index];
};

const luxrays::Spectrum DensityGridStorage::GetSpectrum(const u_int index) const {
	assert(index >= 0);
	assert(index < nx * ny * nz);

	return luxrays::Spectrum(voxels[index]);
};

const float DensityGridStorage::GetVoxel(const int s, const int t, const int u) const  {
	const u_int index = ((luxrays::Clamp<int>(u, 0, nz - 1) * ny) + luxrays::Clamp<int>(t, 0, ny - 1)) * nx + luxrays::Clamp<int>(s, 0, nx - 1);
	
	assert(index >= 0);
	assert(index < nx * ny * nz);

	return voxels[index];
};



//------------------------------------------------------------------------------
// DensityGrid
//------------------------------------------------------------------------------

DensityGrid::DensityGrid(const u_int nx, const u_int ny, const u_int nz, const float *dt, const std::string wm) {
	voxelStorage = new DensityGridStorage(nx, ny, nz, dt);
	
	wrapMode = slg::ocl::WRAP_REPEAT;
	if (wm == "black") { wrapMode = slg::ocl::WRAP_BLACK; }
	else if (wm == "white") { wrapMode = slg::ocl::WRAP_WHITE; }
	else if (wm == "clamp") wrapMode = slg::ocl::WRAP_CLAMP;

}

DensityGrid::~DensityGrid() {
	delete voxelStorage;
}


float DensityGrid::GetFloat(const u_int x, const u_int y, const u_int z) const {
	const u_int index = ((luxrays::Clamp<u_int>(z, 0, GetNz() - 1) * GetNy()) + luxrays::Clamp<u_int>(y, 0, GetNy() - 1)) * GetNx() + luxrays::Clamp<u_int>(x, 0, GetNx() - 1);

	return GetStorage()->GetFloat(index);
}


Properties DensityGrid::ToProperties(const std::string &prefix) const {
	Properties props;

	props <<
		Property(prefix + ".blob")(Blob((char *)&(voxelStorage->GetVoxelsData()[0]), voxelStorage->GetMemorySize())) <<
		Property(prefix + ".blob.width")(GetNx()) <<
		Property(prefix + ".blob.height")(GetNy()) <<
		Property(prefix + ".blob.depth")(GetNz());

	return props;
}
