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

#include <memory>

#include <openvdb/openvdb.h>
#include <openvdb/io/File.h>
#include <openvdb/tools/Interpolation.h>

#include "slg/textures/densitygrid.h"
#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// DensityGrid texture
//------------------------------------------------------------------------------

DensityGridTexture::DensityGridTexture(const TextureMapping3D *mp,
		const u_int nx, const u_int ny, const u_int nz,
        const ImageMap *map) : mapping(mp),
		nx(nx), ny(ny), nz(nz), imageMap(map) {
}

ImageMap *DensityGridTexture::ParseData(const luxrays::Property &dataProp,
		const bool isRGB,
		const u_int nx, const u_int ny, const u_int nz,
		const ImageMapStorage::StorageType storageType,
		const ImageMapStorage::WrapType wrapMode) {
	// Create an image map with the data

	// NOTE: wrapMode is only stored inside the ImageMap but is not then used to
	// sample the image. The image data are accessed directly and the wrapping is
	// implemented by the code accessing the data.

	const u_int channelCount = isRGB ? 3 : 1;
	unique_ptr<ImageMap> imgMap(ImageMap::AllocImageMap(channelCount, nx, ny * nz,
			ImageMapConfig(1.f,
				(storageType == ImageMapStorage::AUTO) ? ImageMapStorage::HALF : storageType,
				wrapMode,
				ImageMapStorage::ChannelSelectionType::DEFAULT)));

	ImageMapStorage *imgStorage = imgMap->GetStorage();

	if (isRGB) {
		for (u_int z = 0, i = 0; z < nz; ++z)
			for (u_int y = 0; y < ny; ++y)
				for (u_int x = 0; x < nx; ++x, ++i) {
					const float r = dataProp.Get<float>(i * 3 + 0);
					const float g = dataProp.Get<float>(i * 3 + 1);
					const float b = dataProp.Get<float>(i * 3 + 2);

					imgStorage->SetSpectrum((z * ny + y) * nx + x, Spectrum(r, g, b));
				}
	} else {
		for (u_int z = 0, i = 0; z < nz; ++z)
			for (u_int y = 0; y < ny; ++y)
				for (u_int x = 0; x < nx; ++x, ++i)
					imgStorage->SetFloat((z * ny + y) * nx + x, dataProp.Get<float>(i));
	}

	return imgMap.release();
}

ImageMap *DensityGridTexture::ParseOpenVDB(const string &fileName, const string &gridName,
		const u_int nx, const u_int ny, const u_int nz,
		const ImageMapStorage::StorageType storageType,
		const ImageMapStorage::WrapType wrapMode) {
	SDL_LOG("OpenVDB file: " + fileName);

	openvdb::io::File file(fileName);	
	file.open();

	// List the grids included in the file
	SDL_LOG("OpenVDB grid names:");
	for (auto i = file.beginName(); i != file.endName(); ++i)
		SDL_LOG("  [" + *i + "]");

	// Check if the file has the specified grid
	if (!file.hasGrid(gridName))
		throw runtime_error("Unknown grid " + gridName + " in OpenVDB file " + fileName);
	
	// Read the grid from the file
	openvdb::GridBase::Ptr ovdbGrid = file.readGrid(gridName);

	// Compute the scale factor
	openvdb::CoordBBox gridBBox;
	ovdbGrid->baseTree().evalLeafBoundingBox(gridBBox);

	//const openvdb::CoordBBox gridBBox = ovdbGrid->evalActiveVoxelBoundingBox();
	SDL_LOG("OpenVDB grid bbox: "
			"[(" << gridBBox.min()[0] << ", " << gridBBox.min()[1] << ", " << gridBBox.min()[2] << "), "
			"(" << gridBBox.max()[0] << ", " << gridBBox.max()[1] << ", " << gridBBox.max()[2] << ")]");
	const openvdb::Coord gridBBoxSize = gridBBox.max() - gridBBox.min();
	SDL_LOG("OpenVDB grid size: (" << gridBBoxSize[0] << ", " << gridBBoxSize[1] << ", " << gridBBoxSize[2] << ")");
	
	const openvdb::Vec3f scale = openvdb::Vec3f(
		gridBBoxSize[0] / (float)nx,
		gridBBoxSize[1] / (float)ny,
		gridBBoxSize[2] / (float)nz);


	SDL_LOG("OpenVDB grid type: " + ovdbGrid->valueType());
	const u_int channelsCount =
			((ovdbGrid->valueType() == "vec3s") ||
			(ovdbGrid->valueType() == "vec3f") ||
			(ovdbGrid->valueType() == "vec3d")) ? 3 : 1;

	// NOTE: wrapMode is only stored inside the ImageMap but is not then used to
	// sample the image. The image data are accessed directly and the wrapping is
	// implemented by the code accessing the data.

	unique_ptr<ImageMap> imgMap(ImageMap::AllocImageMap(channelsCount, nx, ny * nz,
			ImageMapConfig(1.f,
				storageType,
				wrapMode,
				ImageMapStorage::ChannelSelectionType::DEFAULT)));

	ImageMapStorage *imgStorage = imgMap->GetStorage();	

	if (channelsCount == 3) {
		// Check if it is the right type of grid
		openvdb::VectorGrid::Ptr grid = openvdb::gridPtrCast<openvdb::VectorGrid>(ovdbGrid);
		if (!grid)
			throw runtime_error("Wrong OpenVDB file type in parsing file " + fileName + " for grid " + gridName);

		#pragma omp parallel for
		for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int z = 0; z < nz; ++z) {
			for (u_int y = 0; y < ny; ++y) {
				for (u_int x = 0; x < nx; ++x) {
					const openvdb::Vec3f xyz = scale * openvdb::Vec3f(x, y, z) + gridBBox.min();

					openvdb::VectorGrid::ValueType v;
					openvdb::tools::QuadraticSampler::sample(grid->tree(), xyz, v);
					openvdb::Vec3f v3f = v;

					imgStorage->SetSpectrum((z * ny + y) * nx + x, Spectrum(v3f.x(), v3f.y(), v3f.z()));
				}
			}
		}
	} else {
		// Check if it is the right type of grid
		openvdb::ScalarGrid::Ptr grid = openvdb::gridPtrCast<openvdb::ScalarGrid>(ovdbGrid);
		if (!grid)
			throw runtime_error("Wrong OpenVDB file type in parsing file " + fileName + " for grid " + gridName);

		#pragma omp parallel for
		for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int z = 0; z < nz; ++z) {
			for (u_int y = 0; y < ny; ++y) {
				for (u_int x = 0; x < nx; ++x) {
					const openvdb::Vec3f xyz = scale * openvdb::Vec3f(x, y, z) + gridBBox.min();

					openvdb::ScalarGrid::ValueType v;
					openvdb::tools::QuadraticSampler::sample(grid->tree(), xyz, v);

					imgStorage->SetFloat((z * ny + y) * nx + x, v);
				}
			}
		}
	}

	file.close();

	return imgMap.release();
}

Spectrum DensityGridTexture::D(int x, int y, int z) const {
	return imageMap->GetStorage()->GetSpectrum(((Clamp(z, 0, nz - 1) * ny) + Clamp(y, 0, ny - 1)) * nx + Clamp(x, 0, nx - 1));
}

Spectrum DensityGridTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
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
	return Lerp(z,
		Lerp(y, Lerp(x, D(vx, vy, vz), D(vx + 1, vy, vz)), Lerp(x, D(vx, vy + 1, vz), D(vx + 1, vy + 1, vz))),
		Lerp(y, Lerp(x, D(vx, vy, vz + 1), D(vx + 1, vy, vz + 1)), Lerp(x, D(vx, vy + 1, vz + 1), D(vx + 1, vy + 1, vz + 1))));
}

float DensityGridTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
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
	const ImageMapStorage *imgStorage = imageMap->GetStorage();

	for (int z = 0; z < nz; ++z)
		for (int y = 0; y < ny; ++y)
			for (int x = 0; x < nx; ++x)
				dataProp.Add<float>(imgStorage->GetFloat((z * ny + y) * nx + x));

	props.Set(dataProp);
	
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
