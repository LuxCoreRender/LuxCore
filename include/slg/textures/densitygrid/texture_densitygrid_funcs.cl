#line 2 "densitygrid_funcs.cl"

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

//------------------------------------------------------------------------------
// DensityGrids support
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_DENSITYGRID) 

__global const void *DensityGrid_GetPixelsAddress(__global const float* restrict* restrict densityGridBuff,
		const uint page, const uint offset) {
	return &densityGridBuff[page][offset];
}


float DensityGrid_GetVoxel(__global const void *voxels,
						   const uint nx, const uint ny, const uint nz,
						   const int x, const  int y, const  int z) {
	const uint u = Mod(x, nx);
	const uint v = Mod(y, ny);
	const uint w = Mod(z, nz);
			
	const uint index = ((w * ny) + v) *nx + u;

	const float a = ((__global const float *)voxels)[index];

	return a;
}

float DensityGridTexture_ConstEvaluateFloat(__global HitPoint *hitPoint, unsigned int densityGridIndex, __global const TextureMapping3D *mapping DENSITYGRIDS_PARAM_DECL) {
	
	__global const DensityGrid *densityGrid = &densityGridDescs[densityGridIndex];
	__global const void *voxels = DensityGrid_GetPixelsAddress(densityGridBuff, densityGrid->pageIndex, densityGrid->voxelsIndex);

	const float3 P = TextureMapping3D_Map(mapping, hitPoint);

	
	float x, y, z;
	int vx, vy, vz;

	switch (densityGrid->wrapMode) {
		case WRAP_REPEAT:
			x = P.x * densityGrid->nx;
			vx = Floor2Int(x);
			x -= vx;
			vx = Mod(vx, densityGrid->nx);
			y = P.y * densityGrid->ny;
			vy = Floor2Int(y);
			y -= vy;
			vy = Mod(vy, densityGrid->ny);
			z = P.z * densityGrid->nz;
			vz = Floor2Int(z);
			z -= vz;
			vz = Mod(vz, densityGrid->nz);
			break;
		case WRAP_BLACK:
			if (P.x < 0.f || P.x >= 1.f ||
				P.y < 0.f || P.y >= 1.f ||
				P.z < 0.f || P.z >= 1.f)
				return 0.f;
			x = P.x * densityGrid->nx;
			vx = Floor2Int(x);
			x -= vx;
			y = P.y * densityGrid->ny;
			vy = Floor2Int(y);
			y -= vy;
			z = P.z * densityGrid->nz;
			vz = Floor2Int(z);
			z -= vz;
			break;
		case WRAP_WHITE:
			if (P.x < 0.f || P.x >= 1.f ||
				P.y < 0.f || P.y >= 1.f ||
				P.z < 0.f || P.z >= 1.f)
				return 1.f;
			x = P.x * densityGrid->nx;
			vx = Floor2Int(x);
			x -= vx;
			y = P.y * densityGrid->ny;
			vy = Floor2Int(y);
			y -= vy;
			z = P.z * densityGrid->nz;
			vz = Floor2Int(z);
			z -= vz;
			break;
		case WRAP_CLAMP:
			x = clamp(P.x, 0.f, 1.f) * densityGrid->nx;
			vx = min(Floor2Int(x), (int)densityGrid->nx - 1);
			x -= vx;
			y = clamp(P.y, 0.f, 1.f) * densityGrid->ny;
			vy = min(Floor2Int(P.y * densityGrid->ny), (int)densityGrid->ny - 1);
			y -= vy;
			z = clamp(P.z, 0.f, 1.f) * densityGrid->nz;
			vz = min(Floor2Int(P.z * densityGrid->nz), (int)densityGrid->nz - 1);
			z -= vz;
			break;

		default:
			return 0.f;
	}

	// Trilinear interpolation of the grid element
	return Lerp(z,
		Lerp(y, 
			Lerp(x, DensityGrid_GetVoxel(voxels, densityGrid->nx, densityGrid->ny, densityGrid->nz, vx, vy, vz), DensityGrid_GetVoxel(voxels, densityGrid->nx, densityGrid->ny, densityGrid->nz, vx + 1, vy, vz)),
			Lerp(x, DensityGrid_GetVoxel(voxels, densityGrid->nx, densityGrid->ny, densityGrid->nz, vx, vy + 1, vz), DensityGrid_GetVoxel(voxels, densityGrid->nx, densityGrid->ny, densityGrid->nz, vx + 1, vy + 1, vz))),
		Lerp(y,
			Lerp(x, DensityGrid_GetVoxel(voxels, densityGrid->nx, densityGrid->ny, densityGrid->nz, vx, vy, vz + 1), DensityGrid_GetVoxel(voxels, densityGrid->nx, densityGrid->ny, densityGrid->nz, vx + 1, vy, vz + 1)),
			Lerp(x, DensityGrid_GetVoxel(voxels, densityGrid->nx, densityGrid->ny, densityGrid->nz, vx, vy + 1, vz + 1), DensityGrid_GetVoxel(voxels, densityGrid->nx, densityGrid->ny, densityGrid->nz, vx + 1, vy + 1, vz + 1))));
}

float3 DensityGridTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint, unsigned int densityGridIndex, __global const TextureMapping3D *mapping DENSITYGRIDS_PARAM_DECL) {
	
	return  DensityGridTexture_ConstEvaluateFloat(hitPoint, densityGridIndex, mapping DENSITYGRIDS_PARAM);
}

#endif
