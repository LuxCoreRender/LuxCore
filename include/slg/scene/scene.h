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

#ifndef _SLG_SCENE_H
#define	_SLG_SCENE_H

#include <string>
#include <iostream>
#include <fstream>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/geometry/bsphere.h"
#include "luxrays/core/namedobjectvector.h"
#include "luxrays/usings.h"
#include "slg/imagemap/imagemap.h"
#include "slg/usings.h"
#include "slg/utils/pathinfo.h"
#include "slg/editaction.h"
#include "slg/lights/lightsourcedefs.h"
#include "slg/shapes/strands.h"
#include "slg/textures/texturedefs.h"
#include "slg/materials/materialdefs.h"
#include "slg/scene/sceneobjectdefs.h"
#include "slg/scene/colorspaceconverters.h"

namespace luxrays {
	class cyHairFile;
}

namespace slg {

// Forward declarations
class PathVolumeInfo;
class BSDF;
class ImageMapConfig;

	// OpenCL data types
namespace ocl {
#include "slg/scene/scene_types.cl"
}

using luxrays::ExtTriangleMesh;
using luxrays::ExtTriangleMeshRef;
using luxrays::ExtTriangleMeshUPtr;
using luxrays::ExtInstanceTriangleMesh;
using luxrays::ExtInstanceTriangleMeshUPtr;
using luxrays::ExtMotionTriangleMesh;
using luxrays::ExtMotionTriangleMeshUPtr;
using luxrays::cyHairFile;

// Note: keep aligned with the copy in scene_types.cl
typedef enum {
	// Mandatory setting: one or the other must be used
	EYE_RAY = 1,
	LIGHT_RAY = 2,

	// This is used to disable any type of ray switch
	GENERIC_RAY = 4,
	// For the very first eye ray
	CAMERA_RAY = 8,
	// For rays used for direct light sampling
	SHADOW_RAY = 16,
	// For rays used for indirect light sampling
	INDIRECT_RAY = 32
} SceneRayTypeType;

// Note: keep aligned with the copy in scene_types.cl
typedef int SceneRayType;

class SampleResult;

// Scene is the root container for all components of the scene. It owns:
// - Camera
// - Default volume
// - Textures
// - Materials
// - SceneObjects (meshes)
// - Light sources
// - Data set
// - Image maps
//
class Scene {
public:
	// Constructor used to create a scene by calling methods
	Scene(luxrays::PropertiesRPtr resizePolicyProps = nullptr);

	// Constructor used to create a scene from properties
	Scene(
		luxrays::PropertiesRPtr&& scnProps,
		luxrays::PropertiesRPtr resizePolicyProps
	);
	~Scene();

	bool Intersect(luxrays::IntersectionDevicePtr device, const SceneRayType rayType, PathVolumeInfo *volInfo,
		const float passThrough, luxrays::Ray *ray, luxrays::RayHit *rayHit, BSDF *bsdf,
		luxrays::Spectrum *connectionThroughput, const luxrays::Spectrum *pathThroughput = nullptr,
		SampleResult *sampleResult = nullptr, const bool backTracing = false) const;

	// Returns the volume including the origin of the ray (the default world
	// volume, if any, when the origin is not inside a volume). It requires the
	// data set.
	VolumeConstPtr GetPointVolume(const luxrays::Ray &ray) const;
	void UpdateLightVolumes();

	void PreprocessCamera(const u_int filmWidth, const u_int filmHeight, const u_int *filmSubRegion);
	void Preprocess(luxrays::Context & ctx,
		const u_int filmWidth, const u_int filmHeight, const u_int *filmSubRegion,
		const bool useRTMode);

	luxrays::PropertiesUPtr ToProperties(const bool useRealFileName) const;

	//--------------------------------------------------------------------------
	// Methods to build and edit scene
	//--------------------------------------------------------------------------

	ImageMapRef DefineImageMap(ImageMapUPtr&& im);
	ImageMapRef DefineImageMap(const std::string &name, void *pixels,
		const u_int channels, const u_int width, const u_int height,
		const ImageMapConfig &cfg);

	bool IsImageMapDefined(const std::string &imgMapName) const;

	// Return type for DefineMesh
	template<typename T>
	using ReturnType = std::tuple<T&, std::unique_ptr<T>>;

	// Mesh shape
	// Use one of the following methods, do not directly call extMeshCache.DefineExtMesh()
	ReturnType<ExtMesh> DefineMesh(ExtMeshUPtr&& mesh);

	ReturnType<ExtTriangleMesh> DefineMesh(ExtTriangleMeshUPtr&& mesh);

	ReturnType<ExtInstanceTriangleMesh> DefineMesh(ExtInstanceTriangleMeshUPtr&& mesh);

	ReturnType<ExtMotionTriangleMesh> DefineMesh(ExtMotionTriangleMeshUPtr&& mesh);

	ReturnType<ExtTriangleMesh> DefineMesh(
		const std::string &shapeName,
		luxrays::VertexBuffer&& p,
		luxrays::TriangleBuffer&& vi,
		luxrays::NormalBuffer&& n,
		luxrays::ExtMeshProp<luxrays::UV>::Layer uvs,
		luxrays::ExtMeshProp<luxrays::Spectrum>::Layer cols,
		luxrays::ExtMeshProp<float>::Layer alphas
	);

	ReturnType<ExtTriangleMesh> DefineMesh(
		const std::string &shapeName,
		luxrays::VertexBuffer&& p,
		luxrays::TriangleBuffer&& vi,
		luxrays::NormalBuffer&& n,
		std::span<luxrays::UV> uvs,
		std::span<luxrays::Spectrum> cols,
		std::span<float> alphas
	);

	ReturnType<ExtTriangleMesh> DefineMeshExt(
		const std::string &shapeName,
		luxrays::VertexBuffer&& p,
		luxrays::TriangleBuffer&& vi,
		luxrays::NormalBuffer&& n,
		std::optional<luxrays::ExtMeshProp<luxrays::UV>> uvs,
		std::optional<luxrays::ExtMeshProp<luxrays::Spectrum>> cols,
		std::optional<luxrays::ExtMeshProp<float>> alphas
	);

	ReturnType<ExtInstanceTriangleMesh> DefineMesh(
		const std::string &instMeshName,
		const std::string &meshName,
		const luxrays::Transform &trans
	);

	ReturnType<ExtMotionTriangleMesh> DefineMesh(
		const std::string &motMeshName,
		const std::string &meshName,
		const luxrays::MotionSystem &ms
	);

	void SetMeshVertexAOV(const std::string &meshName,
		const unsigned int index, std::span<float> data);
	void SetMeshTriangleAOV(const std::string &meshName,
		const unsigned int index, std::span<float> data);

	// Strands shape
	Scene::ReturnType<ExtTriangleMesh> DefineStrands(
		const std::string &shapeName, const luxrays::cyHairFile &strandsFile,
		const StrendsShape::TessellationType tesselType,
		const u_int adaptiveMaxDepth, const float adaptiveError,
		const u_int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const bool useCameraPosition);

	bool IsTextureDefined(const std::string &texName) const;
	bool IsMaterialDefined(const std::string &matName) const;
	bool IsMeshDefined(const std::string &meshName) const;

	void Parse(luxrays::PropertiesRPtr props);

	void DeleteObject(const std::string &objName);
	void DeleteObjects(std::vector<std::string> &objNames);
	void DeleteLight(const std::string &lightName);
	void DeleteLights(std::vector<std::string> &lightNames);

	void DuplicateObject(const std::string &srcObjName, const std::string &dstObjName,
			const luxrays::Transform &trans, const u_int dstObjID);
	void DuplicateObject(const std::string &srcObjName, const std::string &dstObjName,
			const luxrays::MotionSystem &ms, const u_int dstObjID);
	void UpdateObjectMaterial(const std::string &objName, const std::string &matName);
	void UpdateObjectTransformation(const std::string &objName, const luxrays::Transform &trans);

	void RemoveUnusedImageMaps();
	void RemoveUnusedTextures();
	void RemoveUnusedMaterials();
	void RemoveUnusedMeshes();

	// Accessors
	// (Accessor role is:
	// - to allow to change underlying object types without modifying all code
	//	 base
	// - to clarify some constness aspects
	// )
	bool HasCamera() const { return bool(camera); }
	CameraConstRef GetCamera() const {
		if (not HasCamera()) throw std::runtime_error("No camera in scene");
		return *camera;
	}
	CameraRef GetCamera() {
		if (not HasCamera()) throw std::runtime_error("No camera in scene");
		return *camera;
	}

	auto& GetTextures() { return texDefs; }
	const auto& GetTextures() const { return texDefs; }

	auto& GetMaterials() { return matDefs; }
	const auto& GetMaterials() const { return matDefs; }

	auto& GetObjects() { return objDefs; }
	const auto& GetObjects() const { return objDefs; }

	auto& GetLightSources() { return lightDefs; }
	const auto& GetLightSources() const { return lightDefs; }

	auto& GetDataSet() { return *dataSet; }
	const auto& GetDataSet() const { return *dataSet; }

	auto& GetDefaultWorldVolume() const { return *defaultWorldVolume; }
	bool HasDefaultWorldVolume() const { return bool(defaultWorldVolume); }

	auto& GetImageMaps() { return imgMapCache; }
	const auto& GetImageMaps() const { return imgMapCache; }

	auto& GetExtMeshes() { return extMeshCache; }
	const auto& GetExtMeshes() const { return extMeshCache; }

	auto& GetEditActions() { return editActions; }
	const auto& GetEditActions() const { return editActions; }

	const auto& GetSceneBSphere() const { return sceneBSphere; }

	void SetEnableParsePrint(bool status) { enableParsePrint = status; }

	void moveToTrash(luxrays::NamedObjectUPtr&&);
	void emptyTrash();

	ImageMapConstSPtr GetRandomImageMap() const;

	// Serialization
	static SceneUPtr LoadSerialized(const std::string &fileName);
	static void SaveSerialized(const std::string &fileName, SceneUPtr&& scene);

	static std::string EncodeTriangleLightNamePrefix(const std::string &objectName);


protected:
	//--------------------------------------------------------------------------

	// This volume is (optionally) applied to rays hitting nothing
	VolumeConstPtr defaultWorldVolume;

	// This image map is required for some other textures
	inline static ImageMapSPtr randomImageMap{ImageMap::AllocRandomImageMap(512)};

	// Here are the containers for the scene elements:
	ExtMeshCache extMeshCache; // Mesh objects cache
	ImageMapCache imgMapCache; // Image maps cache

	TextureDefinitions texDefs; // Texture definitions
	MaterialDefinitions matDefs; // Material definitions
	SceneObjectDefinitions objDefs; // SceneObject definitions
	LightSourceDefinitions lightDefs; // LightSource definitions

	// The trash bin container collects items that are pending deletion, in
	// order to avoid dangling references in real time rendering when the scene
	// is updated while the render engine is running. Trash bin allows to keep
	// the deleted objects alive while they are still in use in rendering. It
	// is up to the rendering engine to empty trash bin.
	std::vector<luxrays::NamedObjectUPtr> trashBin;
	mutable std::mutex trashMtx;

	// DataSet ownership is not very clear, we set it shared
	luxrays::DataSetSPtr dataSet;

	// The bounding sphere of the scene (including the camera)
	luxrays::BSphere sceneBSphere;

	EditActionList editActions;

	bool enableParsePrint;
	friend class boost::serialization::access;

private:
	CameraUPtr camera;  // The scene owns the camera
						//
	ColorSpaceConverters colorSpaceConv;

	void Init(luxrays::PropertiesRPtr resizePolicyProps);

	void ParseCamera(const luxrays::Properties &props);
	void ParseTextures(const luxrays::Properties &props);
	void ParseVolumes(const luxrays::Properties &props);
	void ParseMaterials(const luxrays::Properties &props);
	void ParseShapes(const luxrays::Properties &props);
	void ParseObjects(const luxrays::Properties &props);
	void ParseLights(const luxrays::Properties &props);

	luxrays::Spectrum GetColor(const luxrays::Property &prop);
	TextureRef GetTexture(const luxrays::Property &prop);
	TextureConstRef GetTexture(const luxrays::Property &prop) const;

	CameraUPtr CreateCamera(const luxrays::Properties &props);
	TextureMapping2DUPtr CreateTextureMapping2D (
		const std::string &prefixName,
		const luxrays::Properties &props
	);


	TextureMapping3DUPtr CreateTextureMapping3D(const std::string &prefixName, const luxrays::Properties &props);
	TextureUPtr CreateTexture(const std::string &texName, const luxrays::Properties &props);
	VolumeUPtr CreateVolume(const u_int defaultVolID, const std::string &volName, const luxrays::Properties &props);
	MaterialUPtr CreateMaterial(const u_int defaultMatID, const std::string &matName, const luxrays::Properties &props);

	luxrays::ExtTriangleMeshUPtr CreateShape(const std::string &shapeName, const luxrays::Properties &props);
	SceneObjectUPtr CreateObject(const u_int defaultObjID, const std::string &objName, const luxrays::Properties &props);
	LightSourceUPtr CreateLightSource(const std::string &lightName, const luxrays::Properties &props);

	// Create directly in cache, so result is just a reference
	ImageMapPtr CreateEmissionMap(const std::string &propName, const luxrays::Properties &props);

	luxrays::ExtTriangleMeshUPtr CreateInlinedMesh(const std::string &shapeName,
			const std::string &propName, const luxrays::Properties &props);

	template<class Archive> void save(Archive &ar, const u_int version) const;
	template<class Archive> void load(Archive &ar, const u_int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()
};

}  // namespace slg

BOOST_CLASS_VERSION(slg::Scene, 1)

BOOST_CLASS_EXPORT_KEY(slg::Scene)

#endif	/* _SLG_SCENE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
