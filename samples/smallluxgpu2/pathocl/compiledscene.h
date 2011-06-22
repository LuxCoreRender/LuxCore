/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

#ifndef _COMPILEDSESSION_H
#define	_COMPILEDSESSION_H

#include "smalllux.h"
#include "ocldatatypes.h"
#include "renderconfig.h"
#include "editaction.h"

#include "luxrays/utils/film/film.h"

class CompiledScene {
public:
	CompiledScene(RenderConfig *cfg, Film *flm);
	~CompiledScene();

	void Recompile(const EditActionList &editActions);
	bool IsMaterialCompiled(const MaterialType type) const;

	RenderConfig *renderConfig;
	Film *film;

	// Compiled Camera
	PathOCL::Camera camera;

	// Compiled Scene Geometry
	vector<Point> verts;
	vector<Normal> normals;
	vector<UV> uvs;
	vector<Triangle> tris;
	vector<PathOCL::Mesh> meshDescs;
	const TriangleMeshID *meshIDs;
	// One element for each mesh, it used to translate the RayBuffer currentTriangleIndex
	// to mesh TriangleID
	unsigned int *meshFirstTriangleOffset;

	// Compiled AreaLights
	vector<PathOCL::TriangleLight> areaLights;

	// Compiled InfiniteLights
	PathOCL::InfiniteLight *infiniteLight;
	const Spectrum *infiniteLightMap;

	// Compiled SunLight
	PathOCL::SunLight *sunLight;
	// Compiled SkyLight
	PathOCL::SkyLight *skyLight;

	// Compiled Materials
	bool enable_MAT_MATTE, enable_MAT_AREALIGHT, enable_MAT_MIRROR, enable_MAT_GLASS,
		enable_MAT_MATTEMIRROR, enable_MAT_METAL, enable_MAT_MATTEMETAL, enable_MAT_ALLOY,
		enable_MAT_ARCHGLASS;
	vector<PathOCL::Material> mats;
	vector<unsigned int> meshMats;

	// Compiled TextureMaps
	vector<PathOCL::TexMap> gpuTexMaps;
	unsigned int totRGBTexMem;
	Spectrum *rgbTexMem;
	unsigned int totAlphaTexMem;
	float *alphaTexMem;
	unsigned int *meshTexs;
	// Bump mapping
	unsigned int *meshBumps;
	float *bumpMapScales;


private:
	void CompileCamera();
	void CompileGeometry();
	void CompileMaterials();
	void CompileAreaLights();
	void CompileInfiniteLight();
	void CompileSunLight();
	void CompileSkyLight();
	void CompileTextureMaps();
};

#endif	/* _COMPILEDSESSION_H */
