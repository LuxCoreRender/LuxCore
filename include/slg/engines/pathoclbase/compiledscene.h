/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_COMPILEDSESSION_H
#define	_SLG_COMPILEDSESSION_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/unordered_set.hpp>

#include "slg/slg.h"
#include "slg/editaction.h"

#include "slg/film/film.h"
#include "slg/scene/scene.h"
#include "slg/scene/sceneobject.h"

namespace slg {

class CompiledScene {
public:
	CompiledScene(Scene *scn, Film *flm);
	~CompiledScene();
	
	void SetMaxMemPageSize(const size_t maxSize);
	void EnableCode(const std::string &tags);

	void Compile();
	void Recompile(const EditActionList &editActions);

	bool IsMaterialCompiled(const MaterialType type) const;
	bool IsTextureCompiled(const TextureType type) const;
	bool IsImageMapFormatCompiled(const ImageMapStorage::StorageType type) const;
	bool IsImageMapChannelCountCompiled(const u_int count) const;

	bool RequiresPassThrough() const;
	bool HasVolumes() const;
	bool HasBumpMaps() const;

	std::string GetTexturesEvaluationSourceCode() const;
	std::string GetMaterialsEvaluationSourceCode() const;

	static float *CompileDistribution1D(const luxrays::Distribution1D *dist, u_int *size);
	static float *CompileDistribution2D(const luxrays::Distribution2D *dist, u_int *size);

	static std::string ToOCLString(const slg::ocl::Spectrum &v);

	Scene *scene;
	Film *film;

	// Compiled Camera
	slg::ocl::CameraType cameraType;
	slg::ocl::Camera camera;
	bool enableCameraClippingPlane, enableCameraOculusRiftBarrel;

	// Compiled Scene Meshes
	vector<luxrays::Point> verts;
	vector<luxrays::Normal> normals;
	vector<luxrays::UV> uvs;
	vector<luxrays::Spectrum> cols;
	vector<float> alphas;
	vector<luxrays::Triangle> tris;
	vector<luxrays::ocl::Mesh> meshDescs;
	luxrays::BSphere worldBSphere;

	// Compiled Scene Objects
	vector<slg::ocl::SceneObject> sceneObjs;

	// Compiled Lights
	vector<slg::ocl::LightSource> lightDefs;
	// Additional light related information
	vector<u_int> lightTypeCounts;
	vector<u_int> envLightIndices;
	vector<u_int> meshTriLightDefsOffset;
	// Infinite light Distribution2Ds
	vector<float> infiniteLightDistributions;
	// Compiled power based light sampling strategy
	float *lightsDistribution;
	u_int lightsDistributionSize;
	bool hasInfiniteLights, hasEnvLights, hasTriangleLightWithVertexColors;

	// Compiled Materials (and Volumes)
	std::set<MaterialType> usedMaterialTypes;
	vector<slg::ocl::Material> mats;
	u_int defaultWorldVolumeIndex;

	// Compiled Textures
	std::set<TextureType> usedTextureTypes;
	vector<slg::ocl::Texture> texs;

	// Compiled ImageMaps
	vector<slg::ocl::ImageMap> imageMapDescs;
	vector<vector<float> > imageMapMemBlocks;
	std::set<ImageMapStorage::StorageType> usedImageMapFormats;
	std::set<u_int> usedImageMapChannels;

private:
	void AddEnabledImageMapCode();
	// There is no AddEnabledTextureCode() version because all textures not already
	// included by default have source code dynamically generated (because they
	// reference always other textures)
	void AddEnabledMaterialCode();

	void CompileCamera();
	void CompileSceneObjects();
	void CompileGeometry();
	void CompileMaterials();
	void CompileTextureMapping2D(slg::ocl::TextureMapping2D *mapping, const TextureMapping2D *m);
	void CompileTextureMapping3D(slg::ocl::TextureMapping3D *mapping, const TextureMapping3D *m);
	void CompileTextures();
	void CompileImageMaps();
	void CompileLights();

	u_int maxMemPageSize;
	boost::unordered_set<std::string> enabledCode;
	bool useTransparency, useBumpMapping;
}; 

}

#endif

#endif	/* _SLG_COMPILEDSESSION_H */
