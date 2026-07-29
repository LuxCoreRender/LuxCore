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

#include <cstdlib>
#include <istream>
#include <stdexcept>
#include <sstream>
#include <set>
#include <typeinfo>
#include <vector>
#include <memory>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/format.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/namedobjectvector.h"
#include "luxrays/core/randomgen.h"
#include "luxrays/usings.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/utils.h"
#include "slg/core/sphericalfunction/sphericalfunction.h"
#include "slg/editaction.h"
#include "slg/imagemap/imagemap.h"
#include "slg/samplers/sampler.h"
#include "slg/core/sdl.h"
#include "slg/scene/scene.h"
#include "slg/textures/constfloat.h"
#include "slg/textures/constfloat3.h"
#include "slg/textures/imagemaptex.h"
#include "slg/usings.h"
#include "slg/utils/pathinfo.h"
#include "slg/cameras/camera.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Scene
//------------------------------------------------------------------------------

Scene::Scene(luxrays::PropertiesRPtr resizePolicyProps) {
	Init(resizePolicyProps);
}

Scene::Scene(
	PropertiesRPtr scnProps,
	PropertiesRPtr resizePolicyProps
) {
	Init(resizePolicyProps);

	Parse(scnProps);
}

void Scene::Init(luxrays::PropertiesRPtr resizePolicyProps) {
	defaultWorldVolume = nullptr;
	// Just in case there is an unexpected exception during the scene loading
    camera = nullptr;

	dataSet = nullptr;

	editActions.AddAllAction();
	if (resizePolicyProps)
		imgMapCache.SetImageResizePolicy(ImageMapResizePolicy::FromProperties(*resizePolicyProps));
	// Add random image map to imgMapCache and specify its resize policy
	imgMapCache.DefineImageMap(ImageMapTexture::randomImageMap);
	imgMapCache.resizePolicyToApply.push_back(false);

	enableParsePrint = true;
}

Scene::~Scene() {
#ifndef NDEBUG
	std::cerr << "Deleting scene\n";
#endif
	dataSet.reset();
}

PropertiesUPtr Scene::ToProperties(const bool useRealFileName) const {
	auto props = std::make_unique<Properties>();

	// Write the camera information
	if (camera)
		props->Set(camera->ToProperties(imgMapCache, useRealFileName));

	// Save all not intersectable light sources
	vector<string> lightNames = lightDefs.GetLightSourceNames();
	for (u_int i = 0; i < lightNames.size(); ++i) {
		auto& l = lightDefs.GetLightSource(lightNames[i]);
		try {
			dynamic_cast<const NotIntersectableLightSource &>(l);
			props->Set((static_cast<const NotIntersectableLightSource &>(l))
				.ToProperties(imgMapCache, useRealFileName));
		}
		catch(std::bad_cast&) {}
	}

	// Get the sorted list of texture names according their dependencies
	auto texNames = texDefs.GetTextureSortedNames();

	// Write the textures information
	for (auto const &texName : texNames) {
		// I can skip all textures starting with Implicit-ConstFloatTexture(3)
		// because they are expanded inline
		if (texName.starts_with("Implicit-ConstFloatTexture"))
			continue;

		TextureConstRef tex = texDefs.GetTexture(texName);
		props->Set(tex.ToProperties(imgMapCache, useRealFileName));
	}

	// Get the sorted list of material names according their dependencies
	vector<string> matNames;
	matDefs.GetMaterialSortedNames(matNames);

	// Write the volumes information
	for (auto const &matName : matNames) {
		MaterialConstRef mat = matDefs.GetMaterial(matName);
		// Check if it is a volume
		try {
			VolumeConstRef vol = dynamic_cast<const Volume &>(mat);
			props->Set(*vol.ToProperties());
		}
		catch (std::bad_cast&) {}
	}

	// Set the default world interior/exterior volume if required
	if (defaultWorldVolume) {
		const u_int index = matDefs.GetMaterialIndex(*defaultWorldVolume);
		props->Set(Property("scene.world.volume.default")(matDefs.GetMaterial(index).GetName()));
	}

	// Write the materials information
	for (auto const &matName : matNames) {
		MaterialConstRef mat = matDefs.GetMaterial(matName);
		// Check if it is not a volume
		try {
			VolumeConstRef vol = dynamic_cast<const Volume &>(mat);
		} catch (std::bad_cast&) {
			props->Set(mat.ToProperties(imgMapCache, useRealFileName));
		}
	}

	// Write the object information
	for (u_int i = 0; i < objDefs.GetSize(); ++i) {
		auto& obj = objDefs.GetSceneObject(i);
		props->Set(obj.ToProperties(extMeshCache, useRealFileName));
	}

	return props;
}

//--------------------------------------------------------------------------
// Methods to build and edit a scene
//--------------------------------------------------------------------------

ImageMapRef Scene::DefineImageMap(ImageMapUPtr&& im) {
	ImageMapRef ref = imgMapCache.DefineImageMap(std::move(im));

	editActions.AddAction(IMAGEMAPS_EDIT);
	return ref;
}
ImageMapRef Scene::DefineImageMap(const string &name, void *pixels,
		const u_int channels, const u_int width, const u_int height,
		const ImageMapConfig &cfg) {
	auto imgMap = ImageMap::AllocImageMap(pixels, channels, width, height, cfg);
	imgMap->SetName(name);

	ImageMapRef ref = DefineImageMap(std::move(imgMap));

	editActions.AddAction(IMAGEMAPS_EDIT);
	return ref;
}

bool Scene::IsImageMapDefined(const string &imgMapName) const {
	return imgMapCache.IsImageMapDefined(imgMapName);
}


// DefineMesh methods


Scene::ReturnType<ExtMesh> Scene::DefineMesh(ExtMeshUPtr&& mesh) {
	const string &shapeName = mesh->GetName();

	if (extMeshCache.IsExtMeshDefined(shapeName)) {
		// A replacement for an existing mesh
		auto& oldMesh = extMeshCache.GetExtMesh(shapeName);

		// Replace old mesh direct references with new one and get the list
		// of scene objects referencing the old mesh
		std::unordered_set<const SceneObject *> modifiedObjsList;
		objDefs.UpdateMeshReferences(oldMesh, *mesh, modifiedObjsList);

		// For each scene object
		for(auto* o: modifiedObjsList) {
			// Check if is a light source
			if (o->GetMaterial().IsLightSource()) {
				const string objName = o->GetName();

				// Delete all old triangle lights
				lightDefs.DeleteLightSourceStartWith(Scene::EncodeTriangleLightNamePrefix(objName));

				// Add all new triangle lights
				SDL_LOG("The " << objName << " object is a light sources with " << mesh->GetTotalTriangleCount() << " triangles");
				objDefs.DefineIntersectableLights(lightDefs, *o);

				editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
			}
		}
	}

	// This is the only place where it is safe to call extMeshCache.DefineExtMesh()
	auto [newMeshRef, oldMeshPtr] = extMeshCache.DefineExtMesh(std::move(mesh));

	editActions.AddAction(GEOMETRY_EDIT);

	return std::make_tuple(
		std::ref(newMeshRef),
		oldMeshPtr ? std::move(oldMeshPtr) : nullptr
	);
}

Scene::ReturnType<ExtTriangleMesh> Scene::DefineMesh(ExtTriangleMeshUPtr&& mesh) {
	ExtMeshUPtr extmesh = std::move(mesh);
	auto [newMeshRef, oldMeshPtr] = DefineMesh(std::move(extmesh));

	auto& newDerivedRef = dynamic_cast<ExtTriangleMesh&>(newMeshRef);
	auto oldDerivedPtr = dynamic_uptr_cast<ExtTriangleMesh>(std::move(oldMeshPtr));

	return std::make_tuple(std::ref(newDerivedRef), std::move(oldDerivedPtr));
}

Scene::ReturnType<ExtInstanceTriangleMesh>
Scene::DefineMesh(ExtInstanceTriangleMeshUPtr&& mesh) {
	ExtMeshUPtr extmesh = std::move(mesh);
	auto [newMeshRef, oldMeshPtr] = DefineMesh(std::move(extmesh));

	auto& newDerivedRef = dynamic_cast<ExtInstanceTriangleMesh&>(newMeshRef);
	auto oldDerivedPtr = dynamic_uptr_cast<ExtInstanceTriangleMesh>(std::move(oldMeshPtr));

	return std::make_tuple(std::ref(newDerivedRef), std::move(oldDerivedPtr));
}


Scene::ReturnType<ExtMotionTriangleMesh>
Scene::DefineMesh(ExtMotionTriangleMeshUPtr&& mesh) {
	ExtMeshUPtr extmesh = std::move(mesh);
	auto [newMeshRef, oldMeshPtr] = DefineMesh(std::move(extmesh));

	auto& newDerivedRef = dynamic_cast<ExtMotionTriangleMesh&>(newMeshRef);
	auto oldDerivedPtr = dynamic_uptr_cast<ExtMotionTriangleMesh>(std::move(oldMeshPtr));

	return std::make_tuple(std::ref(newDerivedRef), std::move(oldDerivedPtr));
}


Scene::ReturnType<ExtTriangleMesh> Scene::DefineMesh(
	const string &shapeName,
	const long plyNbVerts,
	const long plyNbTris,
	Point *p,
	Triangle *vi,
	Normal *n,
	UV *uvs,
	Spectrum *cols,
	float *alphas
) {
	auto mesh = std::make_unique<ExtTriangleMesh>(plyNbVerts, plyNbTris, p, vi, n,
			uvs, cols, alphas);
	mesh->SetName(shapeName);

	return DefineMesh(std::move(mesh));
}

Scene::ReturnType<ExtTriangleMesh> Scene::DefineMeshExt(
	const string &shapeName,
	const long plyNbVerts,
	const long plyNbTris,
	Point *p, Triangle *vi, Normal *n,
	array<UV *, EXTMESH_MAX_DATA_COUNT> *uvs,
	array<Spectrum *, EXTMESH_MAX_DATA_COUNT> *cols,
	array<float *, EXTMESH_MAX_DATA_COUNT> *alphas)
{
	auto mesh = std::make_unique<ExtTriangleMesh>(
			plyNbVerts, plyNbTris, p, vi, n,
			uvs, cols, alphas
	);
	mesh->SetName(shapeName);

	return DefineMesh(std::move(mesh));
}

Scene::ReturnType<ExtInstanceTriangleMesh> Scene::DefineMesh(
	const string &instMeshName,
	const string &meshName,
	const Transform &trans
) {
	auto& mesh = extMeshCache.GetExtMesh(meshName);
	//TODO
	//if (!mesh)
		//throw runtime_error("Unknown mesh in Scene::DefineMesh(): " + meshName);

	try {
		auto& etMesh = dynamic_cast<ExtTriangleMesh&>(mesh);
		auto iMesh = std::make_unique<ExtInstanceTriangleMesh>(etMesh, trans);
		iMesh->SetName(instMeshName);
		return DefineMesh(std::move(iMesh));
	}
	catch (std::bad_cast&) {
		throw runtime_error("Wrong mesh type in Scene::DefineMesh(): " + meshName);
	}

}

Scene::ReturnType<ExtMotionTriangleMesh> Scene::DefineMesh(
	const string &motMeshName,
	const string &meshName,
	const MotionSystem &ms
) {
	auto& mesh = extMeshCache.GetExtMesh(meshName);
	//TODO
	//if (!mesh)
		//throw runtime_error("Unknown mesh in Scene::DefineExtMesh(): " + meshName);

	try {
		auto& etMesh = dynamic_cast<ExtTriangleMesh&>(mesh);
		auto motMesh = std::make_unique<ExtMotionTriangleMesh>(etMesh, ms);
		motMesh->SetName(motMeshName);
		return DefineMesh(std::move(motMesh));
	} catch (std::bad_cast&) {
		throw runtime_error("Wrong mesh type in Scene::DefineMesh(): " + meshName);
	}

}

void Scene::SetMeshVertexAOV(const string &meshName,
		const unsigned int index, float *data) {
	extMeshCache.SetMeshVertexAOV(meshName, index, data);
}

void Scene::SetMeshTriangleAOV(const string &meshName,
		const unsigned int index, float *data) {
	extMeshCache.SetMeshTriangleAOV(meshName, index, data);
}

Scene::ReturnType<ExtTriangleMesh>
Scene::DefineStrands(const string &shapeName, const slg::cyHairFile &strandsFile,
		const StrendsShape::TessellationType tesselType,
		const u_int adaptiveMaxDepth, const float adaptiveError,
		const u_int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const bool useCameraPosition) {
	StrendsShape shape(*this,
			&strandsFile, tesselType,
			adaptiveMaxDepth, adaptiveError,
			solidSideCount, solidCapBottom, solidCapTop,
			useCameraPosition);

	auto mesh = shape.Refine(*this);
	mesh->SetName(shapeName);

	editActions.AddAction(GEOMETRY_EDIT);

	return DefineMesh(std::move(mesh));
}

bool Scene::IsTextureDefined(const string &texName) const {
	return texDefs.IsTextureDefined(texName);
}

bool Scene::IsMaterialDefined(const string &matName) const {
	return matDefs.IsMaterialDefined(matName);
}

bool Scene::IsMeshDefined(const string &meshName) const {
	return extMeshCache.IsExtMeshDefined(meshName);
}

void Scene::Parse(PropertiesRPtr props) {
	if (enableParsePrint) {
		SDL_LOG("========================Scene::Parse()========================="
		<< endl << *props);
		SDL_LOG("===============================================================");
	}

	//--------------------------------------------------------------------------
	// Read all textures
	//--------------------------------------------------------------------------

	SDL_LOG("Parsing textures");
	ParseTextures(*props);

	//--------------------------------------------------------------------------
	// Read all volumes
	//--------------------------------------------------------------------------

	SDL_LOG("Parsing volumes");
	ParseVolumes(*props);

	//--------------------------------------------------------------------------
	// Read all materials
	//--------------------------------------------------------------------------

	SDL_LOG("Parsing materials");
	ParseMaterials(*props);

	//--------------------------------------------------------------------------
	// Read camera position and target
	//
	// note: doing the parsing after volumes because it may reference a volume
	//--------------------------------------------------------------------------

	SDL_LOG("Parsing cameras");
	ParseCamera(*props);

	//--------------------------------------------------------------------------
	// Read all shapes
	//--------------------------------------------------------------------------

	SDL_LOG("Parsing shapes");
	ParseShapes(*props);

	//--------------------------------------------------------------------------
	// Read all objects
	//--------------------------------------------------------------------------

	SDL_LOG("Parsing objects");
	ParseObjects(*props);

	//--------------------------------------------------------------------------
	// Read all env. lights
	//--------------------------------------------------------------------------

	SDL_LOG("Parsing lights");
	ParseLights(*props);
}


void Scene::moveToTrash(NamedObjectUPtr&& oldObj) {
	if (!oldObj) return;
	std::lock_guard lk(trashMtx);
	trashBin.push_back(std::move(oldObj));
	SDL_LOG("Moving object to trash: " << trashBin.back()->GetName());
}


void Scene::emptyTrash() {
	std::lock_guard lk(trashMtx);
	trashBin.clear();
}


void Scene::RemoveUnusedImageMaps() {
	// Build a list of all referenced image maps
	std::unordered_set<const ImageMap *> referencedImgMaps;
	for (u_int i = 0; i < texDefs.GetSize(); ++i)
		texDefs.GetTexture(i).AddReferencedImageMaps(referencedImgMaps);
	for (u_int i = 0; i < objDefs.GetSize(); ++i)
		objDefs.GetSceneObject(i).AddReferencedImageMaps(referencedImgMaps);

	// Add the light image maps

	// I can not use lightDefs.GetLightSources() here because the
	// scene may have been not preprocessed
	for(const string &lightName: lightDefs.GetLightSourceNames()) {
		auto& l = lightDefs.GetLightSource(lightName);
		l.AddReferencedImageMaps(referencedImgMaps);
	}

	// Add the material image maps
	for (u_int i = 0; i < matDefs.GetSize(); ++i)
		matDefs.GetMaterial(i).AddReferencedImageMaps(referencedImgMaps);

	// Avoid to remove random image map from imgMapCache 
	referencedImgMaps.insert(ImageMapTexture::randomImageMap.get());
	
	// Get the list of all defined image maps
	bool deleted = false;
	for(ImageMapConstRef im: imgMapCache.GetImageMaps()) {
		if (referencedImgMaps.count(&im) == 0) {
			SDL_LOG("Deleting unreferenced image map: " << im.GetName());
			imgMapCache.DeleteImageMap(im);
			deleted = true;
		}
	}

	if (deleted) {
		editActions.AddAction(IMAGEMAPS_EDIT);
		// Indices of image maps are changed so I need to update also the
		// textures, materials and light sources
		editActions.AddAction(MATERIALS_EDIT);
		editActions.AddAction(MATERIAL_TYPES_EDIT);
		editActions.AddAction(LIGHTS_EDIT);
		editActions.AddAction(LIGHT_TYPES_EDIT);
	}
}

void Scene::RemoveUnusedTextures() {
	// Build a list of all referenced textures names
	std::unordered_set<const Texture *> referencedTexs;
	for (u_int i = 0; i < matDefs.GetSize(); ++i) {
		//matDefs.GetMaterial(i)->AddReferencedTextures(referencedTexs, matDefs.GetMaterial(i));
		matDefs.GetMaterial(i).AddReferencedTextures(referencedTexs);
	}

	// Get the list of all defined textures
	bool deleted = false;
	for(const auto &texName: texDefs.GetTextureNames()) {
		TextureConstRef t = texDefs.GetTexture(texName);

		if (referencedTexs.count(&t) == 0) {
			SDL_LOG("Deleting unreferenced texture: " << texName);
			moveToTrash(texDefs.DeleteTexture(texName));
			deleted = true;
		}
	}

	if (deleted) {
		editActions.AddAction(MATERIALS_EDIT);
		editActions.AddAction(MATERIAL_TYPES_EDIT);
	}
}

void Scene::RemoveUnusedMaterials() {
	// Build a list of all referenced material names
	std::unordered_set<const Material *> referencedMats;

	// Add the camera volume
	if (camera && camera->HasVolume())
		referencedMats.insert(std::addressof(camera->GetVolume()));

	// Add the default world volume
	if (defaultWorldVolume)
		referencedMats.insert(defaultWorldVolume);

	for (u_int i = 0; i < objDefs.GetSize(); ++i) {
		objDefs.GetSceneObject(i).AddReferencedMaterials(referencedMats);
	}

	// Get the list of all defined materials
	bool deleted = false;
	for(const auto& matName: matDefs.GetMaterialNames()) {
		MaterialConstRef m = matDefs.GetMaterial(matName);

		if (referencedMats.count(&m) == 0) {
			SDL_LOG("Deleting unreferenced material: " << matName);
			auto oldMatPtr = matDefs.DeleteMaterial(matName);
			moveToTrash(std::move(oldMatPtr));
			deleted = true;
		}
	}

	if (deleted) {
		editActions.AddAction(MATERIALS_EDIT);
		editActions.AddAction(MATERIAL_TYPES_EDIT);
	}
}

void Scene::RemoveUnusedMeshes() {
	// Build a list of all referenced meshes
	std::unordered_set<const ExtMesh *> referencedMesh;
	for (u_int i = 0; i < objDefs.GetSize(); ++i)
		objDefs.GetSceneObject(i).AddReferencedMeshes(referencedMesh);

	// Get the list of all defined meshes
	bool deleted = false;
	for(const auto &extMeshName: extMeshCache.GetExtMeshNames()) {
		auto& mesh = extMeshCache.GetExtMesh(extMeshName);

		if (referencedMesh.count(&mesh) == 0) {
			SDL_LOG("Deleting unreferenced mesh: " << extMeshName);
			moveToTrash(extMeshCache.DeleteExtMesh(extMeshName));
			deleted = true;
		}
	}

	if (deleted)
		editActions.AddAction(GEOMETRY_EDIT);
}

void Scene::DeleteObject(const string &objName) {
	if (objDefs.IsSceneObjectDefined(objName)) {
		auto& oldObj = objDefs.GetSceneObject(objName);
		const bool wasLightSource = oldObj.GetMaterial().IsLightSource();

		// Check if the old object was a light source
		if (wasLightSource) {
			editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);

			// Delete all old triangle lights
			const auto& mesh = oldObj.GetExtMesh();
			const string prefix = Scene::EncodeTriangleLightNamePrefix(oldObj.GetName());
			for (u_int i = 0; i < mesh.GetTotalTriangleCount(); ++i)
				moveToTrash(lightDefs.DeleteLightSource(prefix + ToString(i)));
		}

		moveToTrash(objDefs.DeleteSceneObject(objName));

		editActions.AddAction(GEOMETRY_EDIT);
	}
}

void Scene::DeleteObjects(std::vector<string> &objNames) {
	// Delete the light sources
	for(const string &objName: objNames) {
		if (objDefs.IsSceneObjectDefined(objName)) {
			auto& oldObj = objDefs.GetSceneObject(objName);
			const bool wasLightSource = oldObj.GetMaterial().IsLightSource();

			// Check if the old object was a light source
			if (wasLightSource) {
				editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);

				// Delete all old triangle lights
				const auto& mesh = oldObj.GetExtMesh();
				const string prefix = Scene::EncodeTriangleLightNamePrefix(
					oldObj.GetName()
				);
				for (u_int i = 0; i < mesh.GetTotalTriangleCount(); ++i)
					moveToTrash(lightDefs.DeleteLightSource(prefix + ToString(i)));
			}
		}
	}

	objDefs.DeleteSceneObjects(objNames);

	editActions.AddAction(GEOMETRY_EDIT);
}

void Scene::DeleteLight(const string &lightName) {
	if (lightDefs.IsLightSourceDefined(lightName)) {
		moveToTrash(lightDefs.DeleteLightSource(lightName));

		editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
	}
}

void Scene::DeleteLights(std::vector<string> &lightNames) {
	// Separate the objects and send them to delete
	for(const string &lightName: lightNames) {
		DeleteLight(lightName);
	}
}

//------------------------------------------------------------------------------

bool Scene::Intersect(IntersectionDevice *device,
		const SceneRayType rayType, PathVolumeInfo *volInfo,
		const float initialPassThrough, Ray *ray, RayHit *rayHit, BSDF *bsdf,
		Spectrum *connectionThroughput, const Spectrum *pathThroughput,
		SampleResult *sampleResult, const bool backTracing) const {
	*connectionThroughput = Spectrum(1.f);

	// I need a sequence of pseudo-random numbers starting form a floating point
	// pseudo-random number
	TauswortheRandomGenerator rng(initialPassThrough);

	float passThrough = rng.floatValue();
	const float originalMaxT = ray->maxt;

	const bool fromLight = rayType & LIGHT_RAY;
	const bool cameraRay = rayType & CAMERA_RAY;
	const bool shadowRay = rayType & SHADOW_RAY;
	bool throughShadowTransparency = false;

	// This field can be checked by the calling code even if there was no
	// intersection (and not BSDF initialization)
	bsdf->hitPoint.throughShadowTransparency = false;

	for (;;) {
		bool hit = device ?
			device->TraceRay(ray, rayHit) :
			dataSet->GetAccelerator(ACCEL_EMBREE)->Intersect(ray, rayHit);

		bool bevelContinueToTrace = !hit;
		VolumeConstPtr rayVolume =
			volInfo->HasCurrentVolume() ?
			VolumeConstPtr(std::addressof(volInfo->GetCurrentVolume())) :
			VolumeConstPtr();
		if (hit) {
			bsdf->Init(
				fromLight,
				throughShadowTransparency,
				*this,
				*ray,
				*rayHit,
				passThrough,
				volInfo
			);
			rayVolume = bsdf->hitPoint.intoObject ?
				bsdf->hitPoint.exteriorVolume : bsdf->hitPoint.interiorVolume;

			// Check if it a triangle with bevel edges
			auto& mesh = objDefs.GetSceneObject(rayHit->meshIndex).GetExtMesh();
			if (mesh.GetBevelRadius() > 0.f) {
				float t;
				Point p;
				Normal n;
				if (mesh.IntersectBevel(*ray, *rayHit, bevelContinueToTrace, t, p, n)) {
					rayHit->t = t;

					// Update the BSDF with the new intersection point and normal
					bsdf->MoveHitPoint(p, n);
				}
			}

			ray->maxt = rayHit->t;
		} else if (!rayVolume) {
			// No volume information, I use the default volume
			rayVolume = defaultWorldVolume;
		}

		// Check if there is volume scatter event
		if (rayVolume) {
			// This applies volume transmittance too
			//
			// Note: by using passThrough here, I introduce subtle correlation
			// between scattering events and pass-through events
			Spectrum emis;
			const float t = rayVolume->Scatter(*ray, passThrough, volInfo->IsScatteredStart(),
					connectionThroughput, &emis);

			// Add the volume emitted light to the appropriate light group
			if (!emis.Black()) {
				if (sampleResult)
					sampleResult->AddEmission(rayVolume->GetVolumeLightID(), *pathThroughput, emis);
			}

			if (t > 0.f) {
				// There was a volume scatter event

				// I have to set RayHit fields even if there wasn't a real
				// ray hit
				rayHit->t = t;
				// This is a trick in order to have RayHit::Miss() return
				// false. I assume 0xfffffffeu will trigger a memory fault if
				// used (and the bug will be noticed)
				rayHit->meshIndex = 0xfffffffeu;

				bsdf->Init(
					fromLight,
					throughShadowTransparency,
					*this,
					*ray,
					*rayVolume,
					t,
					passThrough
				);
				volInfo->SetScatteredStart(true);

				return true;
			}
		}

		if (hit) {
			bool continueToTrace =
					// Check if was a false hit because of a bevel triangle edge
					bevelContinueToTrace ||
					// Check if the volume priority system tells me to continue to trace the ray
					volInfo->ContinueToTrace(*bsdf) ||
					// Check if it is a camera invisible object and we are a tracing a camera ray
					(cameraRay && objDefs.GetSceneObject(rayHit->meshIndex).IsCameraInvisible());

			// Check if it is a pass through point
			if (!continueToTrace) {
				const Spectrum transp = bsdf->GetPassThroughTransparency(backTracing);
				if (!transp.Black()) {
					*connectionThroughput *= transp;
					continueToTrace = true;
				}
			}

			if (!continueToTrace && shadowRay) {
				const Spectrum &shadowTransparency = bsdf->GetPassThroughShadowTransparency();
				
				if (!shadowTransparency.Black()) {
					*connectionThroughput *= shadowTransparency;
					throughShadowTransparency = true;
					continueToTrace = true;
				}
			}

			if (continueToTrace) {
				// Update volume information
				volInfo->Update(bsdf->GetEventTypes(), *bsdf);

				// It is a transparent material, continue to trace the ray
				ray->mint = rayHit->t + MachineEpsilon::E(rayHit->t);
				ray->maxt = originalMaxT;

				// A safety check in case of not enough numerical precision
				if ((ray->mint == rayHit->t) || (ray->mint >= ray->maxt))
					return false;
			} else
				return true;
		} else {
			// Nothing was hit
			return false;
		}

		passThrough = rng.floatValue();
	}
}

//------------------------------------------------------------------------------

string Scene::EncodeTriangleLightNamePrefix(const string &objectName) {
	// It is important to encode triangle light names in short strings in order
	// to reduce the time to process very large number of light sources.

	const string prefix = objectName + "__triangle__light__";

	return (boost::format("TL%0zx_") % robin_hood::hash_bytes(prefix.data(), sizeof(char) * prefix.size())).str();
}

namespace slg {
// TODO This is a workaround to initialize references to scenes in default constructors.
// Correct solution would be to use boost serialization support for references
Scene NullScene;
}

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
