/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include "slg/textures/densitygrid.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Densitygrid texture
//------------------------------------------------------------------------------

DensityGridTexture::DensityGridTexture(const TextureMapping3D *mp,
		const u_int nx, const u_int ny, const u_int nz,
        const ImageMap *map) : mapping(mp),
		nx(nx), ny(ny), nz(nz), imageMap(map) {
}

float DensityGridTexture::D(const float *data, int x, int y, int z) const {
	return data[((Clamp(z, 0, nz - 1) * ny) + Clamp(y, 0, ny - 1)) * nx + Clamp(x, 0, nx - 1)];
}

float DensityGridTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point P(mapping->Map(hitPoint));

	float x, y, z;
	int vx, vy, vz;

	switch (imageMap->GetStorage()->wrapType) {
		case ImageMapStorage::REPEAT:
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
		case ImageMapStorage::BLACK:
			if (P.x < 0.f || P.x >= 1.f ||
				P.y < 0.f || P.y >= 1.f ||
				P.z < 0.f || P.z >= 1.f)
				return 0.f;
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
		case ImageMapStorage::WHITE:
			if (P.x < 0.f || P.x >= 1.f ||
				P.y < 0.f || P.y >= 1.f ||
				P.z < 0.f || P.z >= 1.f)
				return 1.f;
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
		case ImageMapStorage::CLAMP:
			x = Clamp(P.x, 0.f, 1.f) * nx;
			vx = Min(Floor2Int(x), nx - 1);
			x -= vx;
			y = Clamp(P.y, 0.f, 1.f) * ny;
			vy = Min(Floor2Int(P.y * ny), ny - 1);
			y -= vy;
			z = Clamp(P.z, 0.f, 1.f) * nz;
			vz = Min(Floor2Int(P.z * nz), nz - 1);
			z -= vz;
			break;
		default:
			return 0.f;
	}

	// Trilinear interpolation of the grid element
	const float *data = (float *)imageMap->GetStorage()->GetPixelsData();

	return Lerp(z,
		Lerp(y, Lerp(x, D(data, vx, vy, vz), D(data, vx + 1, vy, vz)), Lerp(x, D(data, vx, vy + 1, vz), D(data, vx + 1, vy + 1, vz))),
		Lerp(y, Lerp(x, D(data, vx, vy, vz + 1), D(data, vx + 1, vy, vz + 1)), Lerp(x, D(data, vx, vy + 1, vz + 1), D(data, vx + 1, vy + 1, vz + 1))));
}

Spectrum DensityGridTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties DensityGridTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("densitygrid"));
	props.Set(Property("scene.textures." + name + ".nx")(nx));
	props.Set(Property("scene.textures." + name + ".ny")(ny));
	props.Set(Property("scene.textures." + name + ".nz")(nz));
	props.Set(Property("scene.textures." + name + ".wrap")(ImageMapStorage::WrapType2String(imageMap->GetStorage()->wrapType)));
	
	Property dataProp("scene.textures." + name + ".data");
	const float *img = (float *)imageMap->GetStorage()->GetPixelsData();
		
	for (int z = 0; z < nz; ++z)
		for (int y = 0; y < ny; ++y)
			for (int x = 0; x < nx; ++x)
				dataProp.Add<float>(img[(z * ny + y) * nx + x]);

	props.Set(dataProp);
	
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
