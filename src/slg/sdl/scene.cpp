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

Scene::Scene() {
	camera = NULL;

	envLight = NULL;
	sunLight = NULL;

	dataSet = NULL;
	accelType = ACCEL_AUTO;
	enableInstanceSupport = true;
	lightsDistribution = NULL;

	lightGroupCount = 1;
}

Scene::Scene(const string &fileName, const float imageScale) {
	// Just in case there is an unexpected exception during the scene loading
    camera = NULL;
	envLight = NULL;
	sunLight = NULL;
	dataSet = NULL;
	lightsDistribution = NULL;

	lightGroupCount = 1;

	imgMapCache.SetImageResize(imageScale);

	SDL_LOG("Reading scene: " << fileName);

	Properties scnProp(fileName);
	Parse(scnProp);

	//--------------------------------------------------------------------------

	if (!envLight && !sunLight && (triLightDefs.size() == 0))
		throw runtime_error("The scene doesn't include any light source");

	UpdateLightGroupCount();

	dataSet = NULL;
	accelType = ACCEL_AUTO;
	enableInstanceSupport = true;
}

Scene::~Scene() {
	delete camera;
	delete envLight;
	delete sunLight;

	for (vector<TriangleLight *>::const_iterator l = triLightDefs.begin(); l != triLightDefs.end(); ++l)
		delete *l;

	delete dataSet;
	delete lightsDistribution;
}

void  Scene::UpdateLightGroupCount() {
	// Update the count of light groups
	if (envLight)
		lightGroupCount = Max(lightGroupCount, envLight->GetID() + 1);
	if (sunLight)
		lightGroupCount = Max(lightGroupCount, sunLight->GetID() + 1);
	BOOST_FOREACH(TriangleLight *tl, triLightDefs) {
		lightGroupCount = Max(lightGroupCount, tl->GetID() + 1);
	}
}

void Scene::Preprocess(Context *ctx) {
	// Rebuild the data set
	delete dataSet;
	dataSet = new DataSet(ctx);
	dataSet->SetInstanceSupport(enableInstanceSupport);
	dataSet->SetAcceleratorType(accelType);

	// Add all objects
	for (u_int i = 0; i < objDefs.GetSize(); ++i)
		dataSet->Add(objDefs.GetSceneObject(i)->GetExtMesh());

	dataSet->Preprocess();

	// Update the count of light groups
	UpdateLightGroupCount();

	// Rebuild the data to power based light sampling
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * dataSet->GetBSphere().rad * 1.01f;
	const float iWorldRadius2 = 1.f / (worldRadius * worldRadius);
	const u_int lightCount = GetLightCount();
	float *lightPower = new float[lightCount];
	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = GetLightByIndex(i);
		lightPower[i] = l->GetPower(*this);

		// In order to avoid over-sampling of distant lights
		if ((l->GetType() == TYPE_IL) ||
				(l->GetType() == TYPE_IL_SKY) ||
				(l->GetType() == TYPE_SUN))
			lightPower[i] *= iWorldRadius2;
	}

	lightsDistribution = new Distribution1D(lightPower, lightCount);
	delete lightPower;

	// Initialize the light source indices
	for (u_int i = 0; i < lightCount; ++i)
		GetLightByIndex(i)->SetSceneIndex(i);

	editActions.Reset();
}

Properties Scene::ToProperties(const string &directoryName) {
		Properties props;

		// Write the camera information
		SDL_LOG("Saving camera information");
		props.Set(camera->ToProperties());

		if (envLight) {
			// Write the infinitelight/skylight information
			SDL_LOG("Saving infinitelight/skylight information");
			props.Set(envLight->ToProperties(imgMapCache));
		}
		
		if (sunLight) {
			// Write the sunlight information
			SDL_LOG("Saving sunlight information");
			props.Set(sunLight->ToProperties());
		}

		// Write the image map information
		SDL_LOG("Saving image map information:");
		vector<ImageMap *> ims;
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
// Methods to build a scene from scratch
//--------------------------------------------------------------------------

void Scene::RebuildTriangleLightDefs() {
	// I have to build a new version of lights and triangleLightSource
	vector<TriangleLight *> newTriLights;
	vector<u_int> newMeshTriLightOffset;

	for (u_int i = 0; i < objDefs.GetSize(); ++i) {
		const SceneObject *obj = objDefs.GetSceneObject(i);
		const ExtMesh *mesh = obj->GetExtMesh();
		const Material *m = obj->GetMaterial();

		if (m->IsLightSource()) {
			newMeshTriLightOffset.push_back(newTriLights.size());

			for (u_int j = 0; j < mesh->GetTotalTriangleCount(); ++j) {
				TriangleLight *tl = new TriangleLight(m, mesh, i, j);
				newTriLights.push_back(tl);
			}
		} else
			newMeshTriLightOffset.push_back(NULL_INDEX);
	}

	// Delete all old TriangleLight
	for (vector<TriangleLight *>::const_iterator l = triLightDefs.begin(); l != triLightDefs.end(); ++l)
		delete *l;

	// Use the new versions
	triLightDefs = newTriLights;
	meshTriLightDefsOffset = newMeshTriLightOffset;
}

void Scene::Parse(const Properties &props) {
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

	ParseEnvLights(props);
}

void Scene::ParseCamera(const Properties &props) {
	if (!props.HaveNames("scene.camera.lookat")) {
		// There is no camera definition
		return;
	}

	Point orig, target;
	if (props.IsDefined("scene.camera.lookat")) {
		SDL_LOG("WARNING: deprecated property scene.camera.lookat");

		const Property &prop = props.Get("scene.camera.lookat");
		orig.x = prop.GetValue<float>(0);
		orig.y = prop.GetValue<float>(1);
		orig.z = prop.GetValue<float>(2);
		target.x = prop.GetValue<float>(3);
		target.y = prop.GetValue<float>(4);
		target.z = prop.GetValue<float>(5);
	} else {
		orig = props.Get("scene.camera.lookat.orig").Get<Point>();
		target = props.Get("scene.camera.lookat.target").Get<Point>();
	}

	SDL_LOG("Camera position: " << orig);
	SDL_LOG("Camera target: " << target);

	const Vector up = props.Get("scene.camera.up", MakePropertyValues(0.f, 0.f, .1f)).Get<Vector>();

	auto_ptr<PerspectiveCamera> newCamera;
	if (props.IsDefined("scene.camera.screenwindow")) {
		float screenWindow[4];

		const Property &prop = props.Get("scene.camera.screenwindow", MakePropertyValues(0.f, 1.f, 0.f, 1.f));
		screenWindow[0] = prop.GetValue<float>(0);
		screenWindow[1] = prop.GetValue<float>(1);
		screenWindow[2] = prop.GetValue<float>(2);
		screenWindow[3] = prop.GetValue<float>(3);

		newCamera.reset(new PerspectiveCamera(orig, target, up, &screenWindow[0]));
	} else
		newCamera.reset(new PerspectiveCamera(orig, target, up));

	newCamera->clipHither = props.Get("scene.camera.cliphither", MakePropertyValues(1e-3f)).Get<float>();
	newCamera->clipYon = props.Get("scene.camera.clipyon", MakePropertyValues(1e30f)).Get<float>();
	newCamera->lensRadius = props.Get("scene.camera.lensradius", MakePropertyValues(0.f)).Get<float>();
	newCamera->focalDistance = props.Get("scene.camera.focaldistance", MakePropertyValues(10.f)).Get<float>();
	newCamera->fieldOfView = props.Get("scene.camera.fieldofview", MakePropertyValues(45.f)).Get<float>();

	if (props.Get("scene.camera.horizontalstereo.enable", MakePropertyValues(false)).Get<bool>()) {
		SDL_LOG("Camera horizontal stereo enabled");
		newCamera->SetHorizontalStereo(true);

		const float eyesDistance = props.Get("scene.camera.horizontalstereo.eyesdistance", MakePropertyValues(.0626f)).Get<float>();
		SDL_LOG("Camera horizontal stereo eyes distance: " << eyesDistance);
		newCamera->SetHorizontalStereoEyesDistance(eyesDistance);
		const float lesnDistance = props.Get("scene.camera.horizontalstereo.lensdistance", MakePropertyValues(.1f)).Get<float>();
		SDL_LOG("Camera horizontal stereo lens distance: " << lesnDistance);
		newCamera->SetHorizontalStereoLensDistance(lesnDistance);

		// Check if I have to enable Oculus Rift Barrel post-processing
		if (props.Get("scene.camera.horizontalstereo.oculusrift.barrelpostpro.enable", MakePropertyValues(false)).Get<bool>()) {
			SDL_LOG("Camera Oculus Rift Barrel post-processing enabled");
			newCamera->SetOculusRiftBarrel(true);
		} else {
			SDL_LOG("Camera Oculus Rift Barrel post-processing disabled");
			newCamera->SetOculusRiftBarrel(false);
		}
	} else {
		SDL_LOG("Camera horizontal stereo disabled");
		newCamera->SetHorizontalStereo(false);
	}

	// Use the new camera
	delete camera;
	camera = newCamera.get();
	newCamera.release();

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

			// Check if the old and/or the new material were/is light sources
			if (wasLightSource || newMat->IsLightSource()) {
				RebuildTriangleLightDefs();

				editActions.AddAction(AREALIGHTS_EDIT);
			}
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
			if (wasLightSource || obj->GetMaterial()->IsLightSource()) {
				RebuildTriangleLightDefs();

				editActions.AddAction(AREALIGHTS_EDIT);
			}
		} else {
			// Only a new object
			objDefs.DefineSceneObject(objName, obj);
			
			// Check if it is a light sources
			const Material *mat = obj->GetMaterial();
			if (mat->IsLightSource()) {
				const ExtMesh *mesh = obj->GetExtMesh();
				SDL_LOG("The " << objName << " object is a light sources with " << mesh->GetTotalTriangleCount() << " triangles");

				meshTriLightDefsOffset.push_back(triLightDefs.size());
				for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i) {
					TriangleLight *tl = new TriangleLight(mat, mesh, objDefs.GetSize() - 1, i);
					triLightDefs.push_back(tl);
				}
			} else
				meshTriLightDefsOffset.push_back(NULL_INDEX);
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

void Scene::ParseEnvLights(const Properties &props) {
	//--------------------------------------------------------------------------
	// SkyLight
	//--------------------------------------------------------------------------

	if (props.HaveNames("scene.infinitelight")) {
		const Matrix4x4 mat = props.Get("scene.skylight.transformation", MakePropertyValues(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SkyLight *sl = new SkyLight(light2World,
				props.Get("scene.skylight.turbidity", MakePropertyValues(2.2f)).Get<float>(),
				props.Get("scene.skylight.dir", MakePropertyValues(0.f, 0.f, 1.f)).Get<Vector>());
		sl->SetGain(props.Get("scene.skylight.gain", MakePropertyValues(1.f, 1.f, 1.f)).Get<Spectrum>());
		sl->SetSamples(props.Get("scene.skylight.samples", MakePropertyValues(-1)).Get<int>());
		sl->SetID(props.Get("scene.skylight.id", MakePropertyValues(0)).Get<int>());
		sl->SetIndirectDiffuseVisibility(props.Get("scene.skylight.visibility.indirect.diffuse.enable", MakePropertyValues(true)).Get<bool>());
		sl->SetIndirectGlossyVisibility(props.Get("scene.skylight.visibility.indirect.glossy.enable", MakePropertyValues(true)).Get<bool>());
		sl->SetIndirectSpecularVisibility(props.Get("scene.skylight.visibility.indirect.specular.enable", MakePropertyValues(true)).Get<bool>());
		sl->Preprocess();

		// Delete the old env. light
		if (envLight)
			delete envLight;
		envLight = sl;
	}

	//--------------------------------------------------------------------------
	// InfiniteLight
	//--------------------------------------------------------------------------

	if (props.HaveNames("scene.infinitelight")) {
		const Matrix4x4 mat = props.Get("scene.infinitelight.transformation", MakePropertyValues(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		const string imageName = props.Get("scene.infinitelight.file", MakePropertyValues("image.png")).Get<string>();
		const float gamma = props.Get("scene.infinitelight.gamma", MakePropertyValues(2.2f)).Get<float>();
		ImageMap *imgMap = imgMapCache.GetImageMap(imageName, gamma);
		InfiniteLight *il = new InfiniteLight(light2World, imgMap);

		il->SetGain(props.Get("scene.infinitelight.gain", MakePropertyValues(1.f, 1.f, 1.f)).Get<Spectrum>());

		const UV shift = props.Get("scene.infinitelight.shift", MakePropertyValues(0.f, 0.f)).Get<UV>();
		il->GetUVMapping()->uDelta = shift.u;
		il->GetUVMapping()->vDelta = shift.v;
		il->SetSamples(props.Get("scene.infinitelight.samples", MakePropertyValues(-1)).Get<int>());
		il->SetID(props.Get("scene.infinitelight.id", MakePropertyValues(0)).Get<int>());
		il->SetIndirectDiffuseVisibility(props.Get("scene.infinitelight.visibility.indirect.diffuse.enable", MakePropertyValues(true)).Get<bool>());
		il->SetIndirectGlossyVisibility(props.Get("scene.infinitelight.visibility.indirect.glossy.enable", MakePropertyValues(true)).Get<bool>());
		il->SetIndirectSpecularVisibility(props.Get("scene.infinitelight.visibility.indirect.specular.enable", MakePropertyValues(true)).Get<bool>());
		il->Preprocess();

		// Delete the old env. light
		if (envLight)
			delete envLight;
		envLight = il;
	}

	//--------------------------------------------------------------------------
	// SunLight
	//--------------------------------------------------------------------------

	if (props.HaveNames("scene.sunlight")) {
		const Matrix4x4 mat = props.Get("scene.sunlight.transformation", MakePropertyValues(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SunLight *sl = new SunLight(light2World,
				props.Get("scene.sunlight.turbidity", MakePropertyValues(2.2f)).Get<float>(),
				props.Get("scene.sunlight.relsize", MakePropertyValues(1.0f)).Get<float>(),
				props.Get("scene.sunlight.dir", MakePropertyValues(0.f, 0.f, 1.f)).Get<Vector>());

		sl->SetGain(props.Get("scene.sunlight.gain", MakePropertyValues(1.f, 1.f, 1.f)).Get<Spectrum>());
		sl->SetSamples(props.Get("scene.sunlight.samples", MakePropertyValues(-1)).Get<int>());
		sl->SetID(props.Get("scene.sunlight.id", MakePropertyValues(0)).Get<int>());
		sl->SetIndirectDiffuseVisibility(props.Get("scene.sunlight.visibility.indirect.diffuse.enable", MakePropertyValues(true)).Get<bool>());
		sl->SetIndirectGlossyVisibility(props.Get("scene.sunlight.visibility.indirect.glossy.enable", MakePropertyValues(true)).Get<bool>());
		sl->SetIndirectSpecularVisibility(props.Get("scene.sunlight.visibility.indirect.specular.enable", MakePropertyValues(true)).Get<bool>());
		sl->Preprocess();

		// Delete the old sun light
		if (sunLight)
			delete sunLight;
		sunLight = sl;
	}
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
		const u_int meshIndex = objDefs.GetSceneObjectIndex(objName);
		for (u_int i = meshTriLightDefsOffset[meshIndex]; i < mesh->GetTotalTriangleCount(); ++i)
			triLightDefs[i]->Init();
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
		}
	}
}

//------------------------------------------------------------------------------

TextureMapping2D *Scene::CreateTextureMapping2D(const string &prefixName, const Properties &props) {
	const string mapType = props.Get(prefixName + ".type", MakePropertyValues("uvmapping2d")).Get<string>();

	if (mapType == "uvmapping2d") {
		const UV uvScale = props.Get(prefixName + ".uvscale", MakePropertyValues(1.f, 1.f)).Get<UV>();
		const UV uvDelta = props.Get(prefixName + ".uvdelta", MakePropertyValues(0.f, 0.f)).Get<UV>();

		return new UVMapping2D(uvScale.u, uvScale.v, uvDelta.u, uvDelta.v);
	} else
		throw runtime_error("Unknown 2D texture coordinate mapping type: " + mapType);
}

TextureMapping3D *Scene::CreateTextureMapping3D(const string &prefixName, const Properties &props) {
	const string mapType = props.Get(prefixName + ".type", MakePropertyValues("uvmapping3d")).Get<string>();

	if (mapType == "uvmapping3d") {
		PropertyValues matIdentity(16);
		for (u_int i = 0; i < 4; ++i) {
			for (u_int j = 0; j < 4; ++j) {
				matIdentity[i * 4 + j] = (i == j) ? 1.f : 0.f;
			}
		}

		const Matrix4x4 mat = props.Get(prefixName + ".transformation", MakePropertyValues(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		return new UVMapping3D(trans);
	} else if (mapType == "globalmapping3d") {
		PropertyValues matIdentity(16);
		for (u_int i = 0; i < 4; ++i) {
			for (u_int j = 0; j < 4; ++j) {
				matIdentity[i * 4 + j] = (i == j) ? 1.f : 0.f;
			}
		}

		const Matrix4x4 mat = props.Get(prefixName + ".transformation", MakePropertyValues(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		return new GlobalMapping3D(trans);
	} else
		throw runtime_error("Unknown 3D texture coordinate mapping type: " + mapType);
}

Texture *Scene::CreateTexture(const string &texName, const Properties &props) {
	const string propName = "scene.textures." + texName;
	const string texType = props.Get(propName + ".type", MakePropertyValues("imagemap")).Get<string>();

	if (texType == "imagemap") {
		const string name = props.Get(propName + ".file", MakePropertyValues("image.png")).Get<string>();
		const float gamma = props.Get(propName + ".gamma", MakePropertyValues(2.2f)).Get<float>();
		const float gain = props.Get(propName + ".gain", MakePropertyValues(1.0f)).Get<float>();

		ImageMap *im = imgMapCache.GetImageMap(name, gamma);
		return new ImageMapTexture(im, CreateTextureMapping2D(propName + ".mapping", props), gain);
	} else if (texType == "constfloat1") {
		const float v = props.Get(propName + ".value", MakePropertyValues(1.f)).Get<float>();
		return new ConstFloatTexture(v);
	} else if (texType == "constfloat3") {
		const Spectrum v = props.Get(propName + ".value", MakePropertyValues(1.f)).Get<Spectrum>();
		return new ConstFloat3Texture(v);
	} else if (texType == "scale") {
		const Texture *tex1 = GetTexture(props.Get(propName + ".texture1", MakePropertyValues(1.f)));
		const Texture *tex2 = GetTexture(props.Get(propName + ".texture2", MakePropertyValues(1.f)));
		return new ScaleTexture(tex1, tex2);
	} else if (texType == "fresnelapproxn") {
		const Texture *tex = GetTexture(props.Get(propName + ".texture", MakePropertyValues(.5f, .5f, .5f)));
		return new FresnelApproxNTexture(tex);
	} else if (texType == "fresnelapproxk") {
		const Texture *tex = GetTexture(props.Get(propName + ".texture", MakePropertyValues(.5f, .5f, .5f)));
		return new FresnelApproxKTexture(tex);
	} else if (texType == "checkerboard2d") {
		const Texture *tex1 = GetTexture(props.Get(propName + ".texture1", MakePropertyValues(1.f)));
		const Texture *tex2 = GetTexture(props.Get(propName + ".texture2", MakePropertyValues(0.f)));

		return new CheckerBoard2DTexture(CreateTextureMapping2D(propName + ".mapping", props), tex1, tex2);
	} else if (texType == "checkerboard3d") {
		const Texture *tex1 = GetTexture(props.Get(propName + ".texture1", MakePropertyValues(1.f)));
		const Texture *tex2 = GetTexture(props.Get(propName + ".texture2", MakePropertyValues(0.f)));

		return new CheckerBoard3DTexture(CreateTextureMapping3D(propName + ".mapping", props), tex1, tex2);
	} else if (texType == "mix") {
		const Texture *amtTex = GetTexture(props.Get(propName + ".amount", MakePropertyValues(.5f)));
		const Texture *tex1 = GetTexture(props.Get(propName + ".texture1", MakePropertyValues(0.f)));
		const Texture *tex2 = GetTexture(props.Get(propName + ".texture2", MakePropertyValues(1.f)));

		return new MixTexture(amtTex, tex1, tex2);
	} else if (texType == "fbm") {
		const int octaves = props.Get(propName + ".octaves", MakePropertyValues(8)).Get<int>();
		const float omega = props.Get(propName + ".roughness", MakePropertyValues(.5f)).Get<float>();

		return new FBMTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega);
	} else if (texType == "marble") {
		const int octaves = props.Get(propName + ".octaves", MakePropertyValues(8)).Get<int>();
		const float omega = props.Get(propName + ".roughness", MakePropertyValues(.5f)).Get<float>();
		const float scale = props.Get(propName + ".scale", MakePropertyValues(1.f)).Get<float>();
		const float variation = props.Get(propName + ".variation", MakePropertyValues(.2f)).Get<float>();

		return new MarbleTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega, scale, variation);
	} else if (texType == "dots") {
		const Texture *insideTex = GetTexture(props.Get(propName + ".inside", MakePropertyValues(1.f)));
		const Texture *outsideTex = GetTexture(props.Get(propName + ".outside", MakePropertyValues(0.f)));

		return new DotsTexture(CreateTextureMapping2D(propName + ".mapping", props), insideTex, outsideTex);
	} else if (texType == "brick") {
		const Texture *tex1 = GetTexture(props.Get(propName + ".bricktex", MakePropertyValues(1.f, 1.f, 1.f)));
		const Texture *tex2 = GetTexture(props.Get(propName + ".mortartex", MakePropertyValues(.2f, .2f, .2f)));
		const Texture *tex3 = GetTexture(props.Get(propName + ".brickmodtex", MakePropertyValues(1.f, 1.f, 1.f)));

		const string brickbond = props.Get(propName + ".brickbond", MakePropertyValues("running")).Get<string>();
		const float brickwidth = props.Get(propName + ".brickwidth", MakePropertyValues(.3f)).Get<float>();
		const float brickheight = props.Get(propName + ".brickheight", MakePropertyValues(.1f)).Get<float>();
		const float brickdepth = props.Get(propName + ".brickdepth", MakePropertyValues(.15f)).Get<float>();
		const float mortarsize = props.Get(propName + ".mortarsize", MakePropertyValues(.01f)).Get<float>();
		const float brickrun = props.Get(propName + ".brickrun", MakePropertyValues(.75f)).Get<float>();
		const float brickbevel = props.Get(propName + ".brickbevel", MakePropertyValues(0.f)).Get<float>();

		return new BrickTexture(CreateTextureMapping3D(propName + ".mapping", props), tex1, tex2, tex3,
				brickwidth, brickheight, brickdepth, mortarsize, brickrun, brickbevel, brickbond);
	} else if (texType == "add") {
		const Texture *tex1 = GetTexture(props.Get(propName + ".texture1", MakePropertyValues(1.f)));
		const Texture *tex2 = GetTexture(props.Get(propName + ".texture2", MakePropertyValues(1.f)));
		return new AddTexture(tex1, tex2);
	} else if (texType == "windy") {
		return new WindyTexture(CreateTextureMapping3D(propName + ".mapping", props));
	} else if (texType == "wrinkled") {
		const int octaves = props.Get(propName + ".octaves", MakePropertyValues(8)).Get<int>();
		const float omega = props.Get(propName + ".roughness", MakePropertyValues(.5f)).Get<float>();

		return new WrinkledTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega);
	} else if (texType == "uv") {
		return new UVTexture(CreateTextureMapping2D(propName + ".mapping", props));
	} else if (texType == "band") {
		const Texture *amtTex = GetTexture(props.Get(propName + ".amount", MakePropertyValues(.5f)));

		vector<float> offsets;
		vector<Spectrum> values;
		for (u_int i = 0; props.IsDefined(propName + ".offset" + ToString(i)); ++i) {
			const float offset = props.Get(propName + ".offset" + ToString(i), MakePropertyValues(0.f)).Get<float>();
			const Spectrum value = props.Get(propName + ".value" + ToString(i), MakePropertyValues(1.f, 1.f, 1.f)).Get<Spectrum>();

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
		const int channel = props.Get(propName + ".channel", MakePropertyValues(-1)).Get<int>();

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
	const string matType = props.Get(propName + ".type", MakePropertyValues("matte")).Get<string>();

	Texture *emissionTex = props.IsDefined(propName + ".emission") ? 
		GetTexture(props.Get(propName + ".emission", MakePropertyValues(0.f, 0.f, 0.f))) : NULL;
	// Required to remove light source while editing the scene
	if (emissionTex && (
			((emissionTex->GetType() == CONST_FLOAT) && (((ConstFloatTexture *)emissionTex)->GetValue() == 0.f)) ||
			((emissionTex->GetType() == CONST_FLOAT3) && (((ConstFloat3Texture *)emissionTex)->GetColor().Black()))))
		emissionTex = NULL;

	Texture *bumpTex = props.IsDefined(propName + ".bumptex") ? 
		GetTexture(props.Get(propName + ".bumptex", MakePropertyValues(1.f))) : NULL;
	Texture *normalTex =props.IsDefined(propName + ".normaltex") ? 
		GetTexture(props.Get(propName + ".normaltex", MakePropertyValues(1.f))) : NULL;

	Material *mat;
	if (matType == "matte") {
		Texture *kd = GetTexture(props.Get(propName + ".kd", MakePropertyValues(.75f, .75f, .75f)));

		mat = new MatteMaterial(emissionTex, bumpTex, normalTex, kd);
	} else if (matType == "mirror") {
		Texture *kr = GetTexture(props.Get(propName + ".kr", MakePropertyValues(1.f, 1.f, 1.f)));

		mat = new MirrorMaterial(emissionTex, bumpTex, normalTex, kr);
	} else if (matType == "glass") {
		Texture *kr = GetTexture(props.Get(propName + ".kr", MakePropertyValues(1.f, 1.f, 1.f)));
		Texture *kt = GetTexture(props.Get(propName + ".kt", MakePropertyValues(1.f, 1.f, 1.f)));
		Texture *ioroutside = GetTexture(props.Get(propName + ".ioroutside", MakePropertyValues(1.f)));
		Texture *iorinside = GetTexture(props.Get(propName + ".iorinside", MakePropertyValues(1.5f)));

		mat = new GlassMaterial(emissionTex, bumpTex, normalTex, kr, kt, ioroutside, iorinside);
	} else if (matType == "metal") {
		Texture *kr = GetTexture(props.Get(propName + ".kr", MakePropertyValues(1.f, 1.f, 1.f)));
		Texture *exp = GetTexture(props.Get(propName + ".exp", MakePropertyValues(10.f)));

		mat = new MetalMaterial(emissionTex, bumpTex, normalTex, kr, exp);
	} else if (matType == "archglass") {
		Texture *kr = GetTexture(props.Get(propName + ".kr", MakePropertyValues(1.f, 1.f, 1.f)));
		Texture *kt = GetTexture(props.Get(propName + ".kt", MakePropertyValues(1.f, 1.f, 1.f)));
		Texture *ioroutside = GetTexture(props.Get(propName + ".ioroutside", MakePropertyValues(1.f)));
		Texture *iorinside = GetTexture(props.Get(propName + ".iorinside", MakePropertyValues(1.f)));

		mat = new ArchGlassMaterial(emissionTex, bumpTex, normalTex, kr, kt, ioroutside, iorinside);
	} else if (matType == "mix") {
		Material *matA = matDefs.GetMaterial(props.Get(propName + ".material1", MakePropertyValues("mat1")).Get<string>());
		Material *matB = matDefs.GetMaterial(props.Get(propName + ".material2", MakePropertyValues("mat2")).Get<string>());
		Texture *mix = GetTexture(props.Get(propName + ".amount", MakePropertyValues(.5f)));

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
		Texture *kr = GetTexture(props.Get(propName + ".kr", MakePropertyValues(.5f, .5f, .5f)));
		Texture *kt = GetTexture(props.Get(propName + ".kt", MakePropertyValues(.5f, .5f, .5f)));

		mat = new MatteTranslucentMaterial(emissionTex, bumpTex, normalTex, kr, kt);
	} else if (matType == "glossy2") {
		Texture *kd = GetTexture(props.Get(propName + ".kd", MakePropertyValues(.5f, .5f, .5f)));
		Texture *ks = GetTexture(props.Get(propName + ".ks", MakePropertyValues(.5f, .5f, .5f)));
		Texture *nu = GetTexture(props.Get(propName + ".uroughness", MakePropertyValues(.1f)));
		Texture *nv = GetTexture(props.Get(propName + ".vroughness", MakePropertyValues(.1f)));
		Texture *ka = GetTexture(props.Get(propName + ".ka", MakePropertyValues(0.f)));
		Texture *d = GetTexture(props.Get(propName + ".d", MakePropertyValues(0.f)));
		Texture *index = GetTexture(props.Get(propName + ".index", MakePropertyValues(0.f)));
		const bool multibounce = props.Get(propName + ".multibounce", MakePropertyValues(false)).Get<bool>();

		mat = new Glossy2Material(emissionTex, bumpTex, normalTex, kd, ks, nu, nv, ka, d, index, multibounce);
	} else if (matType == "metal2") {
		Texture *nu = GetTexture(props.Get(propName + ".uroughness", MakePropertyValues(.1f)));
		Texture *nv = GetTexture(props.Get(propName + ".vroughness", MakePropertyValues(.1f)));

		Texture *eta, *k;
		if (props.IsDefined(propName + ".preset")) {
			const string type = props.Get(propName + ".preset", MakePropertyValues("aluminium")).Get<string>();

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
			eta = GetTexture(props.Get(propName + ".n", MakePropertyValues(.5f, .5f, .5f)));
			k = GetTexture(props.Get(propName + ".k", MakePropertyValues(.5f, .5f, .5f)));
		}

		mat = new Metal2Material(emissionTex, bumpTex, normalTex, eta, k, nu, nv);
	} else if (matType == "roughglass") {
		Texture *kr = GetTexture(props.Get(propName + ".kr", MakePropertyValues(1.f, 1.f, 1.f)));
		Texture *kt = GetTexture(props.Get(propName + ".kt", MakePropertyValues(1.f, 1.f, 1.f)));
		Texture *ioroutside = GetTexture(props.Get(propName + ".ioroutside", MakePropertyValues(1.f)));
		Texture *iorinside = GetTexture(props.Get(propName + ".iorinside", MakePropertyValues(1.5f)));
		Texture *nu = GetTexture(props.Get(propName + ".uroughness", MakePropertyValues(.1f)));
		Texture *nv = GetTexture(props.Get(propName + ".vroughness", MakePropertyValues(.1f)));

		mat = new RoughGlassMaterial(emissionTex, bumpTex, normalTex, kr, kt, ioroutside, iorinside, nu, nv);
	} else
		throw runtime_error("Unknown material type: " + matType);

	mat->SetID(props.Get(propName + ".id", MakePropertyValues(defaultMatID)).Get<u_int>());
	mat->SetLightID(props.Get(propName + ".emission.id", MakePropertyValues(0u)).Get<u_int>());

	mat->SetSamples(Max(-1, props.Get(propName + ".samples", MakePropertyValues(-1)).Get<int>()));
	mat->SetEmittedSamples(Max(-1, props.Get(propName + ".emission.samples", MakePropertyValues(-1)).Get<int>()));

	mat->SetIndirectDiffuseVisibility(props.Get(propName + ".visibility.indirect.diffuse.enable", MakePropertyValues(true)).Get<bool>());
	mat->SetIndirectGlossyVisibility(props.Get(propName + ".visibility.indirect.glossy.enable", MakePropertyValues(true)).Get<bool>());
	mat->SetIndirectSpecularVisibility(props.Get(propName + ".visibility.indirect.specular.enable", MakePropertyValues(true)).Get<bool>());

	return mat;
}

SceneObject *Scene::CreateObject(const string &objName, const Properties &props) {
	const string key = "scene.objects." + objName;

	// Extract the material name
	const string matName = props.Get(key + ".material", MakePropertyValues("")).Get<string>();
	if (matName == "")
		throw runtime_error("Syntax error in object material reference: " + objName);

	// Build the object
	const string plyFileName = props.Get(key + ".ply", MakePropertyValues("")).Get<string>();
	if (plyFileName == "")
		throw runtime_error("Syntax error in object .ply file name: " + objName);

	// Check if I have to calculate normal or not
	const bool usePlyNormals = props.Get(key + ".useplynormals", MakePropertyValues(false)).Get<bool>();

	// Check if I have to use an instance mesh or not
	ExtMesh *mesh;
	if (props.IsDefined(key + ".transformation")) {
		const Matrix4x4 mat = props.Get(key + ".transformation", MakePropertyValues(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		mesh = extMeshCache.GetExtMesh(plyFileName, usePlyNormals, &trans);
	} else
		mesh = extMeshCache.GetExtMesh(plyFileName, usePlyNormals);

	// Get the material
	if (!matDefs.IsMaterialDefined(matName))
		throw runtime_error("Unknown material: " + matName);
	const Material *mat = matDefs.GetMaterial(matName);

	return new SceneObject(mesh, mat);
}

//------------------------------------------------------------------------------

LightSource *Scene::GetLightByType(const LightSourceType lightType) const {
	if (envLight && (lightType == envLight->GetType()))
			return envLight;
	if (sunLight && (lightType == TYPE_SUN))
			return sunLight;

	for (u_int i = 0; i < static_cast<u_int>(triLightDefs.size()); ++i) {
		LightSource *ls = triLightDefs[i];
		if (ls->GetType() == lightType)
			return ls;
	}

	return NULL;
}

const u_int Scene::GetLightCount() const {
	u_int lightsSize = static_cast<u_int>(triLightDefs.size());

	if (envLight)
		++lightsSize;
	if (sunLight)
		++lightsSize;

	return lightsSize;
}

LightSource *Scene::GetLightByIndex(const u_int lightIndex) const {
	const u_int lightsSize = GetLightCount();

	if (envLight) {
		if (sunLight) {
			if (lightIndex == lightsSize - 1)
				return sunLight;
			else if (lightIndex == lightsSize - 2)
				return envLight;
			else
				return triLightDefs[lightIndex];
		} else {
			if (lightIndex == lightsSize - 1)
				return envLight;
			else
				return triLightDefs[lightIndex];
		}
	} else {
		if (sunLight) {
			if (lightIndex == lightsSize - 1)
				return sunLight;
			else
				return triLightDefs[lightIndex];
		} else
			return triLightDefs[lightIndex];
	}
}

LightSource *Scene::SampleAllLights(const float u, float *pdf) const {
	// Power based light strategy
	const u_int lightIndex = lightsDistribution->SampleDiscrete(u, pdf);
	assert ((lightIndex >= 0) && (lightIndex < GetLightCount()));

	return GetLightByIndex(lightIndex);
}

float Scene::SampleAllLightPdf(const LightSource *light) const {
	// Power based light strategy
	return lightsDistribution->Pdf(light->GetSceneIndex());
}

bool Scene::Intersect(IntersectionDevice *device, const bool fromLight,
		const float passThrough, Ray *ray, RayHit *rayHit, BSDF *bsdf,
		Spectrum *connectionThroughput) const {
	*connectionThroughput = Spectrum(1.f, 1.f, 1.f);
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
