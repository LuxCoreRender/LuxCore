#line 2 "texture_densitygrid_funcs.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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
// DensityGrid texture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 DensityGridTexture_D(
		__global const ImageMap *imageMap,
		int x, int y, int z,
		int nx, int ny, int nz
		IMAGEMAPS_PARAM_DECL) {
	__global const void *pixels = ImageMap_GetPixelsAddress(
		imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);
	const ImageMapStorageType storageType = imageMap->storageType;
	const uint channelCount = imageMap->channelCount;

	const uint index = ((clamp(z, 0, nz - 1) * ny) + clamp(y, 0, ny - 1)) * nx + clamp(x, 0, nx - 1);
	
	return ImageMap_GetTexel_SpectrumValue(storageType, pixels, channelCount, index);
}

OPENCL_FORCE_NOT_INLINE float3 DensityGridTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const int nx, const int ny, const int nz,
		const uint imageMapIndex, __global const TextureMapping3D *mapping
		TEXTURES_PARAM_DECL) {
	__global const ImageMap *imageMap = &imageMapDescs[imageMapIndex];

	const float3 P = TextureMapping3D_Map(mapping, hitPoint, NULL TEXTURES_PARAM);

	float x, y, z;
	int vx, vy, vz;

	switch (imageMap->wrapType) {
		case WRAP_REPEAT:
			x = P.x * nx;
			vx = Floor2Int(x);
			x -= vx;
			vx = Mod(vx, nx);
			y = P.y * ny;
			vy = Floor2Int(y);
			y -= vy;
			vy = Mod(vy, ny);
			z = P.z * nz;
			vz = Floor2Int(z);
			z -= vz;
			vz = Mod(vz, nz);
			break;
		case WRAP_BLACK:
			if (P.x < 0.f || P.x >= 1.f ||
				P.y < 0.f || P.y >= 1.f ||
				P.z < 0.f || P.z >= 1.f)
				return BLACK;
			x = P.x * nx;
			vx = Floor2Int(x);
			x -= vx;
			y = P.y * ny;
			vy = Floor2Int(y);
			y -= vy;
			z = P.z * nz;
			vz = Floor2Int(z);
			z -= vz;
			break;
		case WRAP_WHITE:
			if (P.x < 0.f || P.x >= 1.f ||
				P.y < 0.f || P.y >= 1.f ||
				P.z < 0.f || P.z >= 1.f)
				return WHITE;
			x = P.x * nx;
			vx = Floor2Int(x);
			x -= vx;
			y = P.y * ny;
			vy = Floor2Int(y);
			y -= vy;
			z = P.z * nz;
			vz = Floor2Int(z);
			z -= vz;
			break;
		case WRAP_CLAMP:
			x = clamp(P.x, 0.f, 1.f) * nx;
			vx = min(Floor2Int(x), nx - 1);
			x -= vx;
			y = clamp(P.y, 0.f, 1.f) * ny;
			vy = min(Floor2Int(P.y * ny), ny - 1);
			y -= vy;
			z = clamp(P.z, 0.f, 1.f) * nz;
			vz = min(Floor2Int(P.z * nz), nz - 1);
			z -= vz;
			break;
		default:
			return BLACK;
	}

	// Trilinear interpolation of the grid element
	return
		mix(
			mix(
				mix(
					DensityGridTexture_D(imageMap, vx, vy, vz, nx, ny, nz IMAGEMAPS_PARAM),
					DensityGridTexture_D(imageMap, vx + 1, vy, vz, nx, ny, nz IMAGEMAPS_PARAM),
				x),
				mix(
					DensityGridTexture_D(imageMap, vx, vy + 1, vz, nx, ny, nz IMAGEMAPS_PARAM),
					DensityGridTexture_D(imageMap, vx + 1, vy + 1, vz, nx, ny, nz IMAGEMAPS_PARAM),
				x),
			y),
			mix(
				mix(
					DensityGridTexture_D(imageMap, vx, vy, vz + 1, nx, ny, nz IMAGEMAPS_PARAM),
					DensityGridTexture_D(imageMap, vx + 1, vy, vz + 1, nx, ny, nz IMAGEMAPS_PARAM),
				x),
				mix(
					DensityGridTexture_D(imageMap, vx, vy + 1, vz + 1, nx, ny, nz IMAGEMAPS_PARAM),
					DensityGridTexture_D(imageMap, vx + 1, vy + 1, vz + 1, nx, ny, nz IMAGEMAPS_PARAM),
				x),
			y),
		z);
}

OPENCL_FORCE_INLINE float DensityGridTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const int nx, const int ny, const int nz,
		const uint imageMapIndex, __global const TextureMapping3D *mapping
		TEXTURES_PARAM_DECL) {
	return Spectrum_Y(DensityGridTexture_ConstEvaluateSpectrum(hitPoint,
			nx, ny, nz,
			imageMapIndex, mapping
			TEXTURES_PARAM));
}
