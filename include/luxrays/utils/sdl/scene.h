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

#ifndef _LUXRAYS_SDL_SCENE_H
#define	_LUXRAYS_SDL_SCENE_H

#include <string>
#include <iostream>
#include <fstream>

#include "luxrays/utils/sdl/sdl.h"
#include "luxrays/utils/sdl/camera.h"
#include "luxrays/utils/sdl/light.h"
#include "luxrays/utils/sdl/material.h"
#include "luxrays/utils/sdl/texmap.h"
#include "luxrays/utils/sdl/extmeshcache.h"
#include "luxrays/utils/sdl/bsdf.h"
#include "luxrays/utils/properties.h"

namespace luxrays { namespace sdl {

class Scene {
public:
	// Constructor used to create a scene by calling methods
	Scene();
	// Constructor used to load a scene from file
	Scene(const std::string &fileName, const int accType = -1);
	~Scene();

	int GetAccelType() const { return accelType; }

	LightSource *GetLightByType(const LightSourceType lightType) const;
	LightSource *SampleAllLights(const float u, float *pdf) const;
	float PickLightPdf() const;
	Spectrum GetEnvLightsRadiance(const Vector &dir,
			const Point &hitPoint,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const;
	bool Intersect(IntersectionDevice *device,	
		const bool fromLight, const bool stopOnArchGlass,
		const float u0, Ray *ray, RayHit *rayHit,
		BSDF *bsdf, Spectrum *connectionThroughput) const;

	void UpdateDataSet(Context *ctx);

	//--------------------------------------------------------------------------
	// Methods to build a scene from scratch
	//--------------------------------------------------------------------------

	void CreateCamera(const std::string &propsString);
	void CreateCamera(const Properties &props);

	void DefineTexMap(const std::string &tmName, Spectrum *map,
		const unsigned int w, const unsigned int h) { texMapCache->DefineTexMap(tmName, new TextureMap(map, w, h)); }
	void DefineTexMap(const std::string &tmName, Spectrum *map, float *alpha,
		const unsigned int w, const unsigned int h) {
		TextureMap *tm = new TextureMap(map, w, h);
		tm->AddAlpha(alpha);
		texMapCache->DefineTexMap(tmName, tm);
	}

	void AddMaterials(const std::string &propsString);
	void AddMaterials(const Properties &props);

	void DefineObject(const std::string &objName, ExtTriangleMesh *mesh,
		const bool usePlyNormals = true) {
		extMeshCache->DefineExtMesh(objName, mesh, usePlyNormals);
	}
	void DefineObject(const std::string &objName,
		const long plyNbVerts, const long plyNbTris,
		Point *p, Triangle *vi, Normal *n, UV *uv,
		const bool usePlyNormals) {
		extMeshCache->DefineExtMesh(objName, plyNbVerts, plyNbTris, p, vi, n, uv, usePlyNormals);
	}

	void AddObject(const std::string &objName, const std::string &matName, const std::string &propsString);
	void AddObject(const std::string &objName, const std::string &matName, const Properties &props);

	void AddInfiniteLight(const std::string &propsString);
	void AddInfiniteLight(const Properties &props);
	void AddSkyLight(const std::string &propsString);
	void AddSkyLight(const Properties &props);
	void AddSunLight(const std::string &propsString);
	void AddSunLight(const Properties &props);

	//--------------------------------------------------------------------------

	static Material *CreateMaterial(const std::string &propName, const Properties &prop);

	PerspectiveCamera *camera;

	ExtMeshCache *extMeshCache; // Mesh objects
	TextureMapCache *texMapCache; // Texture maps

	LightSource *infiniteLight; // A SLG scene can have only one infinite light
	LightSource *sunLight;
	std::vector<LightSource *> lights; // One for each light source (doesn't include light/infinite light)

	std::vector<Material *> materials; // All materials (one for each light source)
	std::map<std::string, size_t> materialIndices; // All materials indices
	std::vector<ExtMesh *> objects; // All objects
	std::map<std::string, size_t> objectIndices; // All object indices

	std::vector<Material *> objectMaterials; // One for each object
	std::vector<TexMapInstance *> objectTexMaps; // One for each object
	std::vector<BumpMapInstance *> objectBumpMaps; // One for each object
	std::vector<NormalMapInstance *> objectNormalMaps; // One for each object

	std::vector<LightSource *> triangleLightSource; // One for each triangle

	DataSet *dataSet;

protected:
	static std::vector<float> GetParameters(const Properties &prop,
		const std::string &paramName, const unsigned int paramCount,
		const std::string &defaultValue);

	int accelType;
};

} }

#endif	/* _LUXRAYS_SDL_SCENE_H */
