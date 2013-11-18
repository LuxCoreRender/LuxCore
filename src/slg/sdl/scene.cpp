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
#include "slg/sdl/sdl.h"
#include "slg/sampler/sampler.h"
#include "slg/sdl/scene.h"
#include "slg/editaction.h"

using namespace std;
using namespace luxrays;
using namespace slg;

Scene::Scene(const float imageScale) {
	camera = NULL;

	dataSet = NULL;
	accelType = ACCEL_AUTO;
	enableInstanceSupport = true;

	editActions.AddAllAction();
	imgMapCache.SetImageResize(imageScale);
}

Scene::Scene(const string &fileName, const float imageScale) {
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
	if (lightDefs.GetSize() == 0)
		throw runtime_error("The scene doesn't include any light source");

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

		// Save all not intersecable light sources
		for (u_int i = 0; i < lightDefs.GetSize(); ++i) {
			const LightSource *l = lightDefs.GetLightSource(i);
			if (dynamic_cast<const NotIntersecableLightSource *>(l))
				props.Set(((const NotIntersecableLightSource *)l)->ToProperties(imgMapCache));
		}

		// Write the image map information
		SDL_LOG("Saving image map information:");
		vector<const ImageMap *> ims;
		imgMapCache.GetImageMaps(ims);
		for (u_int i = 0; i < ims.size(); ++i) {
			const string fileName = directoryName + "/imagemap-" + (boost::format("%05d") % i).str() + ".exr";
			SDL_LOG("  " + fileName);
			ims[i]->WriteImage(fileName);
		}

		// Write the texture information
		SDL_LOG("Saving texture information:");
		for (u_int i = 0; i < texDefs.GetSize(); ++i) {
			const Texture *tex = texDefs.GetTexture(i);
			SDL_LOG("  " + tex->GetName());
			props.Set(tex->ToProperties(imgMapCache));
		}

		// Write the material information
		SDL_LOG("Saving material information:");
		for (u_int i = 0; i < matDefs.GetSize(); ++i) {
			const Material *mat = matDefs.GetMaterial(i);
			SDL_LOG("  " + mat->GetName());
			props.Set(mat->ToProperties());
		}

		// Write the mesh information
		SDL_LOG("Saving mesh information:");
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

		SDL_LOG("Saving object information:");
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

			// Check if the old and/or the new material were/is light sources
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

			// Check if the old and/or the new object were/is light sources
			if (wasLightSource || obj->GetMaterial()->IsLightSource())
				editActions.AddAction(LIGHTS_EDIT);
		} else {
			// Only a new object
			objDefs.DefineSceneObject(objName, obj);
			
			// Check if it is a light sources
			const Material *mat = obj->GetMaterial();
			if (mat->IsLightSource()) {
				const ExtMesh *mesh = obj->GetExtMesh();
				SDL_LOG("The " << objName << " object is a light sources with " << mesh->GetTotalTriangleCount() << " triangles");

				for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i) {
					TriangleLight *tl = new TriangleLight(mat, mesh, objDefs.GetSize() - 1, i);
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
		// Extract the material name
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
}

void Scene::RemoveUnusedImageMaps() {
	// Build a list of all referenced image maps
	boost::unordered_set<const ImageMap *> referencedImgMaps;
	for (u_int i = 0; i < texDefs.GetSize(); ++i)
		texDefs.GetTexture(i)->AddReferencedImageMaps(referencedImgMaps);

	// Add the infinite light image
	BOOST_FOREACH(EnvLightSource *l, lightDefs.GetEnvLightSources())
		l->AddReferencedImageMaps(referencedImgMaps);

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

	// Get the list of all defined material
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
	for (u_int i = 0; i < objDefs.GetSize(); ++i)
		objDefs.GetSceneObject(i)->AddReferencedMaterials(referencedMats);

	// Get the list of all defined material
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
	// Build a list of all referenced mesh
	boost::unordered_set<const ExtMesh *> referencedMesh;
	for (u_int i = 0; i < objDefs.GetSize(); ++i)
		objDefs.GetSceneObject(i)->AddReferencedMeshes(referencedMesh);

	// Get the list of all defined material
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
		const float gain = props.Get(Property(propName + ".gain")(1.0f)).Get<float>();

		ImageMap *im = imgMapCache.GetImageMap(name, gamma);
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
	} else if (texType == "wood") {
		const std::string woodtype = props.Get(Property(propName + ".woodtype")("bands")).Get<string>();
		const std::string noisebasis2 = props.Get(Property(propName + ".noisebasis2")("sin")).Get<string>();
		const std::string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		return new WoodTexture(CreateTextureMapping3D(propName + ".mapping", props), woodtype, noisebasis2, noisesize, turbulence, (hard=="hard_noise"), bright, contrast);
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

Material *Scene::CreateMaterial(const u_int defaultMatID, const string &matName, const Properties &props) {
	const string propName = "scene.materials." + matName;
	const string matType = props.Get(Property(propName + ".type")("matte")).Get<string>();

	Texture *emissionTex = props.IsDefined(propName + ".emission") ? 
		GetTexture(props.Get(Property(propName + ".emission")(0.f, 0.f, 0.f))) : NULL;
	// Required to remove light source while editing the scene
	if (emissionTex && (
			((emissionTex->GetType() == CONST_FLOAT) && (((ConstFloatTexture *)emissionTex)->GetValue() == 0.f)) ||
			((emissionTex->GetType() == CONST_FLOAT3) && (((ConstFloat3Texture *)emissionTex)->GetColor().Black()))))
		emissionTex = NULL;

	Texture *bumpTex = props.IsDefined(propName + ".bumptex") ? 
		GetTexture(props.Get(Property(propName + ".bumptex")(1.f))) : NULL;
	Texture *normalTex =props.IsDefined(propName + ".normaltex") ? 
		GetTexture(props.Get(Property(propName + ".normaltex")(1.f))) : NULL;

	Material *mat;
	if (matType == "matte") {
		Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(.75f, .75f, .75f)));

		mat = new MatteMaterial(emissionTex, bumpTex, normalTex, kd);
	} else if (matType == "mirror") {
		Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));

		mat = new MirrorMaterial(emissionTex, bumpTex, normalTex, kr);
	} else if (matType == "glass") {
		Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));
		Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(1.f, 1.f, 1.f)));
		Texture *ioroutside = GetTexture(props.Get(Property(propName + ".ioroutside")(1.f)));
		Texture *iorinside = GetTexture(props.Get(Property(propName + ".iorinside")(1.5f)));

		mat = new GlassMaterial(emissionTex, bumpTex, normalTex, kr, kt, ioroutside, iorinside);
	} else if (matType == "metal") {
		Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));
		Texture *exp = GetTexture(props.Get(Property(propName + ".exp")(10.f)));

		mat = new MetalMaterial(emissionTex, bumpTex, normalTex, kr, exp);
	} else if (matType == "archglass") {
		Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));
		Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(1.f, 1.f, 1.f)));
		Texture *ioroutside = GetTexture(props.Get(Property(propName + ".ioroutside")(1.f)));
		Texture *iorinside = GetTexture(props.Get(Property(propName + ".iorinside")(1.f)));

		mat = new ArchGlassMaterial(emissionTex, bumpTex, normalTex, kr, kt, ioroutside, iorinside);
	} else if (matType == "mix") {
		Material *matA = matDefs.GetMaterial(props.Get(Property(propName + ".material1")("mat1")).Get<string>());
		Material *matB = matDefs.GetMaterial(props.Get(Property(propName + ".material2")("mat2")).Get<string>());
		Texture *mix = GetTexture(props.Get(Property(propName + ".amount")(.5f)));

		MixMaterial *mixMat = new MixMaterial(bumpTex, normalTex, matA, matB, mix);

		// Check if there is a loop in Mix material definition
		// (Note: this can not really happen at the moment because forward
		// declarations are not supported)
		if (mixMat->IsReferencing(mixMat))
			throw runtime_error("There is a loop in Mix material definition: " + matName);

		mat = mixMat;
	} else if (matType == "null") {
		mat = new NullMaterial();
	} else if (matType == "mattetranslucent") {
		Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(.5f, .5f, .5f)));
		Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(.5f, .5f, .5f)));

		mat = new MatteTranslucentMaterial(emissionTex, bumpTex, normalTex, kr, kt);
	} else if (matType == "glossy2") {
		Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(.5f, .5f, .5f)));
		Texture *ks = GetTexture(props.Get(Property(propName + ".ks")(.5f, .5f, .5f)));
		Texture *nu = GetTexture(props.Get(Property(propName + ".uroughness")(.1f)));
		Texture *nv = GetTexture(props.Get(Property(propName + ".vroughness")(.1f)));
		Texture *ka = GetTexture(props.Get(Property(propName + ".ka")(0.f)));
		Texture *d = GetTexture(props.Get(Property(propName + ".d")(0.f)));
		Texture *index = GetTexture(props.Get(Property(propName + ".index")(0.f)));
		const bool multibounce = props.Get(Property(propName + ".multibounce")(false)).Get<bool>();

		mat = new Glossy2Material(emissionTex, bumpTex, normalTex, kd, ks, nu, nv, ka, d, index, multibounce);
	} else if (matType == "metal2") {
		Texture *nu = GetTexture(props.Get(Property(propName + ".uroughness")(.1f)));
		Texture *nv = GetTexture(props.Get(Property(propName + ".vroughness")(.1f)));

		Texture *eta, *k;
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

		mat = new Metal2Material(emissionTex, bumpTex, normalTex, eta, k, nu, nv);
	} else if (matType == "roughglass") {
		Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));
		Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(1.f, 1.f, 1.f)));
		Texture *ioroutside = GetTexture(props.Get(Property(propName + ".ioroutside")(1.f)));
		Texture *iorinside = GetTexture(props.Get(Property(propName + ".iorinside")(1.5f)));
		Texture *nu = GetTexture(props.Get(Property(propName + ".uroughness")(.1f)));
		Texture *nv = GetTexture(props.Get(Property(propName + ".vroughness")(.1f)));

		mat = new RoughGlassMaterial(emissionTex, bumpTex, normalTex, kr, kt, ioroutside, iorinside, nu, nv);
	} else
		throw runtime_error("Unknown material type: " + matType);

	mat->SetSamples(Max(-1, props.Get(Property(propName + ".samples")(-1)).Get<int>()));
	mat->SetID(props.Get(Property(propName + ".id")(defaultMatID)).Get<u_int>());

	mat->SetEmittedGain(Max(0.f, props.Get(Property(propName + ".emission.gain")(Spectrum(1.f))).Get<Spectrum>()));
	mat->SetEmittedPower(Max(0.f, props.Get(Property(propName + ".emission.power")(0.f)).Get<float>()));
	mat->SetEmittedEfficency(Max(0.f, props.Get(Property(propName + ".emission.efficency")(0.f)).Get<float>()));
	mat->SetEmittedSamples(Max(-1, props.Get(Property(propName + ".emission.samples")(-1)).Get<int>()));
	mat->SetLightID(props.Get(Property(propName + ".emission.id")(0u)).Get<u_int>());

	mat->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
	mat->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
	mat->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

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

		// Check if I have to use an instance mesh or not
		if (props.IsDefined(propName + ".transformation")) {
			const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
			const Transform trans(mat);

			mesh = extMeshCache.GetExtMesh(plyFileName, &trans);
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

		if (props.IsDefined(propName + ".transformation")) {
			const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
			const Transform trans(mat);

			// Transform directly the vertices. This mesh is defined on-the-fly
			// and can be used only once
			for (u_int i = 0; i < pointsSize; ++i)
				points[i] = trans * points[i];
		}

		extMeshCache.DefineExtMesh("InlinedMesh-" + objName, pointsSize, trisSize,
				points, tris,
				NULL, NULL, NULL, NULL);
		mesh = extMeshCache.GetExtMesh("InlinedMesh-" + objName);
	}

	// Get the material
	if (!matDefs.IsMaterialDefined(matName))
		throw runtime_error("Unknown material: " + matName);
	const Material *mat = matDefs.GetMaterial(matName);

	return new SceneObject(mesh, mat);
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

	NotIntersecableLightSource *lightSource = NULL;
	if (lightType == "sky") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SkyLight *sl = new SkyLight(light2World,
				props.Get(Property(propName + ".turbidity")(2.2f)).Get<float>(),
				props.Get(Property(propName + ".dir")(0.f, 0.f, 1.f)).Get<Vector>());

		lightSource = sl;
	} else if (lightType == "infinite") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		const string imageName = props.Get(Property(propName + ".file")("image.png")).Get<string>();
		const float gamma = props.Get(Property(propName + ".gamma")(2.2f)).Get<float>();
		ImageMap *imgMap = imgMapCache.GetImageMap(imageName, gamma);
		InfiniteLight *il = new InfiniteLight(light2World, imgMap);

		const UV shift = props.Get(Property(propName + ".shift")(0.f, 0.f)).Get<UV>();
		il->GetUVMapping()->uDelta = shift.u;
		il->GetUVMapping()->vDelta = shift.v;

		lightSource = il;
	} else if (lightType == "sun") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SunLight *sl = new SunLight(light2World,
				props.Get(Property(propName + ".turbidity")(2.2f)).Get<float>(),
				props.Get(Property(propName + ".relsize")(1.0f)).Get<float>(),
				props.Get(Property(propName + ".dir")(0.f, 0.f, 1.f)).Get<Vector>());

		sl->Preprocess();

		lightSource = sl;
	} else
		throw runtime_error("Unknown light type: " + lightType);

	lightSource->SetGain(props.Get(Property(propName + ".gain")(Spectrum(1.f))).Get<Spectrum>());
	lightSource->SetSamples(props.Get(Property(propName + ".samples")(-1)).Get<int>());
	lightSource->SetID(props.Get(Property(propName + ".id")(0)).Get<int>());
	lightSource->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
	lightSource->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
	lightSource->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());
	lightSource->Preprocess();
	
	return lightSource;
}

//------------------------------------------------------------------------------

bool Scene::Intersect(IntersectionDevice *device, const bool fromLight,
		const float passThrough, Ray *ray, RayHit *rayHit, BSDF *bsdf,
		Spectrum *connectionThroughput) const {
	*connectionThroughput = Spectrum(1.f);
	for (;;) {
		if (!device->TraceRay(ray, rayHit)) {
			// Nothing was hit
			return false;
		} else {
			// Check if it is a pass through point
			bsdf->Init(fromLight, *this, *ray, *rayHit, passThrough);

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
