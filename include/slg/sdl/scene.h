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

#ifndef _SLG_SCENE_H
#define	_SLG_SCENE_H

#include <string>
#include <iostream>
#include <fstream>

#include "luxrays/utils/properties.h"
#include "luxrays/core/extmeshcache.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/accelerator.h"
#include "slg/camera/camera.h"
#include "slg/editaction.h"
#include "slg/sdl/sdl.h"
#include "slg/sdl/light.h"
#include "slg/sdl/texture.h"
#include "slg/sdl/material.h"
#include "slg/sdl/sceneobject.h"
#include "slg/sdl/bsdf.h"
#include "slg/sdl/mapping.h"
#include "slg/core/mc.h"

namespace slg {

class Scene {
public:
	// Constructor used to create a scene by calling methods
	Scene();
	// Constructor used to load a scene from file
	Scene(const std::string &fileName, const float imageScale = 1.f);
	~Scene();

	const u_int GetLightCount() const;
	LightSource *GetLightByType(const LightSourceType lightType) const;
	LightSource *GetLightByIndex(const u_int index) const;
	LightSource *SampleAllLights(const float u, float *pdf) const;
	float SampleAllLightPdf(const LightSource *light) const;

	bool Intersect(luxrays::IntersectionDevice *device, const bool fromLight,
		const float u0, luxrays::Ray *ray, luxrays::RayHit *rayHit,
		BSDF *bsdf, luxrays::Spectrum *connectionThroughput) const;

	void UpdateLightGroupCount();
	void Preprocess(luxrays::Context *ctx);

	luxrays::Properties ToProperties(const std::string &directoryName);

	//--------------------------------------------------------------------------
	// Methods to build and edit scene
	//--------------------------------------------------------------------------

	void DefineImageMap(const std::string &name, ImageMap *im) {
		imgMapCache.DefineImgMap(name, im);
	}
	void DefineMesh(const std::string &meshName, luxrays::ExtTriangleMesh *mesh,
		const bool usePlyNormals = true) {
		extMeshCache.DefineExtMesh(meshName, mesh, usePlyNormals);
	}
	void DefineMesh(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		luxrays::Point *p, luxrays::Triangle *vi, luxrays::Normal *n, luxrays::UV *uv,
		luxrays::Spectrum *cols, float *alphas,
		const bool usePlyNormals) {
		extMeshCache.DefineExtMesh(meshName, plyNbVerts, plyNbTris, p, vi, n, uv, cols, alphas, usePlyNormals);
	}

	void Parse(const luxrays::Properties &props);

	void UpdateObjectTransformation(const std::string &objName, const luxrays::Transform &trans);

	void RemoveUnusedMaterials();
	void RemoveUnusedTextures();

	// TODO: a method to remove unused image maps from cache
	// TODO: a method to remove unused meshes from cache

	//--------------------------------------------------------------------------

	PerspectiveCamera *camera;

	luxrays::ExtMeshCache extMeshCache; // Mesh objects cache
	ImageMapCache imgMapCache; // Image maps cache

	TextureDefinitions texDefs; // Texture definitions
	MaterialDefinitions matDefs; // Material definitions
	SceneObjectDefinitions objDefs; // SceneObject definitions

	u_int lightGroupCount;
	InfiniteLightBase *envLight; // A SLG scene can have only one infinite light
	SunLight *sunLight;
	std::vector<TriangleLight *> triLightDefs; // One for each light source (doesn't include sun/infinite light)
	std::vector<u_int> meshTriLightDefsOffset; // One for each mesh

	luxrays::DataSet *dataSet;
	luxrays::AcceleratorType accelType;
	bool enableInstanceSupport;

	// Used for power based light sampling strategy
	Distribution1D *lightsDistribution;

	EditActionList editActions;

protected:
	void ParseCamera(const luxrays::Properties &props);
	void ParseTextures(const luxrays::Properties &props);
	void ParseMaterials(const luxrays::Properties &props);
	void ParseObjects(const luxrays::Properties &props);
	void ParseEnvLights(const luxrays::Properties &props);

	TextureMapping2D *CreateTextureMapping2D(const std::string &prefixName, const luxrays::Properties &props);
	TextureMapping3D *CreateTextureMapping3D(const std::string &prefixName, const luxrays::Properties &props);
	Texture *CreateTexture(const std::string &texName, const luxrays::Properties &props);
	Texture *GetTexture(const luxrays::Property &name);
	Material *CreateMaterial(const u_int defaultMatID, const std::string &matName, const luxrays::Properties &props);
	SceneObject *CreateObject(const std::string &objName, const luxrays::Properties &props);

	void RebuildTriangleLightDefs();
};

}

#endif	/* _SLG_SCENE_H */
