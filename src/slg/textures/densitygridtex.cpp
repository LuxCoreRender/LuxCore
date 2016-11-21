/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include "slg/textures/densitygridtex.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Densitygrid texture
//------------------------------------------------------------------------------

DensityGridTexture::DensityGridTexture(const TextureMapping3D *mp, const DensityGrid *dg) :
		densityGrid(dg), mapping(mp) {

}

DensityGridTexture::~DensityGridTexture() {
	delete mapping; 
}

float DensityGridTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point P(mapping->Map(hitPoint));

	float x, y, z;
	int vx, vy, vz;

	const int nx = densityGrid->GetNx();
	const int ny = densityGrid->GetNy();
	const int nz = densityGrid->GetNz();

	switch (densityGrid->GetWrapMode()) {
		case slg::ocl::WRAP_REPEAT:
			x = P.x * nx;
			vx = luxrays::Floor2Int(x);
			x -= vx;
			vx = luxrays::Mod(vx, nx);
			y = P.y * ny;
			vy = luxrays::Floor2Int(y);
			y -= vy;
			vy = luxrays::Mod(vy, ny);
			z = P.z * nz;
			vz = luxrays::Floor2Int(z);
			z -= vz;
			vz = luxrays::Mod(vz, nz);
			break;
		case slg::ocl::WRAP_BLACK:
			if (P.x < 0.f || P.x >= 1.f ||
				P.y < 0.f || P.y >= 1.f ||
				P.z < 0.f || P.z >= 1.f)
				return 0.f;
			x = P.x * nx;
			vx = luxrays::Floor2Int(x);
			x -= vx;
			y = P.y * ny;
			vy = luxrays::Floor2Int(y);
			y -= vy;
			z = P.z * nz;
			vz = luxrays::Floor2Int(z);
			z -= vz;
			break;
		case slg::ocl::WRAP_WHITE:
			if (P.x < 0.f || P.x >= 1.f ||
				P.y < 0.f || P.y >= 1.f ||
				P.z < 0.f || P.z >= 1.f)
				return 1.f;
			x = P.x * nx;
			vx = luxrays::Floor2Int(x);
			x -= vx;
			y = P.y * ny;
			vy = luxrays::Floor2Int(y);
			y -= vy;
			z = P.z * nz;
			vz = luxrays::Floor2Int(z);
			z -= vz;
			break;
		case slg::ocl::WRAP_CLAMP:
			x = luxrays::Clamp(P.x, 0.f, 1.f) * nx;
			vx = min(luxrays::Floor2Int(x), nx - 1);
			x -= vx;
			y = luxrays::Clamp(P.y, 0.f, 1.f) * ny;
			vy = min(luxrays::Floor2Int(P.y * ny), ny - 1);
			y -= vy;
			z = luxrays::Clamp(P.z, 0.f, 1.f) * nz;
			vz = min(luxrays::Floor2Int(P.z * nz), nz - 1);
			z -= vz;
			break;
		default:
			return 0.f;
	}

	// Trilinear interpolation of the grid element
	return luxrays::Lerp(z,
		luxrays::Lerp(y, luxrays::Lerp(x, D(vx, vy, vz), D(vx + 1, vy, vz)), luxrays::Lerp(x, D(vx, vy + 1, vz), D(vx + 1, vy + 1, vz))),
		luxrays::Lerp(y, luxrays::Lerp(x, D(vx, vy, vz + 1), D(vx + 1, vy, vz + 1)), luxrays::Lerp(x, D(vx, vy + 1, vz + 1), D(vx + 1, vy + 1, vz + 1))));
}

Spectrum DensityGridTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties DensityGridTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;
	std::string wrap = "";

    switch (densityGrid->GetWrapMode()) {
		default:
		case slg::ocl::WRAP_REPEAT:
		    wrap = "repeat";
			break;
		case slg::ocl::WRAP_BLACK:
		    wrap = "black";
			break;
		case slg::ocl::WRAP_WHITE:
		    wrap = "white";
			break;
		case slg::ocl::WRAP_CLAMP:
		    wrap = "clamp";
			break;
    }

	const string name = GetName();
	const int nx = densityGrid->GetNx();
	const int ny = densityGrid->GetNy();
	const int nz = densityGrid->GetNz();
	vector<float> data = densityGrid->GetStorage()->GetVoxelsData();

	props.Set(Property("scene.textures." + name + ".type")("densitygrid"));
	props.Set(Property("scene.textures." + name + ".nx")(nx));
	props.Set(Property("scene.textures." + name + ".ny")(ny));
	props.Set(Property("scene.textures." + name + ".nz")(nz));
	props.Set(Property("scene.textures." + name + ".wrap")(wrap));
	props.Set(Property("scene.textures." + name + ".data")(data));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
