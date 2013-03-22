/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#ifndef _SLG_COMPILEDSESSION_H
#define	_SLG_COMPILEDSESSION_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <set>

#include "slg/slg.h"
#include "slg/engines/pathocl/ocldatatypes.h"
#include "slg/editaction.h"

#include "slg/film/film.h"
#include "slg/sdl/scene.h"

namespace slg {

class CompiledScene {
public:
	CompiledScene(Scene *scn, Film *flm, const size_t maxMemPageS);
	~CompiledScene();

	void Recompile(const EditActionList &editActions);
	bool IsMaterialCompiled(const MaterialType type) const;
	bool IsTextureCompiled(const TextureType type) const;

	bool RequiresPassThrough() {
		return (IsMaterialCompiled(GLASS) ||
				IsMaterialCompiled(ARCHGLASS) ||
				IsMaterialCompiled(MIX) ||
				IsMaterialCompiled(NULLMAT) ||
				IsMaterialCompiled(MATTETRANSLUCENT) ||
				IsMaterialCompiled(GLOSSY2) ||
				IsMaterialCompiled(ROUGHGLASS));
	}

	Scene *scene;
	Film *film;
	u_int maxMemPageSize;

	// Compiled Camera
	slg::ocl::Camera camera;

	// Compiled Scene Geometry
	vector<luxrays::Point> verts;
	vector<luxrays::Normal> normals;
	vector<luxrays::UV> uvs;
	vector<luxrays::Spectrum> cols;
	vector<float> alphas;
	vector<luxrays::Triangle> tris;
	vector<luxrays::ocl::Mesh> meshDescs;
	const luxrays::TriangleMeshID *meshIDs;
	// One element for each mesh, it used to translate the RayBuffer currentTriangleIndex
	// to mesh TriangleID (with MQBVH)
	u_int *meshFirstTriangleOffset;

	// Compiled AreaLights
	vector<slg::ocl::TriangleLight> triLightDefs;
	vector<u_int> meshLights;

	// Compiled InfiniteLights
	slg::ocl::InfiniteLight *infiniteLight;

	// Compiled SunLight
	slg::ocl::SunLight *sunLight;
	// Compiled SkyLight
	slg::ocl::SkyLight *skyLight;

	// Compiled Materials
	std::set<MaterialType> usedMaterialTypes;
	vector<slg::ocl::Material> mats;
	vector<u_int> meshMats;
	bool useBumpMapping, useNormalMapping;

	// Compiled Textures
	std::set<TextureType> usedTextureTypes;
	vector<slg::ocl::Texture> texs;

	// Compiled ImageMaps
	vector<slg::ocl::ImageMap> imageMapDescs;
	vector<vector<float> > imageMapMemBlocks;

private:
	void CompileCamera();
	void CompileGeometry();
	void CompileMaterials();
	void CompileTextureMapping2D(slg::ocl::TextureMapping2D *mapping, const TextureMapping2D *m);
	void CompileTextureMapping3D(slg::ocl::TextureMapping3D *mapping, const TextureMapping3D *m);
	void CompileTextures();
	void CompileImageMaps();
	void CompileAreaLights();
	void CompileInfiniteLight();
	void CompileSunLight();
	void CompileSkyLight();
};

}

#endif

#endif	/* _SLG_COMPILEDSESSION_H */
