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

#ifndef _SLG_DENSITYGRID_H
#define	_SLG_DENSITYGRID_H

#include <string>
#include <limits>

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/utils/properties.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/textures/densitygrid/texture_densitygrid_types.cl"
}

//------------------------------------------------------------------------------
// DensityGridStorage
//------------------------------------------------------------------------------

class DensityGridStorage {
public:
	DensityGridStorage(const u_int x, const u_int y, const u_int z, const float *dt) {
		nx = x;
		ny = y;
		nz = z;
		voxels.assign(dt, dt + nx * ny * nz);
	}

	virtual ~DensityGridStorage() { }

	u_int GetNx() { return nx; }
	u_int GetNy() { return ny; }
	u_int GetNz() { return nz; }

	size_t GetMemorySize() const { return nx * ny * nz * sizeof(float); }
	const std::vector<float> &GetVoxelsData() const { return voxels; }

	const float GetFloat(const u_int index) const;
	const luxrays::Spectrum GetSpectrum(const u_int index) const;
	const float GetVoxel(const int s, const int t, const int u) const;

private:
	u_int nx, ny, nz;
	std::vector<float> voxels;
};

//------------------------------------------------------------------------------
// DensityGrid
//------------------------------------------------------------------------------

class DensityGridCache;

class DensityGrid {
public:	
	DensityGrid(const u_int nx, const u_int ny, const u_int nz, const float *dt, const std::string wm);

	virtual ~DensityGrid();

	u_int GetNx() const { return voxelStorage->GetNx(); }
	u_int GetNy() const { return voxelStorage->GetNy(); }
	u_int GetNz() const { return voxelStorage->GetNz(); }
	slg::ocl::WrapModeType GetWrapMode() const { return wrapMode; }

	const DensityGridStorage *GetStorage() const { return voxelStorage; }

	luxrays::Properties ToProperties(const std::string &prefix) const;

	float GetFloat(const u_int index) const { return voxelStorage->GetFloat(index); }
	float GetFloat(const u_int x, const u_int y, const u_int z) const;

	luxrays::Spectrum GetSpectrum(const u_int index) const { return voxelStorage->GetSpectrum(index); }

private:
	DensityGrid(DensityGridStorage *voxel);

	DensityGridStorage *voxelStorage;
	slg::ocl::WrapModeType wrapMode;
};

}

#endif	/* _SLG_DENSITYGRID_H */
