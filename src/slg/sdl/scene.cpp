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

#include <cstdlib>
#include <istream>
#include <stdexcept>
#include <sstream>
#include <set>
#include <vector>
#include <memory>

#include <boost/detail/container_fwd.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>
#include <boost/unordered_set.hpp>

#include "luxrays/core/dataset.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/properties.h"
#include "slg/editaction.h"
#include "slg/sampler/sampler.h"
#include "slg/sdl/sdl.h"
#include "slg/sdl/scene.h"
#include "slg/sdl/blender_texture.h"
#include "slg/core/sphericalfunction/sphericalfunction.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace slg::blender;

Scene::Scene(const float imageScale) {
	defaultWorldVolume = NULL;
	camera = NULL;

	dataSet = NULL;
	accelType = ACCEL_AUTO;
	enableInstanceSupport = true;

	editActions.AddAllAction();
	imgMapCache.SetImageResize(imageScale);
}

Scene::Scene(const string &fileName, const float imageScale) {
	defaultWorldVolume = NULL;
	// Just in case there is an unexpected exception during the scene loading
    camera = NULL;

	dataSet = NULL;
	accelType = ACCEL_AUTO;
	enableInstanceSupport = true;

	editActions.AddAllAction();
	imgMapCache.SetImageResize(imageScale);

	SDL_LOG("Reading scene: " << fileName);

	Properties scnProp(fileName);
	Parse(scnProp);
}

Scene::~Scene() {
	delete camera;

	delete dataSet;
}

void Scene::Preprocess(Context *ctx, const u_int filmWidth, const u_int filmHeight) {
	if (lightDefs.GetSize() == 0) {
		throw runtime_error("The scene doesn't include any light source (note: volume emission doesn't count for this check)");

		// There may be only a volume emitting light. However I ignore this case
		// because a lot of code has been written assuming that there is always
		// at least one light source (i.e. for direct light sampling).
		/*bool hasEmittingVolume = false;
		for (u_int i = 0; i < matDefs.GetSize(); ++i) {
			const Material *mat = matDefs.GetMaterial(i);
			// Check if it is a volume
			const Volume *vol = dynamic_cast<const Volume *>(mat);
			if (vol && vol->GetVolumeEmissionTexture() &&
					(vol->GetVolumeEmissionTexture()->Y() > 0.f)) {
				hasEmittingVolume = true;
				break;
			}
		}

		if (!hasEmittingVolume)
			throw runtime_error("The scene doesn't include any light source");*/
	}

	// Check if I have to update the camera
	if (editActions.Has(CAMERA_EDIT))
		camera->Update(filmWidth, filmHeight);

	// Check if I have to rebuild the dataset
	if (editActions.Has(GEOMETRY_EDIT)) {
		// Rebuild the data set
		delete dataSet;
		dataSet = new DataSet(ctx);
		dataSet->SetInstanceSupport(enableInstanceSupport);
		dataSet->SetAcceleratorType(accelType);

		// Add all objects
		for (u_int i = 0; i < objDefs.GetSize(); ++i)
			dataSet->Add(objDefs.GetSceneObject(i)->GetExtMesh());

		dataSet->Preprocess();
	}

	// Check if something has changed in light sources
	if (editActions.Has(GEOMETRY_EDIT) ||
			editActions.Has(MATERIALS_EDIT) ||
			editActions.Has(MATERIAL_TYPES_EDIT) ||
			editActions.Has(LIGHTS_EDIT) ||
			editActions.Has(IMAGEMAPS_EDIT)) {
		lightDefs.Preprocess(this);
	}

	editActions.Reset();
}

Properties Scene::ToProperties(const string &directoryName) {
		Properties props;

		// Write the camera information
		SDL_LOG("Saving camera information");
		props.Set(camera->ToProperties());

		// Save all not intersectable light sources
        SDL_LOG("Saving Not intersectable light sources:");
		for (u_int i = 0; i < lightDefs.GetSize(); ++i) {
			const LightSource *l = lightDefs.GetLightSource(i);
			if (dynamic_cast<const NotIntersectableLightSource *>(l))
				props.Set(((const NotIntersectableLightSource *)l)->ToProperties(imgMapCache));
		}

		// Write the image map information
		SDL_LOG("Saving image maps information:");
		vector<const ImageMap *> ims;
		imgMapCache.GetImageMaps(ims);
		for (u_int i = 0; i < ims.size(); ++i) {
			const string fileName = directoryName + "/imagemap-" + (boost::format("%05d") % i).str() + ".exr";
			SDL_LOG("  " + fileName);
			ims[i]->WriteImage(fileName);
		}

		// Write the textures information
		SDL_LOG("Saving textures information:");
		for (u_int i = 0; i < texDefs.GetSize(); ++i) {
			const Texture *tex = texDefs.GetTexture(i);
			SDL_LOG("  " + tex->GetName());
			props.Set(tex->ToProperties(imgMapCache));
		}

		// Write the volumes information
		SDL_LOG("Saving volumes information:");
		for (u_int i = 0; i < matDefs.GetSize(); ++i) {
			const Material *mat = matDefs.GetMaterial(i);
			// Check if it is a volume
			const Volume *vol = dynamic_cast<const Volume *>(mat);
			if (vol) {
				SDL_LOG("  " + vol->GetName());
				props.Set(vol->ToProperties());
			}
		}

		// Set the default world interior/exterior volume if required
		if (defaultWorldVolume) {
			const u_int index = matDefs.GetMaterialIndex(defaultWorldVolume);
			props.Set(Property("scene.world.volume.default")(matDefs.GetMaterial(index)->GetName()));
		}

		// Write the materials information
		SDL_LOG("Saving materials information:");
		for (u_int i = 0; i < matDefs.GetSize(); ++i) {
			const Material *mat = matDefs.GetMaterial(i);
			// Check if it is not a volume
			const Volume *vol = dynamic_cast<const Volume *>(mat);
			if (!vol) {
				SDL_LOG("  " + mat->GetName());
				props.Set(mat->ToProperties());
			}
		}

		// Write the mesh information
		SDL_LOG("Saving meshes information:");
		const vector<ExtMesh *> &meshes =  extMeshCache.GetMeshes();
		set<string> savedMeshes;
		double lastPrint = WallClockTime();
		for (u_int i = 0; i < meshes.size(); ++i) {
			if (WallClockTime() - lastPrint > 2.0) {
				SDL_LOG("  " << i << "/" << meshes.size());
				lastPrint = WallClockTime();
			}

			u_int meshIndex;
			if (meshes[i]->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
				const ExtInstanceTriangleMesh *m = (ExtInstanceTriangleMesh *)meshes[i];
				meshIndex = extMeshCache.GetExtMeshIndex(m->GetExtTriangleMesh());
			} else
				meshIndex = extMeshCache.GetExtMeshIndex(meshes[i]);
			const string fileName = directoryName + "/mesh-" + (boost::format("%05d") % meshIndex).str() + ".ply";

			// Check if I have already saved this mesh (mostly useful for instances)
			if (savedMeshes.find(fileName) == savedMeshes.end()) {
				//SDL_LOG("  " + fileName);
				meshes[i]->WritePly(fileName);
				savedMeshes.insert(fileName);
			}
		}

		SDL_LOG("Saving objects information:");
		lastPrint = WallClockTime();
		for (u_int i = 0; i < objDefs.GetSize(); ++i) {
			if (WallClockTime() - lastPrint > 2.0) {
				SDL_LOG("  " << i << "/" << objDefs.GetSize());
				lastPrint = WallClockTime();
			}

			const SceneObject *obj = objDefs.GetSceneObject(i);
			//SDL_LOG("  " + obj->GetName());
			props.Set(obj->ToProperties(extMeshCache));
		}

		return props;
}

//--------------------------------------------------------------------------
// Methods to build and edit a scene
//--------------------------------------------------------------------------

void Scene::DefineImageMap(const std::string &name, ImageMap *im) {
	imgMapCache.DefineImageMap(name, im);

	editActions.AddAction(IMAGEMAPS_EDIT);
}

void Scene::DefineImageMap(const std::string &name, float *cols, const float gamma,
	const u_int channels, const u_int width, const u_int height) {
	DefineImageMap(name, new ImageMap(cols, gamma, channels, width, height));

	editActions.AddAction(IMAGEMAPS_EDIT);
}

bool Scene::IsImageMapDefined(const std::string &imgMapName) const {
	return imgMapCache.IsImageMapDefined(imgMapName);
}

void Scene::DefineMesh(const std::string &meshName, luxrays::ExtTriangleMesh *mesh) {
	extMeshCache.DefineExtMesh(meshName, mesh);

	editActions.AddAction(GEOMETRY_EDIT);
}

void Scene::DefineMesh(const std::string &meshName,
	const long plyNbVerts, const long plyNbTris,
	luxrays::Point *p, luxrays::Triangle *vi, luxrays::Normal *n, luxrays::UV *uv,
	luxrays::Spectrum *cols, float *alphas) {
	extMeshCache.DefineExtMesh(meshName, plyNbVerts, plyNbTris, p, vi, n, uv, cols, alphas);

	editActions.AddAction(GEOMETRY_EDIT);
}

bool Scene::IsMeshDefined(const std::string &meshName) const {
	return extMeshCache.IsExtMeshDefined(meshName);
}

bool Scene::IsTextureDefined(const std::string &texName) const {
	return texDefs.IsTextureDefined(texName);
}

bool Scene::IsMaterialDefined(const std::string &matName) const {
	return matDefs.IsMaterialDefined(matName);
}

void Scene::Parse(const Properties &props) {
	sceneProperties.Set(props);

	//--------------------------------------------------------------------------
	// Read camera position and target
	//--------------------------------------------------------------------------

	ParseCamera(props);

	//--------------------------------------------------------------------------
	// Read all textures
	//--------------------------------------------------------------------------

	ParseTextures(props);

	//--------------------------------------------------------------------------
	// Read all volumes
	//--------------------------------------------------------------------------

	ParseVolumes(props);

	//--------------------------------------------------------------------------
	// Read all materials
	//--------------------------------------------------------------------------

	ParseMaterials(props);

	//--------------------------------------------------------------------------
	// Read all objects .ply file
	//--------------------------------------------------------------------------

	ParseObjects(props);

	//--------------------------------------------------------------------------
	// Read all env. lights
	//--------------------------------------------------------------------------

	ParseLights(props);
}

void Scene::ParseCamera(const Properties &props) {
	if (!props.HaveNames("scene.camera")) {
		// There is no camera definition
		return;
	}

	Camera *newCamera = Camera::AllocCamera(props);

	// Use the new camera
	delete camera;
	camera = newCamera;

	editActions.AddAction(CAMERA_EDIT);
}

void Scene::ParseTextures(const Properties &props) {
	vector<string> texKeys = props.GetAllUniqueSubNames("scene.textures");
	if (texKeys.size() == 0) {
		// There are not texture definitions
		return;
	}

	BOOST_FOREACH(const string &key, texKeys) {
		// Extract the texture name
		const string texName = Property::ExtractField(key, 2);
		if (texName == "")
			throw runtime_error("Syntax error in texture definition: " + texName);

		SDL_LOG("Texture definition: " << texName);

		Texture *tex = CreateTexture(texName, props);

		if (texDefs.IsTextureDefined(texName)) {
			// A replacement for an existing texture
			const Texture *oldTex = texDefs.GetTexture(texName);

			texDefs.DefineTexture(texName, tex);
			matDefs.UpdateTextureReferences(oldTex, tex);
		} else {
			// Only a new texture
			texDefs.DefineTexture(texName, tex);
		}
	}

	editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
}

void Scene::ParseVolumes(const Properties &props) {
	vector<string> matKeys = props.GetAllUniqueSubNames("scene.volumes");
	BOOST_FOREACH(const string &key, matKeys) {
		// Extract the volume name
		const string volName = Property::ExtractField(key, 2);
		if (volName == "")
			throw runtime_error("Syntax error in volume definition: " + volName);

		SDL_LOG("Volume definition: " << volName);
		// In order to have harlequin colors with MATERIAL_ID output
		const u_int volID = ((u_int)(RadicalInverse(matDefs.GetSize() + 1, 2) * 255.f + .5f)) |
				(((u_int)(RadicalInverse(matDefs.GetSize() + 1, 3) * 255.f + .5f)) << 8) |
				(((u_int)(RadicalInverse(matDefs.GetSize() + 1, 5) * 255.f + .5f)) << 16);
		// Volumes are just a special kind of materials so they are stored
		// in matDefs too.
		Material *newMat = CreateVolume(volID, volName, props);

		if (matDefs.IsMaterialDefined(volName)) {
			// A replacement for an existing material
			const Material *oldMat = matDefs.GetMaterial(volName);
			// Volumes can not (yet) be light sources
			//const bool wasLightSource = oldMat->IsLightSource();

			matDefs.DefineMaterial(volName, newMat);

			// Replace old material direct references with new one
			objDefs.UpdateMaterialReferences(oldMat, newMat);
			//lightDefs.UpdateMaterialReferences(oldMat, newMat);

			// Check also the world default volume
			if (defaultWorldVolume == oldMat)
				defaultWorldVolume = (Volume *)newMat;

			// Check if the old material was or the new material is a light source
			//if (wasLightSource || newMat->IsLightSource())
			//	editActions.AddAction(LIGHTS_EDIT);
		} else {
			// Only a new Material
			matDefs.DefineMaterial(volName, newMat);
		}
	}

	if (props.IsDefined("scene.world.volume.default")) {
		const string volName = props.Get("scene.world.volume.default").Get<string>();
		const Material *m = matDefs.GetMaterial(volName);
		const Volume *v = dynamic_cast<const Volume *>(m);
		if (!v)
			throw runtime_error(volName + " is not a volume and can not be used for default world volume");
		defaultWorldVolume = v;

		editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
	}

	if (matKeys.size() > 0)
		editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
}

void Scene::ParseMaterials(const Properties &props) {
	vector<string> matKeys = props.GetAllUniqueSubNames("scene.materials");
	if (matKeys.size() == 0) {
		// There are not material definitions
		return;
	}

	BOOST_FOREACH(const string &key, matKeys) {
		// Extract the material name
		const string matName = Property::ExtractField(key, 2);
		if (matName == "")
			throw runtime_error("Syntax error in material definition: " + matName);

		SDL_LOG("Material definition: " << matName);

		// In order to have harlequin colors with MATERIAL_ID output
		const u_int matID = ((u_int)(RadicalInverse(matDefs.GetSize() + 1, 2) * 255.f + .5f)) |
				(((u_int)(RadicalInverse(matDefs.GetSize() + 1, 3) * 255.f + .5f)) << 8) |
				(((u_int)(RadicalInverse(matDefs.GetSize() + 1, 5) * 255.f + .5f)) << 16);
		Material *newMat = CreateMaterial(matID, matName, props);

		if (matDefs.IsMaterialDefined(matName)) {
			// A replacement for an existing material
			const Material *oldMat = matDefs.GetMaterial(matName);
			const bool wasLightSource = oldMat->IsLightSource();

			matDefs.DefineMaterial(matName, newMat);

			// Replace old material direct references with new one
			objDefs.UpdateMaterialReferences(oldMat, newMat);
			lightDefs.UpdateMaterialReferences(oldMat, newMat);

			// Check if the old material was or the new material is a light source
			if (wasLightSource || newMat->IsLightSource())
				editActions.AddAction(LIGHTS_EDIT);
		} else {
			// Only a new Material
			matDefs.DefineMaterial(matName, newMat);
		}
	}

	editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
}

void Scene::ParseObjects(const Properties &props) {
	vector<string> objKeys = props.GetAllUniqueSubNames("scene.objects");
	if (objKeys.size() == 0) {
		// There are not object definitions
		return;
	}

	double lastPrint = WallClockTime();
	u_int objCount = 0;
	BOOST_FOREACH(const string &key, objKeys) {
		// Extract the object name
		const string objName = Property::ExtractField(key, 2);
		if (objName == "")
			throw runtime_error("Syntax error in " + key);

		SceneObject *obj = CreateObject(objName, props);

		if (objDefs.IsSceneObjectDefined(objName)) {
			// A replacement for an existing object
			const SceneObject *oldObj = objDefs.GetSceneObject(objName);
			const bool wasLightSource = oldObj->GetMaterial()->IsLightSource();

			objDefs.DefineSceneObject(objName, obj);

			// Check if the old material was or the new material is a light source
			if (wasLightSource || obj->GetMaterial()->IsLightSource())
				editActions.AddAction(LIGHTS_EDIT);
		} else {
			// Only a new object
			objDefs.DefineSceneObject(objName, obj);

			// Check if it is a light source
			const Material *mat = obj->GetMaterial();
			if (mat->IsLightSource()) {
				const ExtMesh *mesh = obj->GetExtMesh();
				SDL_LOG("The " << objName << " object is a light sources with " << mesh->GetTotalTriangleCount() << " triangles");

				for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i) {
					TriangleLight *tl = new TriangleLight();
					tl->lightMaterial = mat;
					tl->mesh = mesh;
					tl->meshIndex = objDefs.GetSize() - 1;
					tl->triangleIndex = i;
					tl->Preprocess();

					lightDefs.DefineLightSource(objName + "_triangle_light_" + ToString(i), tl);
				}
			}
		}

		++objCount;

		const double now = WallClockTime();
		if (now - lastPrint > 2.0) {
			SDL_LOG("PLY object count: " << objCount);
			lastPrint = now;
		}
	}
	SDL_LOG("PLY object count: " << objCount);

	editActions.AddActions(GEOMETRY_EDIT);
}

void Scene::ParseLights(const Properties &props) {
	// The following code is used only for compatibility with the past syntax
	if (props.HaveNames("scene.skylight")) {
		// Parse all syntax
		LightSource *newLight = CreateLightSource("scene.skylight", props);
		lightDefs.DefineLightSource("skylight", newLight);
		editActions.AddActions(LIGHTS_EDIT);
	}
	if (props.HaveNames("scene.infinitelight")) {
		// Parse all syntax
		LightSource *newLight = CreateLightSource("scene.infinitelight", props);
		lightDefs.DefineLightSource("infinitelight", newLight);
		editActions.AddActions(LIGHTS_EDIT);
	}
	if (props.HaveNames("scene.sunlight")) {
		// Parse all syntax
		LightSource *newLight = CreateLightSource("scene.sunlight", props);
		lightDefs.DefineLightSource("sunlight", newLight);
		editActions.AddActions(LIGHTS_EDIT);
	}

	vector<string> lightKeys = props.GetAllUniqueSubNames("scene.lights");
	if (lightKeys.size() == 0) {
		// There are not light definitions
		return;
	}

	BOOST_FOREACH(const string &key, lightKeys) {
		// Extract the light name
		const string lightName = Property::ExtractField(key, 2);
		if (lightName == "")
			throw runtime_error("Syntax error in light definition: " + lightName);

		SDL_LOG("Light definition: " << lightName);

		LightSource *newLight = CreateLightSource(lightName, props);
		lightDefs.DefineLightSource(lightName, newLight);
	}

	editActions.AddActions(LIGHTS_EDIT);
}

void Scene::UpdateObjectTransformation(const string &objName, const Transform &trans) {
	SceneObject *obj = objDefs.GetSceneObject(objName);
	ExtMesh *mesh = obj->GetExtMesh();

	ExtInstanceTriangleMesh *instanceMesh = dynamic_cast<ExtInstanceTriangleMesh *>(mesh);
	if (instanceMesh)
		instanceMesh->SetTransformation(trans);
	else
		mesh->ApplyTransform(trans);

	// Check if it is a light source
	if (obj->GetMaterial()->IsLightSource()) {
		// Have to update all light sources using this mesh
		for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i)
			lightDefs.GetLightSource(objName + "_triangle_light_" + ToString(i))->Preprocess();
	}

	editActions.AddAction(GEOMETRY_EDIT);
}

void Scene::RemoveUnusedImageMaps() {
	// Build a list of all referenced image maps
	boost::unordered_set<const ImageMap *> referencedImgMaps;
	for (u_int i = 0; i < texDefs.GetSize(); ++i)
		texDefs.GetTexture(i)->AddReferencedImageMaps(referencedImgMaps);

	// Add the light image maps
	BOOST_FOREACH(LightSource *l, lightDefs.GetLightSources())
		l->AddReferencedImageMaps(referencedImgMaps);

	// Add the material image maps
	BOOST_FOREACH(Material *m, matDefs.GetMaterials())
		m->AddReferencedImageMaps(referencedImgMaps);

	// Get the list of all defined image maps
	std::vector<const ImageMap *> ims;
	imgMapCache.GetImageMaps(ims);
	BOOST_FOREACH(const ImageMap *im, ims) {
		if (referencedImgMaps.count(im) == 0) {
			SDL_LOG("Deleting unreferenced texture: " << imgMapCache.GetPath(im));
			imgMapCache.DeleteImageMap(im);
		}
	}
}

void Scene::RemoveUnusedTextures() {
	// Build a list of all referenced textures names
	boost::unordered_set<const Texture *> referencedTexs;
	for (u_int i = 0; i < matDefs.GetSize(); ++i)
		matDefs.GetMaterial(i)->AddReferencedTextures(referencedTexs);

	// Get the list of all defined textures
	vector<string> definedTexs = texDefs.GetTextureNames();
	BOOST_FOREACH(const string  &texName, definedTexs) {
		Texture *t = texDefs.GetTexture(texName);

		if (referencedTexs.count(t) == 0) {
			SDL_LOG("Deleting unreferenced texture: " << texName);
			texDefs.DeleteTexture(texName);

			// Delete the texture definition from the properties
			sceneProperties.DeleteAll(sceneProperties.GetAllNames("scene.textures." + texName));
		}
	}
}

void Scene::RemoveUnusedMaterials() {
	// Build a list of all referenced material names
	boost::unordered_set<const Material *> referencedMats;

	// Add the default world volume
	if (defaultWorldVolume)
		referencedMats.insert(defaultWorldVolume);

	for (u_int i = 0; i < objDefs.GetSize(); ++i)
		objDefs.GetSceneObject(i)->AddReferencedMaterials(referencedMats);

	// Get the list of all defined materials
	const vector<string> definedMats = matDefs.GetMaterialNames();
	BOOST_FOREACH(const string  &matName, definedMats) {
		Material *m = matDefs.GetMaterial(matName);

		if (referencedMats.count(m) == 0) {
			SDL_LOG("Deleting unreferenced material: " << matName);
			matDefs.DeleteMaterial(matName);

			// Delete the material definition from the properties
			sceneProperties.DeleteAll(sceneProperties.GetAllNames("scene.materials." + matName));
		}
	}
}

void Scene::RemoveUnusedMeshes() {
	// Build a list of all referenced meshes
	boost::unordered_set<const ExtMesh *> referencedMesh;
	for (u_int i = 0; i < objDefs.GetSize(); ++i)
		objDefs.GetSceneObject(i)->AddReferencedMeshes(referencedMesh);

	// Get the list of all defined objects
	const vector<string> definedObjects = objDefs.GetSceneObjectNames();
	BOOST_FOREACH(const string  &objName, definedObjects) {
		SceneObject *obj = objDefs.GetSceneObject(objName);

		if (referencedMesh.count(obj->GetExtMesh()) == 0) {
			SDL_LOG("Deleting unreferenced mesh: " << objName);
			objDefs.DeleteSceneObject(objName);

			// Delete the object definition from the properties
			sceneProperties.DeleteAll(sceneProperties.GetAllNames("scene.objects." + objName));
		}
	}
}

void Scene::DeleteObject(const std::string &objName) {
	if (objDefs.IsSceneObjectDefined(objName)) {
		if (objDefs.GetSceneObject(objName)->GetMaterial()->IsLightSource())
			editActions.AddAction(LIGHTS_EDIT);

		objDefs.DeleteSceneObject(objName);

		editActions.AddAction(GEOMETRY_EDIT);

		// Delete the object definition from the properties
		sceneProperties.DeleteAll(sceneProperties.GetAllNames("scene.objects." + objName));
	}
}

//------------------------------------------------------------------------------

TextureMapping2D *Scene::CreateTextureMapping2D(const string &prefixName, const Properties &props) {
	const string mapType = props.Get(Property(prefixName + ".type")("uvmapping2d")).Get<string>();

	if (mapType == "uvmapping2d") {
		const UV uvScale = props.Get(Property(prefixName + ".uvscale")(1.f, 1.f)).Get<UV>();
		const UV uvDelta = props.Get(Property(prefixName + ".uvdelta")(0.f, 0.f)).Get<UV>();

		return new UVMapping2D(uvScale.u, uvScale.v, uvDelta.u, uvDelta.v);
	} else
		throw runtime_error("Unknown 2D texture coordinate mapping type: " + mapType);
}

TextureMapping3D *Scene::CreateTextureMapping3D(const string &prefixName, const Properties &props) {
	const string mapType = props.Get(Property(prefixName + ".type")("uvmapping3d")).Get<string>();

	if (mapType == "uvmapping3d") {
		PropertyValues matIdentity(16);
		for (u_int i = 0; i < 4; ++i) {
			for (u_int j = 0; j < 4; ++j) {
				matIdentity[i * 4 + j] = (i == j) ? 1.f : 0.f;
			}
		}

		const Matrix4x4 mat = props.Get(Property(prefixName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		return new UVMapping3D(trans);
	} else if (mapType == "globalmapping3d") {
		PropertyValues matIdentity(16);
		for (u_int i = 0; i < 4; ++i) {
			for (u_int j = 0; j < 4; ++j) {
				matIdentity[i * 4 + j] = (i == j) ? 1.f : 0.f;
			}
		}

		const Matrix4x4 mat = props.Get(Property(prefixName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		return new GlobalMapping3D(trans);
	} else
		throw runtime_error("Unknown 3D texture coordinate mapping type: " + mapType);
}

Texture *Scene::CreateTexture(const string &texName, const Properties &props) {
	const string propName = "scene.textures." + texName;
	const string texType = props.Get(Property(propName + ".type")("imagemap")).Get<string>();

	if (texType == "imagemap") {
		const string name = props.Get(Property(propName + ".file")("image.png")).Get<string>();
		const float gamma = props.Get(Property(propName + ".gamma")(2.2f)).Get<float>();
		const float gain = props.Get(Property(propName + ".gain")(1.f)).Get<float>();
		const string stype = props.Get(Property(propName + ".channel")("default")).Get<string>();

		ImageMap::ChannelSelectionType selectionType;
		if (stype == "default")
			selectionType = ImageMap::DEFAULT;
		else if (stype == "red")
			selectionType = ImageMap::RED;
		else if (stype == "green")
			selectionType = ImageMap::GREEN;
		else if (stype == "blue")
			selectionType = ImageMap::BLUE;
		else if (stype == "alpha")
			selectionType = ImageMap::ALPHA;
		else if (stype == "mean")
			selectionType = ImageMap::MEAN;
		else if (stype == "colored_mean")
			selectionType = ImageMap::WEIGHTED_MEAN;
		else
			throw runtime_error("Unknown channel selection type in imagemap: " + texName);
			
		ImageMap *im = imgMapCache.GetImageMap(name, gamma, selectionType);
		return new ImageMapTexture(im, CreateTextureMapping2D(propName + ".mapping", props), gain);
	} else if (texType == "constfloat1") {
		const float v = props.Get(Property(propName + ".value")(1.f)).Get<float>();
		return new ConstFloatTexture(v);
	} else if (texType == "constfloat3") {
		const Spectrum v = props.Get(Property(propName + ".value")(1.f)).Get<Spectrum>();
		return new ConstFloat3Texture(v);
	} else if (texType == "scale") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		return new ScaleTexture(tex1, tex2);
	} else if (texType == "fresnelapproxn") {
		const Texture *tex = GetTexture(props.Get(Property(propName + ".texture")(.5f, .5f, .5f)));
		return new FresnelApproxNTexture(tex);
	} else if (texType == "fresnelapproxk") {
		const Texture *tex = GetTexture(props.Get(Property(propName + ".texture")(.5f, .5f, .5f)));
		return new FresnelApproxKTexture(tex);
	} else if (texType == "checkerboard2d") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(0.f)));

		return new CheckerBoard2DTexture(CreateTextureMapping2D(propName + ".mapping", props), tex1, tex2);
	} else if (texType == "checkerboard3d") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(0.f)));

		return new CheckerBoard3DTexture(CreateTextureMapping3D(propName + ".mapping", props), tex1, tex2);
	} else if (texType == "mix") {
		const Texture *amtTex = GetTexture(props.Get(Property(propName + ".amount")(.5f)));
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(0.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));

		return new MixTexture(amtTex, tex1, tex2);
	} else if (texType == "fbm") {
		const int octaves = props.Get(Property(propName + ".octaves")(8)).Get<int>();
		const float omega = props.Get(Property(propName + ".roughness")(.5f)).Get<float>();

		return new FBMTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega);
	} else if (texType == "marble") {
		const int octaves = props.Get(Property(propName + ".octaves")(8)).Get<int>();
		const float omega = props.Get(Property(propName + ".roughness")(.5f)).Get<float>();
		const float scale = props.Get(Property(propName + ".scale")(1.f)).Get<float>();
		const float variation = props.Get(Property(propName + ".variation")(.2f)).Get<float>();

		return new MarbleTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega, scale, variation);
	} else if (texType == "blender_blend") {
		const std::string progressiontype = props.Get(Property(propName + ".progressiontype")("linear")).Get<string>();
		const std::string direct = props.Get(Property(propName + ".direction")("horizontal")).Get<string>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		return new BlenderBlendTexture(CreateTextureMapping3D(propName + ".mapping", props),
				progressiontype, (direct=="vertical"), bright, contrast);
	} else if (texType == "blender_clouds") {
		const std::string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const std::string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const int noisedepth = props.Get(Property(propName + ".noisedepth")(2)).Get<int>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		return new BlenderCloudsTexture(CreateTextureMapping3D(propName + ".mapping", props),
				noisebasis, noisesize, noisedepth,(hard=="hard_noise"), bright, contrast);
	} else if (texType == "blender_distortednoise") {
		const std::string noisedistortion = props.Get(Property(propName + ".noise_distortion")("blender_original")).Get<string>();
		const std::string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const float distortion = props.Get(Property(propName + ".distortion")(1.f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		return new BlenderDistortedNoiseTexture(CreateTextureMapping3D(propName + ".mapping", props),
				noisedistortion, noisebasis, distortion, noisesize, bright, contrast);
	} else if (texType == "blender_magic") {
		const int noisedepth = props.Get(Property(propName + ".noisedepth")(2)).Get<int>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		return new BlenderMagicTexture(CreateTextureMapping3D(propName + ".mapping", props),
				noisedepth, turbulence, bright, contrast);
	} else if (texType == "blender_marble") {
		const std::string marbletype = props.Get(Property(propName + ".marbletype")("soft")).Get<string>();
		const std::string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const std::string noisebasis2 = props.Get(Property(propName + ".noisebasis2")("sin")).Get<string>();
		const int noisedepth = props.Get(Property(propName + ".noisedepth")(2)).Get<int>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(0.25f)).Get<float>();
		const std::string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		return new BlenderMarbleTexture(CreateTextureMapping3D(propName + ".mapping", props),
				marbletype, noisebasis, noisebasis2, noisesize, turbulence, noisedepth, (hard=="hard_noise"), bright, contrast);
	} else if (texType == "blender_musgrave") {
		const std::string musgravetype = props.Get(Property(propName + ".musgravetype")("multifractal")).Get<string>();
		const std::string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const float dimension = props.Get(Property(propName + ".dimension")(1.f)).Get<float>();
		const float intensity = props.Get(Property(propName + ".intensity")(1.f)).Get<float>();
		const float lacunarity = props.Get(Property(propName + ".lacunarity")(1.f)).Get<float>();
		const float offset = props.Get(Property(propName + ".offset")(1.f)).Get<float>();
		const float gain = props.Get(Property(propName + ".gain")(1.f)).Get<float>();
		const float octaves = props.Get(Property(propName + ".octaves")(2.f)).Get<float>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(0.25f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		return new BlenderMusgraveTexture(CreateTextureMapping3D(propName + ".mapping", props),
				musgravetype, noisebasis, dimension, intensity, lacunarity, offset, gain, octaves, noisesize, bright, contrast);
	} else if (texType == "blender_noise") {
		const int noisedepth = props.Get(Property(propName + ".noisedepth")(2)).Get<int>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		return new BlenderNoiseTexture(noisedepth, bright, contrast);
	} else if (texType == "blender_stucci") {
		const std::string woodtype = props.Get(Property(propName + ".stuccitype")("plastic")).Get<string>();
		const std::string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const std::string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		return new BlenderStucciTexture(CreateTextureMapping3D(propName + ".mapping", props),
				woodtype, noisebasis, noisesize, turbulence, (hard=="hard_noise"), bright, contrast);
	} else if (texType == "blender_wood") {
		const std::string woodtype = props.Get(Property(propName + ".woodtype")("bands")).Get<string>();
		const std::string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const std::string noisebasis2 = props.Get(Property(propName + ".noisebasis2")("sin")).Get<string>();
		const std::string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		return new BlenderWoodTexture(CreateTextureMapping3D(propName + ".mapping", props),
				woodtype, noisebasis2, noisebasis, noisesize, turbulence, (hard=="hard_noise"), bright, contrast);
	} else if (texType == "blender_voronoi") {
		const float intensity = props.Get(Property(propName + ".intensity")(1.f)).Get<float>();
		const float exponent = props.Get(Property(propName + ".exponent")(2.f)).Get<float>();
		const std::string distmetric = props.Get(Property(propName + ".distmetric")("actual_distance")).Get<string>();
		const float fw1 = props.Get(Property(propName + ".w1")(1.f)).Get<float>();
		const float fw2 = props.Get(Property(propName + ".w2")(0.f)).Get<float>();
		const float fw3 = props.Get(Property(propName + ".w3")(0.f)).Get<float>();
		const float fw4 = props.Get(Property(propName + ".w4")(0.f)).Get<float>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		return new BlenderVoronoiTexture(CreateTextureMapping3D(propName + ".mapping", props), intensity, exponent, fw1, fw2, fw3, fw4, distmetric, noisesize, bright, contrast);
	} else if (texType == "dots") {
		const Texture *insideTex = GetTexture(props.Get(Property(propName + ".inside")(1.f)));
		const Texture *outsideTex = GetTexture(props.Get(Property(propName + ".outside")(0.f)));

		return new DotsTexture(CreateTextureMapping2D(propName + ".mapping", props), insideTex, outsideTex);
	} else if (texType == "brick") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".bricktex")(1.f, 1.f, 1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".mortartex")(.2f, .2f, .2f)));
		const Texture *tex3 = GetTexture(props.Get(Property(propName + ".brickmodtex")(1.f, 1.f, 1.f)));

		const string brickbond = props.Get(Property(propName + ".brickbond")("running")).Get<string>();
		const float brickwidth = props.Get(Property(propName + ".brickwidth")(.3f)).Get<float>();
		const float brickheight = props.Get(Property(propName + ".brickheight")(.1f)).Get<float>();
		const float brickdepth = props.Get(Property(propName + ".brickdepth")(.15f)).Get<float>();
		const float mortarsize = props.Get(Property(propName + ".mortarsize")(.01f)).Get<float>();
		const float brickrun = props.Get(Property(propName + ".brickrun")(.75f)).Get<float>();
		const float brickbevel = props.Get(Property(propName + ".brickbevel")(0.f)).Get<float>();

		return new BrickTexture(CreateTextureMapping3D(propName + ".mapping", props), tex1, tex2, tex3,
				brickwidth, brickheight, brickdepth, mortarsize, brickrun, brickbevel, brickbond);
	} else if (texType == "add") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		return new AddTexture(tex1, tex2);
	} else if (texType == "windy") {
		return new WindyTexture(CreateTextureMapping3D(propName + ".mapping", props));
	} else if (texType == "wrinkled") {
		const int octaves = props.Get(Property(propName + ".octaves")(8)).Get<int>();
		const float omega = props.Get(Property(propName + ".roughness")(.5f)).Get<float>();

		return new WrinkledTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega);
	} else if (texType == "uv") {
		return new UVTexture(CreateTextureMapping2D(propName + ".mapping", props));
	} else if (texType == "band") {
		const Texture *amtTex = GetTexture(props.Get(Property(propName + ".amount")(.5f)));

		vector<float> offsets;
		vector<Spectrum> values;
		for (u_int i = 0; props.IsDefined(propName + ".offset" + ToString(i)); ++i) {
			const float offset = props.Get(Property(propName + ".offset" + ToString(i))(0.f)).Get<float>();
			const Spectrum value = props.Get(Property(propName + ".value" + ToString(i))(1.f, 1.f, 1.f)).Get<Spectrum>();

			offsets.push_back(offset);
			values.push_back(value);
		}
		if (offsets.size() == 0)
			throw runtime_error("Empty Band texture: " + texName);

		return new BandTexture(amtTex, offsets, values);
	} else if (texType == "hitpointcolor") {
		return new HitPointColorTexture();
	} else if (texType == "hitpointalpha") {
		return new HitPointAlphaTexture();
	} else if (texType == "hitpointgrey") {
		const int channel = props.Get(Property(propName + ".channel")(-1)).Get<int>();

		return new HitPointGreyTexture(((channel != 0) && (channel != 1) && (channel != 2)) ?
			numeric_limits<u_int>::max() : static_cast<u_int>(channel));
	} else
		throw runtime_error("Unknown texture type: " + texType);
}

Texture *Scene::GetTexture(const luxrays::Property &prop) {
	const string &name = prop.GetValuesString();

	if (texDefs.IsTextureDefined(name))
		return texDefs.GetTexture(name);
	else {
		// Check if it is an implicit declaration of a constant texture
		try {
			vector<string> strs;
			boost::split(strs, name, boost::is_any_of("\t "));

			vector<float> floats;
			BOOST_FOREACH(const string &s, strs) {
				if (s.length() != 0) {
					const double f = boost::lexical_cast<double>(s);
					floats.push_back(static_cast<float>(f));
				}
			}

			if (floats.size() == 1) {
				ConstFloatTexture *tex = new ConstFloatTexture(floats.at(0));
				texDefs.DefineTexture("Implicit-ConstFloatTexture-" + boost::lexical_cast<string>(tex), tex);

				return tex;
			} else if (floats.size() == 3) {
				ConstFloat3Texture *tex = new ConstFloat3Texture(Spectrum(floats.at(0), floats.at(1), floats.at(2)));
				texDefs.DefineTexture("Implicit-ConstFloatTexture3-" + boost::lexical_cast<string>(tex), tex);

				return tex;
			} else
				throw runtime_error("Wrong number of arguments in the implicit definition of a constant texture: " +
						boost::lexical_cast<string>(floats.size()));
		} catch (boost::bad_lexical_cast) {
			throw runtime_error("Syntax error in texture name: " + name);
		}
	}
}

Volume *Scene::CreateVolume(const u_int defaultVolID, const string &volName, const Properties &props) {
	const string propName = "scene.volumes." + volName;
	const string volType = props.Get(Property(propName + ".type")("homogenous")).Get<string>();

	const Texture *iorTex = GetTexture(props.Get(Property(propName + ".ior")(1.f)));
	const Texture *emissionTex = props.IsDefined(propName + ".emission") ?
		GetTexture(props.Get(Property(propName + ".emission")(0.f, 0.f, 0.f))) : NULL;
	// Required to remove light source while editing the scene
	if (emissionTex && (
			((emissionTex->GetType() == CONST_FLOAT) && (((ConstFloatTexture *)emissionTex)->GetValue() == 0.f)) ||
			((emissionTex->GetType() == CONST_FLOAT3) && (((ConstFloat3Texture *)emissionTex)->GetColor().Black()))))
		emissionTex = NULL;

	Volume *vol;
	if (volType == "clear") {
		const Texture *absorption = GetTexture(props.Get(Property(propName + ".absorption")(0.f, 0.f, 0.f)));

		vol = new ClearVolume(iorTex, emissionTex, absorption);
	} else if (volType == "homogeneous") {
		const Texture *absorption = GetTexture(props.Get(Property(propName + ".absorption")(0.f, 0.f, 0.f)));
		const Texture *scattering = GetTexture(props.Get(Property(propName + ".scattering")(0.f, 0.f, 0.f)));
		const Texture *asymmetry = GetTexture(props.Get(Property(propName + ".asymmetry")(0.f, 0.f, 0.f)));
		const bool multiScattering =  props.Get(Property(propName + ".multiscattering")(false)).Get<bool>();

		vol = new HomogeneousVolume(iorTex, emissionTex, absorption, scattering, asymmetry, multiScattering);
	} else if (volType == "heterogeneous") {
		const Texture *absorption = GetTexture(props.Get(Property(propName + ".absorption")(0.f, 0.f, 0.f)));
		const Texture *scattering = GetTexture(props.Get(Property(propName + ".scattering")(0.f, 0.f, 0.f)));
		const Texture *asymmetry = GetTexture(props.Get(Property(propName + ".asymmetry")(0.f, 0.f, 0.f)));
		const float stepSize =  props.Get(Property(propName + ".steps.size")(1.f)).Get<float>();
		const u_int maxStepsCount =  props.Get(Property(propName + ".steps.maxcount")(32u)).Get<u_int>();
		const bool multiScattering =  props.Get(Property(propName + ".multiscattering")(false)).Get<bool>();

		vol = new HeterogeneousVolume(iorTex, emissionTex, absorption, scattering, asymmetry, stepSize, maxStepsCount, multiScattering);
	} else
		throw runtime_error("Unknown volume type: " + volType);

	vol->SetID(props.Get(Property(propName + ".id")(defaultVolID)).Get<u_int>());

	vol->SetVolumeLightID(props.Get(Property(propName + ".emission.id")(0u)).Get<u_int>());
	vol->SetPriority(props.Get(Property(propName + ".priority")(0)).Get<int>());

	return vol;
}

Material *Scene::CreateMaterial(const u_int defaultMatID, const string &matName, const Properties &props) {
	const string propName = "scene.materials." + matName;
	const string matType = props.Get(Property(propName + ".type")("matte")).Get<string>();

	const Texture *emissionTex = props.IsDefined(propName + ".emission") ?
		GetTexture(props.Get(Property(propName + ".emission")(0.f, 0.f, 0.f))) : NULL;
	// Required to remove light source while editing the scene
	if (emissionTex && (
			((emissionTex->GetType() == CONST_FLOAT) && (((ConstFloatTexture *)emissionTex)->GetValue() == 0.f)) ||
			((emissionTex->GetType() == CONST_FLOAT3) && (((ConstFloat3Texture *)emissionTex)->GetColor().Black()))))
		emissionTex = NULL;

	Texture *bumpTex = props.IsDefined(propName + ".bumptex") ?
		GetTexture(props.Get(Property(propName + ".bumptex")(1.f))) : NULL;
    if (!bumpTex) {
        const Texture *normalTex = props.IsDefined(propName + ".normaltex") ?
            GetTexture(props.Get(Property(propName + ".normaltex")(1.f))) : NULL;

        if (normalTex) {
            bumpTex = new NormalMapTexture(normalTex);
            texDefs.DefineTexture("Implicit-NormalMapTexture-" + boost::lexical_cast<string>(bumpTex), bumpTex);
        }
    }

    const float bumpSampleDistance = props.Get(Property(propName + ".bumpsamplingdistance")(.001f)).Get<float>();

	Material *mat;
	if (matType == "matte") {
		const Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(.75f, .75f, .75f)));

		mat = new MatteMaterial(emissionTex, bumpTex, kd);
	} else if (matType == "roughmatte") {
		const Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(.75f, .75f, .75f)));
		const Texture *sigma = GetTexture(props.Get(Property(propName + ".sigma")(0.f)));

		mat = new RoughMatteMaterial(emissionTex, bumpTex, kd, sigma);
	} else if (matType == "mirror") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));

		mat = new MirrorMaterial(emissionTex, bumpTex, kr);
	} else if (matType == "glass") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));
		const Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(1.f, 1.f, 1.f)));

		Texture *exteriorIor = NULL;
		Texture *interiorIor = NULL;
		// For compatibility with the past
		if (props.IsDefined(propName + ".ioroutside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".ioroutside");
			exteriorIor = GetTexture(props.Get(Property(propName + ".ioroutside")(1.f)));
		} else if (props.IsDefined(propName + ".exteriorior"))
			exteriorIor = GetTexture(props.Get(Property(propName + ".exteriorior")(1.f)));
		// For compatibility with the past
		if (props.IsDefined(propName + ".iorinside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".iorinside");
			interiorIor = GetTexture(props.Get(Property(propName + ".iorinside")(1.5f)));
		} else if (props.IsDefined(propName + ".interiorior"))
			interiorIor = GetTexture(props.Get(Property(propName + ".interiorior")(1.5f)));

		mat = new GlassMaterial(emissionTex, bumpTex, kr, kt, exteriorIor, interiorIor);
	} else if (matType == "archglass") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));
		const Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(1.f, 1.f, 1.f)));

		Texture *exteriorIor = NULL;
		Texture *interiorIor = NULL;
		// For compatibility with the past
		if (props.IsDefined(propName + ".ioroutside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".ioroutside");
			exteriorIor = GetTexture(props.Get(Property(propName + ".ioroutside")(1.f)));
		} else if (props.IsDefined(propName + ".exteriorior"))
			exteriorIor = GetTexture(props.Get(Property(propName + ".exteriorior")(1.f)));
		// For compatibility with the past
		if (props.IsDefined(propName + ".iorinside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".iorinside");
			interiorIor = GetTexture(props.Get(Property(propName + ".iorinside")(1.f)));
		} else if (props.IsDefined(propName + ".interiorior"))
			interiorIor = GetTexture(props.Get(Property(propName + ".interiorior")(1.f)));

		mat = new ArchGlassMaterial(emissionTex, bumpTex, kr, kt, exteriorIor, interiorIor);
	} else if (matType == "mix") {
		Material *matA = matDefs.GetMaterial(props.Get(Property(propName + ".material1")("mat1")).Get<string>());
		Material *matB = matDefs.GetMaterial(props.Get(Property(propName + ".material2")("mat2")).Get<string>());
		const Texture *mix = GetTexture(props.Get(Property(propName + ".amount")(.5f)));

		MixMaterial *mixMat = new MixMaterial(bumpTex, matA, matB, mix);

		// Check if there is a loop in Mix material definition
		// (Note: this can not really happen at the moment because forward
		// declarations are not supported)
		if (mixMat->IsReferencing(mixMat))
			throw runtime_error("There is a loop in Mix material definition: " + matName);

		mat = mixMat;
	} else if (matType == "null") {
		mat = new NullMaterial();
	} else if (matType == "mattetranslucent") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(.5f, .5f, .5f)));
		const Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(.5f, .5f, .5f)));

		mat = new MatteTranslucentMaterial(emissionTex, bumpTex, kr, kt);
	} else if (matType == "roughmattetranslucent") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(.5f, .5f, .5f)));
		const Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(.5f, .5f, .5f)));
		const Texture *sigma = GetTexture(props.Get(Property(propName + ".sigma")(0.f)));

		mat = new RoughMatteTranslucentMaterial(emissionTex, bumpTex, kr, kt, sigma);
	} else if (matType == "glossy2") {
		const Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(.5f, .5f, .5f)));
		const Texture *ks = GetTexture(props.Get(Property(propName + ".ks")(.5f, .5f, .5f)));
		const Texture *nu = GetTexture(props.Get(Property(propName + ".uroughness")(.1f)));
		const Texture *nv = GetTexture(props.Get(Property(propName + ".vroughness")(.1f)));
		const Texture *ka = GetTexture(props.Get(Property(propName + ".ka")(0.f, 0.f, 0.f)));
		const Texture *d = GetTexture(props.Get(Property(propName + ".d")(0.f)));
		const Texture *index = GetTexture(props.Get(Property(propName + ".index")(0.f, 0.f, 0.f)));
		const bool multibounce = props.Get(Property(propName + ".multibounce")(false)).Get<bool>();

		mat = new Glossy2Material(emissionTex, bumpTex, kd, ks, nu, nv, ka, d, index, multibounce);
	} else if (matType == "metal2") {
		const Texture *nu = GetTexture(props.Get(Property(propName + ".uroughness")(.1f)));
		const Texture *nv = GetTexture(props.Get(Property(propName + ".vroughness")(.1f)));

		const Texture *eta, *k;
		if (props.IsDefined(propName + ".preset")) {
			const string type = props.Get(Property(propName + ".preset")("aluminium")).Get<string>();

			if (type == "aluminium") {
				eta = GetTexture(Property("Implicit-Aluminium-eta")("1.697 0.879833 0.530174"));
				k = GetTexture(Property("Implicit-Aluminium-k")("9.30201 6.27604 4.89434"));
			} else if (type == "silver") {
				eta = GetTexture(Property("Implicit-Silver-eta")("0.155706 0.115925 0.138897"));
				k = GetTexture(Property("Implicit-Silver-k")("4.88648 3.12787 2.17797"));
			} else if (type == "gold") {
				eta = GetTexture(Property("Implicit-Gold-eta")("0.117959 0.354153 1.43897"));
				k = GetTexture(Property("Implicit-gold-k")("4.03165 2.39416 1.61967"));
			} else if (type == "copper") {
				eta = GetTexture(Property("Implicit-Copper-eta")("0.134794 0.928983 1.10888"));
				k = GetTexture(Property("Implicit-Copper-k")("3.98126 2.44098 2.16474"));
			} else if (type == "amorphous carbon") {
				eta = GetTexture(Property("Implicit-Amorphous-Carbon-eta")("2.94553 2.22816 1.98665"));
				k = GetTexture(Property("Implicit-Amorphous-Carbon-k")("0.876641 0.799505 0.821194"));
			} else
				throw runtime_error("Unknown Metal2 preset: " + type);
		} else {
			eta = GetTexture(props.Get(Property(propName + ".n")(.5f, .5f, .5f)));
			k = GetTexture(props.Get(Property(propName + ".k")(.5f, .5f, .5f)));
		}

		mat = new Metal2Material(emissionTex, bumpTex, eta, k, nu, nv);
	} else if (matType == "roughglass") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));
		const Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(1.f, 1.f, 1.f)));

		Texture *exteriorIor = NULL;
		Texture *interiorIor = NULL;
		// For compatibility with the past
		if (props.IsDefined(propName + ".ioroutside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".ioroutside");
			exteriorIor = GetTexture(props.Get(Property(propName + ".ioroutside")(1.f)));
		} else if (props.IsDefined(propName + ".exteriorior"))
			exteriorIor = GetTexture(props.Get(Property(propName + ".exteriorior")(1.f)));
		// For compatibility with the past
		if (props.IsDefined(propName + ".iorinside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".iorinside");
			interiorIor = GetTexture(props.Get(Property(propName + ".iorinside")(1.5f)));
		} else if (props.IsDefined(propName + ".interiorior"))
			interiorIor = GetTexture(props.Get(Property(propName + ".interiorior")(1.5f)));

		const Texture *nu = GetTexture(props.Get(Property(propName + ".uroughness")(.1f)));
		const Texture *nv = GetTexture(props.Get(Property(propName + ".vroughness")(.1f)));

		mat = new RoughGlassMaterial(emissionTex, bumpTex, kr, kt, exteriorIor, interiorIor, nu, nv);
	} else if (matType == "velvet") {
		const Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(.5f, .5f, .5f)));
		const Texture *p1 = GetTexture(props.Get(Property(propName + ".p1")(-2.0f)));
		const Texture *p2 = GetTexture(props.Get(Property(propName + ".p2")(20.0f)));
		const Texture *p3 = GetTexture(props.Get(Property(propName + ".p3")(2.0f)));
		const Texture *thickness = GetTexture(props.Get(Property(propName + ".thickness")(0.1f)));

		mat = new VelvetMaterial(emissionTex, bumpTex, kd, p1, p2, p3, thickness);
	} else if (matType == "cloth") {
		slg::ocl::ClothPreset preset = slg::ocl::DENIM;

		if (props.IsDefined(propName + ".preset")) {
			const string type = props.Get(Property(propName + ".preset")("denim")).Get<string>();

			if (type == "denim")
				preset = slg::ocl::DENIM;
			else if (type == "silk_charmeuse")
				preset = slg::ocl::SILKCHARMEUSE;
			else if (type == "silk_shantung")
				preset = slg::ocl::SILKSHANTUNG;
			else if (type == "cotton_twill")
				preset = slg::ocl::COTTONTWILL;
			else if (type == "wool_garbardine")
				preset = slg::ocl::WOOLGARBARDINE;
			else if (type == "polyester_lining_cloth")
				preset = slg::ocl::POLYESTER;
		}
		const Texture *weft_kd = GetTexture(props.Get(Property(propName + ".weft_kd")(.5f, .5f, .5f)));
		const Texture *weft_ks = GetTexture(props.Get(Property(propName + ".weft_ks")(.5f, .5f, .5f)));
		const Texture *warp_kd = GetTexture(props.Get(Property(propName + ".warp_kd")(.5f, .5f, .5f)));
		const Texture *warp_ks = GetTexture(props.Get(Property(propName + ".warp_ks")(.5f, .5f, .5f)));
		const float repeat_u = props.Get(Property(propName + ".repeat_u")(100.0f)).Get<float>();
		const float repeat_v = props.Get(Property(propName + ".repeat_v")(100.0f)).Get<float>();

		mat = new ClothMaterial(emissionTex, bumpTex, preset, weft_kd, weft_ks, warp_kd, warp_ks, repeat_u, repeat_v);
	} else if (matType == "carpaint") {
		const Texture *ka = GetTexture(props.Get(Property(propName + ".ka")(0.f, 0.f, 0.f)));
		const Texture *d = GetTexture(props.Get(Property(propName + ".d")(0.f)));

        mat = NULL; // To remove a GCC warning
		string preset = props.Get(Property(propName + ".preset")("")).Get<string>();
		if (preset != "") {
			const int numPaints = CarPaintMaterial::NbPresets();
			int i;
			for (i = 0; i < numPaints; ++i) {
				if (preset == CarPaintMaterial::data[i].name)
					break;
			}

			if (i == numPaints)
				preset = "";
			else {
				const Texture *kd = GetTexture(Property("Implicit-" + preset + "-kd")(CarPaintMaterial::data[i].kd[0], CarPaintMaterial::data[i].kd[1], CarPaintMaterial::data[i].kd[2]));
				const Texture *ks1 = GetTexture(Property("Implicit-" + preset + "-ks1")(CarPaintMaterial::data[i].ks1[0], CarPaintMaterial::data[i].ks1[1], CarPaintMaterial::data[i].ks1[2]));
				const Texture *ks2 = GetTexture(Property("Implicit-" + preset + "-ks2")(CarPaintMaterial::data[i].ks2[0], CarPaintMaterial::data[i].ks2[1], CarPaintMaterial::data[i].ks2[2]));
				const Texture *ks3 = GetTexture(Property("Implicit-" + preset + "-ks3")(CarPaintMaterial::data[i].ks3[0], CarPaintMaterial::data[i].ks3[1], CarPaintMaterial::data[i].ks3[2]));
				const Texture *r1 = GetTexture(Property("Implicit-" + preset + "-r1")(CarPaintMaterial::data[i].r1));
				const Texture *r2 = GetTexture(Property("Implicit-" + preset + "-r2")(CarPaintMaterial::data[i].r2));
				const Texture *r3 = GetTexture(Property("Implicit-" + preset + "-r3")(CarPaintMaterial::data[i].r3));
				const Texture *m1 = GetTexture(Property("Implicit-" + preset + "-m1")(CarPaintMaterial::data[i].m1));
				const Texture *m2 = GetTexture(Property("Implicit-" + preset + "-m2")(CarPaintMaterial::data[i].m2));
				const Texture *m3 = GetTexture(Property("Implicit-" + preset + "-m3")(CarPaintMaterial::data[i].m3));
				mat = new CarPaintMaterial(emissionTex, bumpTex, kd, ks1, ks2, ks3, m1, m2, m3, r1, r2, r3, ka, d);
			}
		}

		// preset can be reset above if the name is not found
		if (preset == "") {
			const Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(CarPaintMaterial::data[0].kd[0], CarPaintMaterial::data[0].kd[1], CarPaintMaterial::data[0].kd[2])));
			const Texture *ks1 = GetTexture(props.Get(Property(propName + ".ks1")(CarPaintMaterial::data[0].ks1[0], CarPaintMaterial::data[0].ks1[1], CarPaintMaterial::data[0].ks1[2])));
			const Texture *ks2 = GetTexture(props.Get(Property(propName + ".ks2")(CarPaintMaterial::data[0].ks2[0], CarPaintMaterial::data[0].ks2[1], CarPaintMaterial::data[0].ks2[2])));
			const Texture *ks3 = GetTexture(props.Get(Property(propName + ".ks3")(CarPaintMaterial::data[0].ks3[0], CarPaintMaterial::data[0].ks3[1], CarPaintMaterial::data[0].ks3[2])));
			const Texture *r1 = GetTexture(props.Get(Property(propName + ".r1")(CarPaintMaterial::data[0].r1)));
			const Texture *r2 = GetTexture(props.Get(Property(propName + ".r2")(CarPaintMaterial::data[0].r2)));
			const Texture *r3 = GetTexture(props.Get(Property(propName + ".r3")(CarPaintMaterial::data[0].r3)));
			const Texture *m1 = GetTexture(props.Get(Property(propName + ".m1")(CarPaintMaterial::data[0].m1)));
			const Texture *m2 = GetTexture(props.Get(Property(propName + ".m2")(CarPaintMaterial::data[0].m2)));
			const Texture *m3 = GetTexture(props.Get(Property(propName + ".m3")(CarPaintMaterial::data[0].m3)));
			mat = new CarPaintMaterial(emissionTex, bumpTex, kd, ks1, ks2, ks3, m1, m2, m3, r1, r2, r3, ka, d);
		}
	} else
		throw runtime_error("Unknown material type: " + matType);

	mat->SetSamples(Max(-1, props.Get(Property(propName + ".samples")(-1)).Get<int>()));
	mat->SetID(props.Get(Property(propName + ".id")(defaultMatID)).Get<u_int>());
    mat->SetBumpSampleDistance(bumpSampleDistance);

	mat->SetEmittedGain(props.Get(Property(propName + ".emission.gain")(Spectrum(1.f))).Get<Spectrum>());
	mat->SetEmittedPower(Max(0.f, props.Get(Property(propName + ".emission.power")(0.f)).Get<float>()));
	mat->SetEmittedEfficency(Max(0.f, props.Get(Property(propName + ".emission.efficency")(0.f)).Get<float>()));
	mat->SetEmittedSamples(Max(-1, props.Get(Property(propName + ".emission.samples")(-1)).Get<int>()));
	mat->SetLightID(props.Get(Property(propName + ".emission.id")(0u)).Get<u_int>());
	mat->SetEmittedImportance(props.Get(Property(propName + ".importance")(1.f)).Get<float>());

	mat->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
	mat->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
	mat->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

	// Check if there is a image or IES map
	const ImageMap *emissionMap = CreateEmissionMap(propName + ".emission", props);
	if (emissionMap) {
		// There is one
		mat->SetEmissionMap(emissionMap);
	}

	// Interior volumes
	if (props.IsDefined(propName + ".volume.interior")) {
		const string volName = props.Get(Property(propName + ".volume.interior")("vol1")).Get<string>();
		const Material *m = matDefs.GetMaterial(volName);
		const Volume *v = dynamic_cast<const Volume *>(m);
		if (!v)
			throw runtime_error(volName + " is not a volume and can not be used for material interior volume: " + matName);
		mat->SetInteriorVolume(v);
	}

	// Exterior volumes
	if (props.IsDefined(propName + ".volume.exterior")) {
		const string volName = props.Get(Property(propName + ".volume.exterior")("vol2")).Get<string>();
		const Material *m = matDefs.GetMaterial(volName);
		const Volume *v = dynamic_cast<const Volume *>(m);
		if (!v)
			throw runtime_error(volName + " is not a volume and can not be used for material exterior volume: " + matName);
		mat->SetExteriorVolume(v);
	}

	return mat;
}

SceneObject *Scene::CreateObject(const string &objName, const Properties &props) {
	const string propName = "scene.objects." + objName;

	// Extract the material name
	const string matName = props.Get(Property(propName + ".material")("")).Get<string>();
	if (matName == "")
		throw runtime_error("Syntax error in object material reference: " + objName);

	// Build the object
	ExtMesh *mesh;
	if (props.IsDefined(propName + ".ply")) {
		// The mesh definition is in a PLY file
		const string plyFileName = props.Get(Property(propName + ".ply")("")).Get<string>();
		if (plyFileName == "")
			throw runtime_error("Syntax error in object .ply file name: " + objName);

		// Check if I have to use a motion mesh, instance mesh or normal mesh
		if (props.IsDefined(propName + ".motion.0.time")) {
			// Build the motion system
			vector<float> times;
			vector<Transform> transforms;
			for (u_int i =0;; ++i) {
				const string prefix = propName + ".motion." + ToString(i);
				if (!props.IsDefined(prefix +".time"))
					break;

				const float t = props.Get(prefix +".time").Get<float>();
				if (i > 0 && t <= times.back())
					throw runtime_error(objName + " motion time must be monotonic");
				times.push_back(t);

				const Matrix4x4 mat = props.Get(Property(prefix +
					".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
				transforms.push_back(Transform(mat));
			}

			MotionSystem ms(times, transforms);
			mesh = extMeshCache.GetExtMesh(plyFileName, ms);
		} else if (props.IsDefined(propName + ".transformation")) {
			const Matrix4x4 mat = props.Get(Property(propName +
				".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();

			mesh = extMeshCache.GetExtMesh(plyFileName, Transform(mat));
		} else
			mesh = extMeshCache.GetExtMesh(plyFileName);
	} else {
		// The mesh definition is in-lined
		u_int pointsSize;
		Point *points;
		if (props.IsDefined(propName + ".vertices")) {
			Property prop = props.Get(propName + ".vertices");
			if ((prop.GetSize() == 0) || (prop.GetSize() % 3 != 0))
				throw runtime_error("Wrong object vertex list length: " + objName);

			pointsSize = prop.GetSize() / 3;
			points = new Point[pointsSize];
			for (u_int i = 0; i < pointsSize; ++i) {
				const u_int index = i * 3;
				points[i] = Point(prop.Get<float>(index), prop.Get<float>(index + 1), prop.Get<float>(index + 2));
			}
		} else
			throw runtime_error("Missing object vertex list: " + objName);

		u_int trisSize;
		Triangle *tris;
		if (props.IsDefined(propName + ".faces")) {
			Property prop = props.Get(propName + ".faces");
			if ((prop.GetSize() == 0) || (prop.GetSize() % 3 != 0))
				throw runtime_error("Wrong object face list length: " + objName);

			trisSize = prop.GetSize() / 3;
			tris = new Triangle[trisSize];
			for (u_int i = 0; i < trisSize; ++i) {
				const u_int index = i * 3;
				tris[i] = Triangle(prop.Get<u_int>(index), prop.Get<u_int>(index + 1), prop.Get<u_int>(index + 2));
			}
		} else {
			delete[] points;
			throw runtime_error("Missing object face list: " + objName);
		}

		Normal *normals = NULL;
		if (props.IsDefined(propName + ".normals")) {
			Property prop = props.Get(propName + ".normals");
			if ((prop.GetSize() == 0) || (prop.GetSize() / 3 != pointsSize))
				throw runtime_error("Wrong object normal list length: " + objName);

			normals = new Normal[pointsSize];
			for (u_int i = 0; i < pointsSize; ++i) {
				const u_int index = i * 3;
				normals[i] = Normal(prop.Get<float>(index), prop.Get<float>(index + 1), prop.Get<float>(index + 2));
			}
		}

		UV *uvs = NULL;
		if (props.IsDefined(propName + ".uvs")) {
			Property prop = props.Get(propName + ".uvs");
			if ((prop.GetSize() == 0) || (prop.GetSize() / 2 != pointsSize))
				throw runtime_error("Wrong object uv list length: " + objName);

			uvs = new UV[pointsSize];
			for (u_int i = 0; i < pointsSize; ++i) {
				const u_int index = i * 2;
				uvs[i] = UV(prop.Get<float>(index), prop.Get<float>(index + 1));
			}
		}

		if (props.IsDefined(propName + ".transformation")) {
			const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
			const Transform trans(mat);

			// Transform directly the vertices and normals. This mesh is 
			// defined on-the-fly and can be used only once
			for (u_int i = 0; i < pointsSize; ++i)
				points[i] = trans * points[i];
			if (normals) {
				for (u_int i = 0; i < pointsSize; ++i)
					normals[i] = trans * normals[i];
			}
		}

		extMeshCache.DefineExtMesh("InlinedMesh-" + objName, pointsSize, trisSize,
				points, tris, normals, uvs,
				NULL, NULL);
		mesh = extMeshCache.GetExtMesh("InlinedMesh-" + objName);
	}

	// Get the material
	if (!matDefs.IsMaterialDefined(matName))
		throw runtime_error("Unknown material: " + matName);
	const Material *mat = matDefs.GetMaterial(matName);

	return new SceneObject(mesh, mat);
}

ImageMap *Scene::CreateEmissionMap(const std::string &propName, const luxrays::Properties &props) {
	const string imgMapName = props.Get(Property(propName + ".mapfile")("")).Get<string>();
	const string iesName = props.Get(Property(propName + ".iesfile")("")).Get<string>();
	const float gamma = props.Get(Property(propName + ".gamma")(2.2f)).Get<float>();
	const u_int width = props.Get(Property(propName + ".map.width")(0)).Get<u_int>();
	const u_int height = props.Get(Property(propName + ".map.height")(0)).Get<u_int>();

	ImageMap *map = NULL;
	if ((imgMapName != "") && (iesName != "")) {
		ImageMap *imgMap = imgMapCache.GetImageMap(imgMapName, gamma, ImageMap::DEFAULT);

		ImageMap *iesMap;
		PhotometricDataIES data(iesName.c_str());
		if (data.IsValid()) {
			const bool flipZ = props.Get(Property(propName + ".flipz")(false)).Get<bool>();
			iesMap = IESSphericalFunction::IES2ImageMap(data, flipZ,
					(width > 0) ? width : 512,
					(height > 0) ? height : 256);
		} else
			throw runtime_error("Invalid IES file in property " + propName + ": " + iesName);

		// Merge the 2 maps
		map = ImageMap::Merge(imgMap, iesMap, imgMap->GetChannelCount(),
				(width > 0) ? width: Max(imgMap->GetWidth(), iesMap->GetWidth()),
				(height > 0) ? height : Max(imgMap->GetHeight(), iesMap->GetHeight()));
		delete iesMap;

		if ((width > 0) || (height > 0)) {
			// I have to resample the image
			ImageMap *resampledMap = ImageMap::Resample(map, map->GetChannelCount(),
					(width > 0) ? width: map->GetWidth(),
					(height > 0) ? height : map->GetHeight());
			delete map;
			map = resampledMap;
		}

		// Add the image map to the cache
		imgMapCache.DefineImageMap("LUXCORE_EMISSIONMAP_MERGEDMAP_" + propName, map);
	} else if (imgMapName != "") {
		map = imgMapCache.GetImageMap(imgMapName, gamma, ImageMap::DEFAULT);

		if ((width > 0) || (height > 0)) {
			// I have to resample the image
			map = ImageMap::Resample(map, map->GetChannelCount(),
					(width > 0) ? width: map->GetWidth(),
					(height > 0) ? height : map->GetHeight());

			// Add the image map to the cache
			imgMapCache.DefineImageMap("LUXCORE_EMISSIONMAP_RESAMPLED_" + propName, map);
		}
	} else if (iesName != "") {
		PhotometricDataIES data(iesName.c_str());
		if (data.IsValid()) {
			const bool flipZ = props.Get(Property(propName + ".flipz")(false)).Get<bool>();
			map = IESSphericalFunction::IES2ImageMap(data, flipZ,
					(width > 0) ? width : 512,
					(height > 0) ? height : 256);

			// Add the image map to the cache
			imgMapCache.DefineImageMap("LUXCORE_EMISSIONMAP_IES2IMAGEMAP_" + propName, map);
		} else
			throw runtime_error("Invalid IES file in property " + propName + ": " + iesName);
	}

	return map;
}

LightSource *Scene::CreateLightSource(const std::string &lightName, const luxrays::Properties &props) {
	string propName, lightType;

	// The following code is used only for compatibility with the past syntax
	if (lightName == "scene.skylight") {
		SLG_LOG("WARNING: deprecated property scene.skylight");

		propName = "scene.skylight";
		lightType = "sky";
	} else if (lightName == "scene.infinitelight") {
		SLG_LOG("WARNING: deprecated property scene.infinitelight");

		propName = "scene.infinitelight";
		lightType = "infinite";
	} else if (lightName == "scene.sunlight") {
		SLG_LOG("WARNING: deprecated property scene.sunlight");

		propName = "scene.sunlight";
		lightType = "sun";
	} else {
		propName = "scene.lights." + lightName;
		lightType = props.Get(Property(propName + ".type")("sky")).Get<string>();
	}

	NotIntersectableLightSource *lightSource = NULL;
	if (lightType == "sky") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SkyLight *sl = new SkyLight();
		sl->lightToWorld = light2World;
		sl->turbidity = Max(0.f, props.Get(Property(propName + ".turbidity")(2.2f)).Get<float>());
		sl->localSunDir = Normalize(props.Get(Property(propName + ".dir")(0.f, 0.f, 1.f)).Get<Vector>());

		sl->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		sl->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		sl->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		lightSource = sl;
	} else if (lightType == "sky2") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SkyLight2 *sl = new SkyLight2();
		sl->lightToWorld = light2World;
		sl->turbidity = Max(0.f, props.Get(Property(propName + ".turbidity")(2.2f)).Get<float>());
		sl->localSunDir = Normalize(props.Get(Property(propName + ".dir")(0.f, 0.f, 1.f)).Get<Vector>());

		sl->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		sl->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		sl->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		lightSource = sl;
	} else if (lightType == "infinite") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		const string imageName = props.Get(Property(propName + ".file")("image.png")).Get<string>();
		const float gamma = props.Get(Property(propName + ".gamma")(2.2f)).Get<float>();
		const ImageMap *imgMap = imgMapCache.GetImageMap(imageName, gamma, ImageMap::DEFAULT);

		InfiniteLight *il = new InfiniteLight();
		il->lightToWorld = light2World;
		il->imageMap = imgMap;

		// An old parameter kept only for compatibility
		const UV shift = props.Get(Property(propName + ".shift")(0.f, 0.f)).Get<UV>();
		il->mapping.uDelta = shift.u;
		il->mapping.vDelta = shift.v;

		il->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		il->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		il->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		lightSource = il;
	} else if (lightType == "sun") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SunLight *sl = new SunLight();
		sl->lightToWorld = light2World;
		sl->turbidity = Max(0.f, props.Get(Property(propName + ".turbidity")(2.2f)).Get<float>());
		sl->relSize = Max(0.f, props.Get(Property(propName + ".relsize")(1.0f)).Get<float>());
		sl->localSunDir = Normalize(props.Get(Property(propName + ".dir")(0.f, 0.f, 1.f)).Get<Vector>());

		sl->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		sl->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		sl->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		lightSource = sl;
	} else if (lightType == "point") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		PointLight *pl = new PointLight();
		pl->lightToWorld = light2World;
		pl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		pl->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		pl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		pl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = pl;
	} else if (lightType == "mappoint") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		ImageMap *map = CreateEmissionMap(propName, props);
		if (!map)
			throw runtime_error("MapPoint light source (" + propName + ") is missing mapfile or iesfile property");

		MapPointLight *mpl = new MapPointLight();
		mpl->lightToWorld = light2World;
		mpl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		mpl->imageMap = map;
		mpl->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		mpl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		mpl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = mpl;
	} else if (lightType == "spot") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SpotLight *sl = new SpotLight();
		sl->lightToWorld = light2World;
		sl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		sl->localTarget = props.Get(Property(propName + ".target")(Point(0.f, 0.f, 1.f))).Get<Point>();
		sl->coneAngle = Max(0.f, props.Get(Property(propName + ".coneangle")(30.f)).Get<float>());
		sl->coneDeltaAngle = Max(0.f, props.Get(Property(propName + ".conedeltaangle")(5.f)).Get<float>());
		sl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		sl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = sl;
	} else if (lightType == "projection") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		const string imageName = props.Get(Property(propName + ".mapfile")("")).Get<string>();
		const float gamma = props.Get(Property(propName + ".gamma")(2.2f)).Get<float>();
		const ImageMap *imgMap = (imageName == "") ? NULL : imgMapCache.GetImageMap(imageName, gamma, ImageMap::DEFAULT);

		ProjectionLight *pl = new ProjectionLight();
		pl->lightToWorld = light2World;
		pl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		pl->localTarget = props.Get(Property(propName + ".target")(Point(0.f, 0.f, 1.f))).Get<Point>();
		pl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		pl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());
		pl->imageMap = imgMap;
		pl->fov = Max(0.f, props.Get(Property(propName + ".fov")(45.f)).Get<float>());

		lightSource = pl;
	} else if (lightType == "laser") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		LaserLight *ll = new LaserLight();
		ll->lightToWorld = light2World;
		ll->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		ll->localTarget = props.Get(Property(propName + ".target")(Point(0.f, 0.f, 1.f))).Get<Point>();
		ll->radius = Max(0.f, props.Get(Property(propName + ".radius")(.01f)).Get<float>());
		ll->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		ll->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		ll->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = ll;
	} else if (lightType == "constantinfinite") {
		ConstantInfiniteLight *cil = new ConstantInfiniteLight();

		cil->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		cil->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		cil->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		cil->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		lightSource = cil;
	} else if (lightType == "sharpdistant") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SharpDistantLight *sdl = new SharpDistantLight();
		sdl->lightToWorld = light2World;
		sdl->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		sdl->localLightDir = Normalize(props.Get(Property(propName + ".direction")(Vector(0.f, 0.f, 1.f))).Get<Vector>());

		lightSource = sdl;
	} else if (lightType == "distant") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		DistantLight *dl = new DistantLight();
		dl->lightToWorld = light2World;
		dl->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		dl->localLightDir = Normalize(props.Get(Property(propName + ".direction")(Vector(0.f, 0.f, 1.f))).Get<Vector>());
		dl->theta = props.Get(Property(propName + ".theta")(10.f)).Get<float>();

		lightSource = dl;
	} else
		throw runtime_error("Unknown light type: " + lightType);

	lightSource->gain = props.Get(Property(propName + ".gain")(Spectrum(1.f))).Get<Spectrum>();
	lightSource->SetSamples(props.Get(Property(propName + ".samples")(-1)).Get<int>());
	lightSource->SetID(props.Get(Property(propName + ".id")(0)).Get<int>());
	lightSource->Preprocess();
	lightSource->SetImportance(props.Get(Property(propName + ".importance")(1.f)).Get<float>());

	return lightSource;
}

//------------------------------------------------------------------------------

bool Scene::Intersect(IntersectionDevice *device,
		const bool fromLight, PathVolumeInfo *volInfo,
		const float initialPassThrough, Ray *ray, RayHit *rayHit, BSDF *bsdf,
		Spectrum *connectionThroughput, SampleResult *sampleResult,
		Spectrum *connectionEmission) const {
	*connectionThroughput = Spectrum(1.f);
	if (connectionEmission)
		*connectionEmission = Spectrum();

	float passThrough = initialPassThrough;
	const float originalMaxT = ray->maxt;

	for (;;) {
		const bool hit = device->TraceRay(ray, rayHit);

		const Volume *rayVolume = volInfo->GetCurrentVolume();
		if (hit) {
			bsdf->Init(fromLight, *this, *ray, *rayHit, passThrough, volInfo);
			rayVolume = bsdf->hitPoint.intoObject ? bsdf->hitPoint.exteriorVolume : bsdf->hitPoint.interiorVolume;
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
					sampleResult->AddEmission(rayVolume->GetVolumeLightID(), emis);
				if (connectionEmission)
					*connectionEmission += emis;
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

				bsdf->Init(fromLight, *this, *ray, *rayVolume, t, passThrough);
				volInfo->SetScatteredStart(true);

				return true;
			}
		}

		if (hit) {
			// Check if the volume priority system tells me to continue to trace the ray
			bool continueToTrace = volInfo->ContinueToTrace(*bsdf);

			// Check if it is a pass through point
			if (!continueToTrace) {
				const Spectrum transp = bsdf->GetPassThroughTransparency();
				if (!transp.Black()) {
					*connectionThroughput *= transp;
					continueToTrace = true;
				}
			}

			if (continueToTrace) {
				// Update volume information
				volInfo->Update(bsdf->GetEventTypes(), *bsdf);

				// It is a transparent material, continue to trace the ray
				ray->mint = rayHit->t + MachineEpsilon::E(rayHit->t);
				ray->maxt = originalMaxT;

				// A safety check
				if (ray->mint >= ray->maxt)
					return false;
			} else
				return true;
		} else {
			// Nothing was hit
			return false;
		}

		// I generate a new random variable starting from the previous one. I'm
		// not really sure about the kind of correlation introduced by this
		// trick.
		passThrough = fabsf(passThrough - .5f) * 2.f;
	}
}

// Just for all code not yet supporting volume rendering
bool Scene::Intersect(IntersectionDevice *device,
		const bool fromLight,
		const float passThrough, Ray *ray, RayHit *rayHit, BSDF *bsdf,
		Spectrum *connectionThroughput) const {
	*connectionThroughput = Spectrum(1.f);
	for (;;) {
		if (!device->TraceRay(ray, rayHit)) {
			// Nothing was hit
			return false;
		} else {
			// Check if it is a pass through point
			bsdf->Init(fromLight, *this, *ray, *rayHit, passThrough, NULL);

			// Mix material can have IsPassThrough() = true and return Spectrum(0.f)
			Spectrum t = bsdf->GetPassThroughTransparency();
			if (t.Black())
				return true;

			*connectionThroughput *= t;

			// It is a transparent material, continue to trace the ray
			ray->mint = rayHit->t + MachineEpsilon::E(rayHit->t);

			// A safety check
			if (ray->mint >= ray->maxt)
				return false;
		}
	}
}
