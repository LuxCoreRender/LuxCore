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
#include "luxrays/utils/sdl/texture.h"
#include "luxrays/utils/sdl/extmeshcache.h"
#include "luxrays/utils/sdl/bsdf.h"
#include "luxrays/utils/properties.h"

namespace luxrays { namespace sdl {

class Scene {
public:
	// Constructor used to create a scene by calling methods
	Scene(const int accType = -1);
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
	bool Intersect(IntersectionDevice *device, const bool fromLight,
		const float u0, Ray *ray, RayHit *rayHit,
		BSDF *bsdf, Spectrum *connectionThroughput) const;

	void UpdateDataSet(Context *ctx);

	//--------------------------------------------------------------------------
	// Methods to build a scene from scratch
	//--------------------------------------------------------------------------

	void CreateCamera(const std::string &propsString);
	void CreateCamera(const Properties &props);

	void DefineImageMap(const std::string &name, ImageMap *im) {
		imgMapCache.DefineImgMap(name, im);
	}

	void DefineTextures(const std::string &propsString);
	void DefineTextures(const Properties &props);

	void DefineMaterials(const std::string &propsString);
	void DefineMaterials(const Properties &props);
	void UpdateMaterial(const std::string &name, const std::string &propsString);
	void UpdateMaterial(const std::string &name, const Properties &props);

	void DefineObject(const std::string &meshName, ExtTriangleMesh *mesh,
		const bool usePlyNormals = true) {
		extMeshCache.DefineExtMesh(meshName, mesh, usePlyNormals);
	}
	void DefineObject(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		Point *p, Triangle *vi, Normal *n, UV *uv,
		const bool usePlyNormals) {
		extMeshCache.DefineExtMesh(meshName, plyNbVerts, plyNbTris, p, vi, n, uv, usePlyNormals);
	}

	void AddObject(const std::string &objName, const std::string &meshName, const std::string &propsString);
	void AddObject(const std::string &objName, const Properties &props);
	void UpdateObjectTransformation(const std::string &objName, const Transform &trans);

	void AddObjects(const std::string &propsString);
	void AddObjects(const Properties &props);

	void AddInfiniteLight(const std::string &propsString);
	void AddInfiniteLight(const Properties &props);
	void AddSkyLight(const std::string &propsString);
	void AddSkyLight(const Properties &props);
	void AddSunLight(const std::string &propsString);
	void AddSunLight(const Properties &props);

	void RemoveUnusedMaterials();

	//--------------------------------------------------------------------------

	Texture *CreateTexture(const std::string &texName, const Properties &props);
	Material *CreateMaterial(const std::string &matName, const Properties &props);

	PerspectiveCamera *camera;

	TextureDefinitions texDefs; // Texture definitions
	MaterialDefinitions matDefs; // Material definitions
	ExtMeshDefinitions meshDefs; // ExtMesh definitions

	LightSource *infiniteLight; // A SLG scene can have only one infinite light
	LightSource *sunLight;
	std::vector<LightSource *> lights; // One for each light source (doesn't include sun/infinite light)

	std::vector<Material *> objectMaterials; // One for each object
	std::vector<TriangleLight *> triangleLightSource; // One for each triangle

	DataSet *dataSet;

protected:
	static std::vector<std::string> GetStringParameters(const Properties &prop,
		const std::string &paramName, const unsigned int paramCount,
		const std::string &defaultValue);
	static std::vector<float> GetFloatParameters(const Properties &prop,
		const std::string &paramName, const unsigned int paramCount,
		const std::string &defaultValue);

	Texture *GetTexture(const std::string &name);

	ExtMeshCache extMeshCache; // Mesh objects
	ImageMapCache imgMapCache; // Image maps

	int accelType;
};

} }

#endif	/* _LUXRAYS_SDL_SCENE_H */
