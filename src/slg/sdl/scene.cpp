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

	//--------------------------------------------------------------------------
	// Read camera position and target
	//--------------------------------------------------------------------------

	ParseCamera(scnProp);

	//--------------------------------------------------------------------------
	// Read all textures
	//--------------------------------------------------------------------------

	ParseTextures(scnProp);

	//--------------------------------------------------------------------------
	// Read all materials
	//--------------------------------------------------------------------------

	DefineMaterials(scnProp);

	//--------------------------------------------------------------------------
	// Read all objects .ply file
	//--------------------------------------------------------------------------

	AddObjects(scnProp);

	//--------------------------------------------------------------------------
	// Check if there is an infinitelight source defined
	//--------------------------------------------------------------------------

	AddInfiniteLight(scnProp);

	//--------------------------------------------------------------------------
	// Check if there is a SkyLight defined
	//--------------------------------------------------------------------------

	AddSkyLight(scnProp);

	//--------------------------------------------------------------------------
	// Check if there is a SunLight defined
	//--------------------------------------------------------------------------

	AddSunLight(scnProp);

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
	const vector<ExtMesh *> &objects = meshDefs.GetAllMesh();
	for (vector<ExtMesh *>::const_iterator obj = objects.begin(); obj != objects.end(); ++obj)
		dataSet->Add(*obj);

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
		for (u_int i = 0; i < meshDefs.GetSize(); ++i) {			
			if (WallClockTime() - lastPrint > 2.0) {
				SDL_LOG("  " << i << "/" << meshDefs.GetSize());
				lastPrint = WallClockTime();
			}

			const ExtMesh *mesh = meshDefs.GetExtMesh(i);
			//SDL_LOG("  " + mesh->GetName());
			props.Set(mesh->ToProperties(objectMaterials[i]->GetName(), extMeshCache));
		}

		return props;
}

vector<string> Scene::GetStringParameters(const Properties &prop, const string &paramName,
		const u_int paramCount, const string &defaultValue) {
	const vector<string> vs = prop.GetStringVector(paramName, defaultValue);
	if (vs.size() != paramCount) {
		stringstream ss;
		ss << "Syntax error in " << paramName << " (required " << paramCount << " parameters)";
		throw runtime_error(ss.str());
	}

	return vs;
}

vector<int> Scene::GetIntParameters(const Properties &prop, const string &paramName,
		const u_int paramCount, const string &defaultValue) {
	const vector<int> vi = prop.GetIntVector(paramName, defaultValue);
	if (vi.size() != paramCount) {
		stringstream ss;
		ss << "Syntax error in " << paramName << " (required " << paramCount << " parameters)";
		throw runtime_error(ss.str());
	}

	return vi;
}

vector<float> Scene::GetFloatParameters(const Properties &prop, const string &paramName,
		const u_int paramCount, const string &defaultValue) {
	const vector<float> vf = prop.GetFloatVector(paramName, defaultValue);
	if (vf.size() != paramCount) {
		stringstream ss;
		ss << "Syntax error in " << paramName << " (required " << paramCount << " parameters)";
		throw runtime_error(ss.str());
	}

	return vf;
}

//--------------------------------------------------------------------------
// Methods to build a scene from scratch
//--------------------------------------------------------------------------

void Scene::ParseCamera(const Properties &props) {
	if (!props.HaveNames("scene.camera.lookat")) {
		// There is no camera definition
		return;
	}

	Point orig, target;
	if (props.IsDefined("scene.camera.lookat")) {
		SDL_LOG("WARNING: deprecated syntax used in property scene.camera.lookat");

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
		texDefs.DefineTexture(texName, tex);
	}

	editActions.AddAction(MATERIALS_EDIT);
	editActions.AddAction(MATERIAL_TYPES_EDIT);
}

void Scene::DefineMaterials(const string &propsString) {
	Properties prop;
	prop.SetFromString(propsString);

	DefineMaterials(prop);
}

void Scene::DefineMaterials(const Properties &props) {
	vector<string> matKeys = props.GetAllNames("scene.materials.");
	if (matKeys.size() == 0)
		throw runtime_error("No material definition found");

	for (vector<string>::const_iterator matKey = matKeys.begin(); matKey != matKeys.end(); ++matKey) {
		const string &key = *matKey;
		const size_t dot1 = key.find(".", string("scene.materials.").length());
		if (dot1 == string::npos)
			continue;

		// Extract the material name
		const string matName = Property::ExtractField(key, 2);
		if (matName == "")
			throw runtime_error("Syntax error in material definition: " + matName);

		// Check if it is a new material root otherwise skip
		if (matDefs.IsMaterialDefined(matName))
			continue;

		SDL_LOG("Material definition: " << matName);

		// In order to have harlequin colors with MATERIAL_ID output
		const u_int matID = ((u_int)(RadicalInverse(matDefs.GetSize() + 1, 2) * 255.f + .5f)) |
				(((u_int)(RadicalInverse(matDefs.GetSize() + 1, 3) * 255.f + .5f)) << 8) |
				(((u_int)(RadicalInverse(matDefs.GetSize() + 1, 5) * 255.f + .5f)) << 16);
		Material *mat = CreateMaterial(matID, matName, props);
		matDefs.DefineMaterial(matName, mat);
	}
}

void Scene::UpdateMaterial(const string &name, const string &propsString) {
	Properties prop;
	prop.SetFromString(propsString);

	UpdateMaterial(name, prop);
}

void Scene::UpdateMaterial(const string &name, const Properties &props) {
	//SDL_LOG("Updating material " << name << " with:\n" << props.ToString());

	// Look for old material
	Material *oldMat = matDefs.GetMaterial(name);
	const bool wasLightSource = oldMat->IsLightSource();

	// Create new material
	Material *newMat = CreateMaterial(oldMat->GetID(), name, props);

	// UpdateMaterial() deletes oldMat
	matDefs.UpdateMaterial(name, newMat);

	// Replace old material direct references with new one
	for (u_int i = 0; i < objectMaterials.size(); ++i) {
		if (objectMaterials[i] == oldMat)
			objectMaterials[i] = newMat;
	}

	// Check if old and/or the new material were/is light sources
	if (wasLightSource || newMat->IsLightSource()) {
		// I have to build a new version of lights and triangleLightSource
		vector<TriangleLight *> newTriLights;
		vector<u_int> newMeshTriLightOffset;

		for (u_int i = 0; i < meshDefs.GetSize(); ++i) {
			const ExtMesh *mesh = meshDefs.GetExtMesh(i);

			if (objectMaterials[i]->IsLightSource()) {
				newMeshTriLightOffset.push_back(newTriLights.size());

				for (u_int j = 0; j < mesh->GetTotalTriangleCount(); ++j) {
					TriangleLight *tl = new TriangleLight(objectMaterials[i], mesh, i, j);
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
}

void Scene::AddObject(const string &objName, const string &meshName,
		const string &propsString) {
	Properties prop;
	prop.SetFromString("scene.objects." + objName + ".ply = " + meshName + "\n");
	prop.SetFromString(propsString);

	AddObject(objName, prop);
}

void Scene::AddObject(const string &objName, const Properties &props) {
	const string key = "scene.objects." + objName;

	// Extract the material name
	const string matName = GetStringParameters(props, key + ".material", 1, "").at(0);
	if (matName == "")
		throw runtime_error("Syntax error in object material reference: " + objName);

	// Build the object
	const string plyFileName = GetStringParameters(props, key + ".ply", 1, "").at(0);
	if (plyFileName == "")
		throw runtime_error("Syntax error in object .ply file name: " + objName);

	// Check if I have to calculate normal or not
	const bool usePlyNormals = props.GetBoolean(key + ".useplynormals", false);

	// Check if I have to use an instance mesh or not
	ExtMesh *meshObject;
	if (props.IsDefined(key + ".transformation")) {
		const vector<float> vf = GetFloatParameters(props, key + ".transformation", 16, "1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
		const Matrix4x4 mat(
				vf.at(0), vf.at(4), vf.at(8), vf.at(12),
				vf.at(1), vf.at(5), vf.at(9), vf.at(13),
				vf.at(2), vf.at(6), vf.at(10), vf.at(14),
				vf.at(3), vf.at(7), vf.at(11), vf.at(15));
		const Transform trans(mat);

		meshObject = extMeshCache.GetExtMesh(plyFileName, usePlyNormals, trans);
	} else
		meshObject = extMeshCache.GetExtMesh(plyFileName, usePlyNormals);

	meshDefs.DefineExtMesh(objName, meshObject);

	// Get the material
	if (!matDefs.IsMaterialDefined(matName))
		throw runtime_error("Unknown material: " + matName);
	Material *mat = matDefs.GetMaterial(matName);

	// Check if it is a light sources
	objectMaterials.push_back(mat);
	if (mat->IsLightSource()) {
		SDL_LOG("The " << objName << " object is a light sources with " << meshObject->GetTotalTriangleCount() << " triangles");

		meshTriLightDefsOffset.push_back(triLightDefs.size());
		for (u_int i = 0; i < meshObject->GetTotalTriangleCount(); ++i) {
			TriangleLight *tl = new TriangleLight(mat, meshObject, meshDefs.GetSize() - 1, i);
			triLightDefs.push_back(tl);
		}
	} else
		meshTriLightDefsOffset.push_back(NULL_INDEX);
}

void Scene::UpdateObjectTransformation(const string &objName, const Transform &trans) {
	ExtMesh *mesh = meshDefs.GetExtMesh(objName);

	ExtInstanceTriangleMesh *instanceMesh = dynamic_cast<ExtInstanceTriangleMesh *>(mesh);
	if (instanceMesh)
		instanceMesh->SetTransformation(trans);
	else
		mesh->ApplyTransform(trans);

	// Check if it is a light source
	const u_int meshIndex = meshDefs.GetExtMeshIndex(objName);
	if (objectMaterials[meshIndex]->IsLightSource()) {
		// Have to update all light sources using this mesh
		for (u_int i = meshTriLightDefsOffset[meshIndex]; i < mesh->GetTotalTriangleCount(); ++i)
			triLightDefs[i]->Init();
	}
}

void Scene::AddObjects(const string &propsString) {
	Properties prop;
	prop.SetFromString(propsString);

	AddObjects(prop);
}

void Scene::AddObjects(const Properties &props) {
	vector<string> objKeys = props.GetAllNames("scene.objects.");
	if (objKeys.size() == 0)
		throw runtime_error("Unable to find object definitions");

	double lastPrint = WallClockTime();
	u_int objCount = 0;
	for (vector<string>::const_iterator objKey = objKeys.begin(); objKey != objKeys.end(); ++objKey) {
		const string &key = *objKey;
		const size_t dot1 = key.find(".", string("scene.objects.").length());
		if (dot1 == string::npos)
			continue;

		// Extract the object name
		const string objName = Property::ExtractField(key, 2);
		if (objName == "")
			throw runtime_error("Syntax error in " + key);

		// Check if it is a new object root otherwise skip
		if (meshDefs.IsExtMeshDefined(objName))
			continue;

		AddObject(objName, props);
		++objCount;

		const double now = WallClockTime();
		if (now - lastPrint > 2.0) {
			SDL_LOG("PLY object count: " << objCount);
			lastPrint = now;
		}
	}
	SDL_LOG("PLY object count: " << objCount);
}

void Scene::AddInfiniteLight(const string &propsString) {
	Properties prop;
	prop.SetFromString(propsString);

	AddInfiniteLight(prop);
}

void Scene::AddInfiniteLight(const Properties &props) {
	const vector<string> ilParams = props.GetStringVector("scene.infinitelight.file", "");

	if (ilParams.size() > 0) {
		if (envLight)
			throw runtime_error("Can not define an infinitelight when there is already an skylight defined");

		vector<float> vf = GetFloatParameters(props, "scene.infinitelight.transformation", 16, "1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
		const Matrix4x4 mat(
				vf.at(0), vf.at(4), vf.at(8), vf.at(12),
				vf.at(1), vf.at(5), vf.at(9), vf.at(13),
				vf.at(2), vf.at(6), vf.at(10), vf.at(14),
				vf.at(3), vf.at(7), vf.at(11), vf.at(15));
		const Transform light2World(mat);

		const float gamma = props.GetFloat("scene.infinitelight.gamma", 2.2f);
		ImageMap *imgMap = imgMapCache.GetImageMap(ilParams.at(0), gamma);
		InfiniteLight *il = new InfiniteLight(light2World, imgMap);

		vf = GetFloatParameters(props, "scene.infinitelight.gain", 3, "1.0 1.0 1.0");
		il->SetGain(Spectrum(vf.at(0), vf.at(1), vf.at(2)));

		vf = GetFloatParameters(props, "scene.infinitelight.shift", 2, "0.0 0.0");
		il->GetUVMapping()->uDelta = vf.at(0);
		il->GetUVMapping()->vDelta = vf.at(1);
		il->SetSamples(props.GetInt("scene.infinitelight.samples", -1));
		il->SetID(props.GetInt("scene.infinitelight.id", 0));
		il->SetIndirectDiffuseVisibility(props.GetBoolean("scene.infinitelight.visibility.indirect.diffuse.enable", true));
		il->SetIndirectGlossyVisibility(props.GetBoolean("scene.infinitelight.visibility.indirect.glossy.enable", true));
		il->SetIndirectSpecularVisibility(props.GetBoolean("scene.infinitelight.visibility.indirect.specular.enable", true));
		il->Preprocess();

		envLight = il;
	} else
		envLight = NULL;
}

void Scene::AddSkyLight(const string &propsString) {
	Properties prop;
	prop.SetFromString(propsString);

	AddSkyLight(prop);
}

void Scene::AddSkyLight(const Properties &props) {
	const vector<string> silParams = props.GetStringVector("scene.skylight.dir", "");

	if (silParams.size() > 0) {
		if (envLight)
			throw runtime_error("Can not define a skylight when there is already an infinitelight defined");

		vector<float> vf = GetFloatParameters(props, "scene.skylight.transformation", 16, "1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
		const Matrix4x4 mat(
				vf.at(0), vf.at(4), vf.at(8), vf.at(12),
				vf.at(1), vf.at(5), vf.at(9), vf.at(13),
				vf.at(2), vf.at(6), vf.at(10), vf.at(14),
				vf.at(3), vf.at(7), vf.at(11), vf.at(15));
		const Transform light2World(mat);

		vector<float> sdir = GetFloatParameters(props, "scene.skylight.dir", 3, "0.0 0.0 1.0");
		const float turb = props.GetFloat("scene.skylight.turbidity", 2.2f);
		vector<float> gain = GetFloatParameters(props, "scene.skylight.gain", 3, "1.0 1.0 1.0");

		SkyLight *sl = new SkyLight(light2World, turb, Vector(sdir.at(0), sdir.at(1), sdir.at(2)));
		sl->SetGain(Spectrum(gain.at(0), gain.at(1), gain.at(2)));
		sl->SetSamples(props.GetInt("scene.skylight.samples", -1));
		sl->SetID(props.GetInt("scene.skylight.id", 0));
		sl->SetIndirectDiffuseVisibility(props.GetBoolean("scene.skylight.visibility.indirect.diffuse.enable", true));
		sl->SetIndirectGlossyVisibility(props.GetBoolean("scene.skylight.visibility.indirect.glossy.enable", true));
		sl->SetIndirectSpecularVisibility(props.GetBoolean("scene.skylight.visibility.indirect.specular.enable", true));
		sl->Preprocess();

		envLight = sl;
	}
}

void Scene::AddSunLight(const string &propsString) {
	Properties prop;
	prop.SetFromString(propsString);

	AddSunLight(prop);
}

void Scene::AddSunLight(const Properties &props) {
	const vector<string> sulParams = props.GetStringVector("scene.sunlight.dir", "");
	if (sulParams.size() > 0) {
		vector<float> vf = GetFloatParameters(props, "scene.sunlight.transformation", 16, "1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
		const Matrix4x4 mat(
				vf.at(0), vf.at(4), vf.at(8), vf.at(12),
				vf.at(1), vf.at(5), vf.at(9), vf.at(13),
				vf.at(2), vf.at(6), vf.at(10), vf.at(14),
				vf.at(3), vf.at(7), vf.at(11), vf.at(15));
		const Transform light2World(mat);

		vector<float> sdir = GetFloatParameters(props, "scene.sunlight.dir", 3, "0.0 0.0 1.0");
		const float turb = props.GetFloat("scene.sunlight.turbidity", 2.2f);
		const float relSize = props.GetFloat("scene.sunlight.relsize", 1.0f);
		vector<float> gain = GetFloatParameters(props, "scene.sunlight.gain", 3, "1.0 1.0 1.0");

		SunLight *sl = new SunLight(light2World, turb, relSize, Vector(sdir.at(0), sdir.at(1), sdir.at(2)));
		sl->SetGain(Spectrum(gain.at(0), gain.at(1), gain.at(2)));
		sl->SetSamples(props.GetInt("scene.sunlight.samples", -1));
		sl->SetID(props.GetInt("scene.sunlight.id", 0));
		sl->SetIndirectDiffuseVisibility(props.GetBoolean("scene.sunlight.visibility.indirect.diffuse.enable", true));
		sl->SetIndirectGlossyVisibility(props.GetBoolean("scene.sunlight.visibility.indirect.glossy.enable", true));
		sl->SetIndirectSpecularVisibility(props.GetBoolean("scene.sunlight.visibility.indirect.specular.enable", true));
		sl->Preprocess();

		sunLight = sl;
	} else
		sunLight = NULL;
}

void Scene::RemoveUnusedMaterials() {
	// Build a list of all referenced material names
	set<const Material *> referencedMats;
	for (vector<Material *>::const_iterator it = objectMaterials.begin(); it < objectMaterials.end(); ++it)
		(*it)->AddReferencedMaterials(referencedMats);

	// Get the list of all defined material
	vector<string> definedMats = matDefs.GetMaterialNames();
	for (vector<string>::const_iterator it = definedMats.begin(); it < definedMats.end(); ++it) {
		Material *m = matDefs.GetMaterial(*it);

		if (referencedMats.count(m) == 0) {
			SDL_LOG("Deleting unreferenced material: " << *it);
			matDefs.DeleteMaterial(*it);
		}
	}
}

void Scene::RemoveUnusedTextures() {
	// Build a list of all referenced textures names
	set<const Texture *> referencedTexs;
	for (vector<Material *>::const_iterator it = objectMaterials.begin(); it < objectMaterials.end(); ++it)
		(*it)->AddReferencedTextures(referencedTexs);

	// Get the list of all defined material
	vector<string> definedTexs = texDefs.GetTextureNames();
	for (vector<string>::const_iterator it = definedTexs.begin(); it < definedTexs.end(); ++it) {
		Texture *t = texDefs.GetTexture(*it);

		if (referencedTexs.count(t) == 0) {
			SDL_LOG("Deleting unreferenced texture: " << *it);
			texDefs.DeleteTexture(*it);
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

		const Matrix4x4 mat = props.Get(prefixName + ".transformation", matIdentity).Get<Matrix4x4>();
		const Transform trans(mat);

		return new UVMapping3D(trans);
	} else if (mapType == "globalmapping3d") {
		PropertyValues matIdentity(16);
		for (u_int i = 0; i < 4; ++i) {
			for (u_int j = 0; j < 4; ++j) {
				matIdentity[i * 4 + j] = (i == j) ? 1.f : 0.f;
			}
		}

		const Matrix4x4 mat = props.Get(prefixName + ".transformation", matIdentity).Get<Matrix4x4>();
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
		const string tex1Name = props.Get(propName + ".texture1", MakePropertyValues("1.0")).Get<string>();
		const Texture *tex1 = GetTexture(tex1Name);
		const string tex2Name = props.Get(propName + ".texture2", MakePropertyValues("1.0")).Get<string>();
		const Texture *tex2 = GetTexture(tex2Name);
		return new ScaleTexture(tex1, tex2);
	} else if (texType == "fresnelapproxn") {
		const string texName = props.Get(propName + ".texture", MakePropertyValues("0.5 0.5 0.5")).Get<string>();
		const Texture *tex = GetTexture(texName);
		return new FresnelApproxNTexture(tex);
	} else if (texType == "fresnelapproxk") {
		const string texName = props.Get(propName + ".texture", MakePropertyValues("0.5 0.5 0.5")).Get<string>();
		const Texture *tex = GetTexture(texName);
		return new FresnelApproxKTexture(tex);
	} else if (texType == "checkerboard2d") {
		const string tex1Name = props.Get(propName + ".texture1", MakePropertyValues("1.0")).Get<string>();
		const Texture *tex1 = GetTexture(tex1Name);
		const string tex2Name = props.Get(propName + ".texture2", MakePropertyValues("0.0")).Get<string>();
		const Texture *tex2 = GetTexture(tex2Name);

		return new CheckerBoard2DTexture(CreateTextureMapping2D(propName + ".mapping", props), tex1, tex2);
	} else if (texType == "checkerboard3d") {
		const string tex1Name = props.Get(propName + ".texture1", MakePropertyValues("1.0")).Get<string>();
		const Texture *tex1 = GetTexture(tex1Name);
		const string tex2Name = props.Get(propName + ".texture2", MakePropertyValues("0.0")).Get<string>();
		const Texture *tex2 = GetTexture(tex2Name);

		return new CheckerBoard3DTexture(CreateTextureMapping3D(propName + ".mapping", props), tex1, tex2);
	} else if (texType == "mix") {
		const string amtName = props.Get(propName + ".amount", MakePropertyValues("0.5")).Get<string>();
		const Texture *amtTex = GetTexture(amtName);
		const string tex1Name = props.Get(propName + ".texture1", MakePropertyValues("0.0")).Get<string>();
		const Texture *tex1 = GetTexture(tex1Name);
		const string tex2Name = props.Get(propName + ".texture2", MakePropertyValues("1.0")).Get<string>();
		const Texture *tex2 = GetTexture(tex2Name);

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
		const string insideTexName = props.Get(propName + ".inside", MakePropertyValues("1.0")).Get<string>();
		const Texture *insideTex = GetTexture(insideTexName);
		const string outsideTexName = props.Get(propName + ".outside", MakePropertyValues("0.0")).Get<string>();
		const Texture *outsideTex = GetTexture(outsideTexName);

		return new DotsTexture(CreateTextureMapping2D(propName + ".mapping", props), insideTex, outsideTex);
	} else if (texType == "brick") {
		const string tex1Name = props.Get(propName + ".bricktex", MakePropertyValues("1.0 1.0 1.0")).Get<string>();
		const Texture *tex1 = GetTexture(tex1Name);
		const string tex2Name = props.Get(propName + ".mortartex", MakePropertyValues("0.2 0.2 0.2")).Get<string>();
		const Texture *tex2 = GetTexture(tex2Name);
		const string tex3Name = props.Get(propName + ".brickmodtex", MakePropertyValues("1.0 1.0 1.0")).Get<string>();
		const Texture *tex3 = GetTexture(tex3Name);

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
		const string tex1Name = GetStringParameters(props, propName + ".texture1", 1, "1.0").at(0);
		const Texture *tex1 = GetTexture(tex1Name);
		const string tex2Name = GetStringParameters(props, propName + ".texture2", 1, "1.0").at(0);
		const Texture *tex2 = GetTexture(tex2Name);
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
		const string amtName = GetStringParameters(props, propName + ".amount", 1, "0.5").at(0);
		const Texture *amtTex = GetTexture(amtName);

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

Texture *Scene::GetTexture(const string &name) {
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
	const string matType = GetStringParameters(props, propName + ".type", 1, "matte").at(0);

	Texture *emissionTex = props.IsDefined(propName + ".emission") ? 
		GetTexture(props.GetString(propName + ".emission", "0.0 0.0 0.0")) : NULL;
	// Required to remove light source while editing the scene
	if (emissionTex && (
			((emissionTex->GetType() == CONST_FLOAT) && (((ConstFloatTexture *)emissionTex)->GetValue() == 0.f)) ||
			((emissionTex->GetType() == CONST_FLOAT3) && (((ConstFloat3Texture *)emissionTex)->GetColor().Black()))))
		emissionTex = NULL;

	Texture *bumpTex = props.IsDefined(propName + ".bumptex") ? 
		GetTexture(props.GetString(propName + ".bumptex", "1.0")) : NULL;
	Texture *normalTex = props.IsDefined(propName + ".normaltex") ? 
		GetTexture(props.GetString(propName + ".normaltex", "1.0")) : NULL;

	Material *mat;
	if (matType == "matte") {
		Texture *kd = GetTexture(props.GetString(propName + ".kd", "0.75 0.75 0.75"));

		mat = new MatteMaterial(emissionTex, bumpTex, normalTex, kd);
	} else if (matType == "mirror") {
		Texture *kr = GetTexture(props.GetString(propName + ".kr", "1.0 1.0 1.0"));
		
		mat = new MirrorMaterial(emissionTex, bumpTex, normalTex, kr);
	} else if (matType == "glass") {
		Texture *kr = GetTexture(props.GetString(propName + ".kr", "1.0 1.0 1.0"));
		Texture *kt = GetTexture(props.GetString(propName + ".kt", "1.0 1.0 1.0"));
		Texture *ioroutside = GetTexture(props.GetString(propName + ".ioroutside", "1.0"));
		Texture *iorinside = GetTexture(props.GetString(propName + ".iorinside", "1.5"));

		mat = new GlassMaterial(emissionTex, bumpTex, normalTex, kr, kt, ioroutside, iorinside);
	} else if (matType == "metal") {
		Texture *kr = GetTexture(props.GetString(propName + ".kr", "1.0 1.0 1.0"));
		Texture *exp = GetTexture(props.GetString(propName + ".exp", "10.0"));

		mat = new MetalMaterial(emissionTex, bumpTex, normalTex, kr, exp);
	} else if (matType == "archglass") {
		Texture *kr = GetTexture(props.GetString(propName + ".kr", "1.0 1.0 1.0"));
		Texture *kt = GetTexture(props.GetString(propName + ".kt", "1.0 1.0 1.0"));
		Texture *ioroutside = GetTexture(props.GetString(propName + ".ioroutside", "1.0"));
		Texture *iorinside = GetTexture(props.GetString(propName + ".iorinside", "1.5"));

		mat = new ArchGlassMaterial(emissionTex, bumpTex, normalTex, kr, kt, ioroutside, iorinside);
	} else if (matType == "mix") {
		Material *matA = matDefs.GetMaterial(props.GetString(propName + ".material1", "mat1"));
		Material *matB = matDefs.GetMaterial(props.GetString(propName + ".material2", "mat2"));
		Texture *mix = GetTexture(props.GetString(propName + ".amount", "0.5"));

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
		Texture *kr = GetTexture(props.GetString(propName + ".kr", "0.5 0.5 0.5"));
		Texture *kt = GetTexture(props.GetString(propName + ".kt", "0.5 0.5 0.5"));

		mat = new MatteTranslucentMaterial(emissionTex, bumpTex, normalTex, kr, kt);
	} else if (matType == "glossy2") {
		Texture *kd = GetTexture(props.GetString(propName + ".kd", "0.5 0.5 0.5"));
		Texture *ks = GetTexture(props.GetString(propName + ".ks", "0.5 0.5 0.5"));
		Texture *nu = GetTexture(props.GetString(propName + ".uroughness", "0.1"));
		Texture *nv = GetTexture(props.GetString(propName + ".vroughness", "0.1"));
		Texture *ka = GetTexture(props.GetString(propName + ".ka", "0.0"));
		Texture *d = GetTexture(props.GetString(propName + ".d", "0.0"));
		Texture *index = GetTexture(props.GetString(propName + ".index", "0.0"));
		const bool multibounce = props.GetBoolean(propName + ".multibounce", false);

		mat = new Glossy2Material(emissionTex, bumpTex, normalTex, kd, ks, nu, nv, ka, d, index, multibounce);
	} else if (matType == "metal2") {
		Texture *nu = GetTexture(props.GetString(propName + ".uroughness", "0.1"));
		Texture *nv = GetTexture(props.GetString(propName + ".vroughness", "0.1"));

		Texture *eta, *k;
		if (props.IsDefined(propName + ".preset")) {
			const string type = props.GetString(propName + ".preset", "aluminium");

			if (type == "aluminium") {
				eta = GetTexture("1.697 0.879833 0.530174");
				k = GetTexture("9.30201 6.27604 4.89434");
			} else if (type == "silver") {
				eta = GetTexture("0.155706 0.115925 0.138897");
				k = GetTexture("4.88648 3.12787 2.17797");
			} else if (type == "gold") {
				eta = GetTexture("0.117959 0.354153 1.43897");
				k = GetTexture("4.03165 2.39416 1.61967");
			} else if (type == "copper") {
				eta = GetTexture("0.134794 0.928983 1.10888");
				k = GetTexture("3.98126 2.44098 2.16474");
			} else if (type == "amorphous carbon") {
				eta = GetTexture("2.94553 2.22816 1.98665");
				k = GetTexture("0.876641 0.799505 0.821194");
			} else
				throw runtime_error("Unknown Metal2 preset: " + type);
		} else {
			eta = GetTexture(props.GetString(propName + ".n", "0.5 0.5 0.5"));
			k = GetTexture(props.GetString(propName + ".k", "0.5 0.5 0.5"));
		}

		mat = new Metal2Material(emissionTex, bumpTex, normalTex, eta, k, nu, nv);
	} else if (matType == "roughglass") {
		Texture *kr = GetTexture(props.GetString(propName + ".kr", "1.0 1.0 1.0"));
		Texture *kt = GetTexture(props.GetString(propName + ".kt", "1.0 1.0 1.0"));
		Texture *ioroutside = GetTexture(props.GetString(propName + ".ioroutside", "1.0"));
		Texture *iorinside = GetTexture(props.GetString(propName + ".iorinside", "1.5"));
		Texture *nu = GetTexture(props.GetString(propName + ".uroughness", "0.1"));
		Texture *nv = GetTexture(props.GetString(propName + ".vroughness", "0.1"));

		mat = new RoughGlassMaterial(emissionTex, bumpTex, normalTex, kr, kt, ioroutside, iorinside, nu, nv);
	} else
		throw runtime_error("Unknown material type: " + matType);

	mat->SetID(props.GetInt(propName + ".id", defaultMatID));
	mat->SetLightID(props.GetInt(propName + ".emission.id", 0));

	mat->SetSamples(Max(-1, props.GetInt(propName + ".samples", -1)));
	mat->SetEmittedSamples(Max(-1, props.GetInt(propName + ".emission.samples", -1)));

	mat->SetIndirectDiffuseVisibility(props.GetBoolean(propName + ".visibility.indirect.diffuse.enable", true));
	mat->SetIndirectGlossyVisibility(props.GetBoolean(propName + ".visibility.indirect.glossy.enable", true));
	mat->SetIndirectSpecularVisibility(props.GetBoolean(propName + ".visibility.indirect.specular.enable", true));

	return mat;
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
