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

#include <boost/detail/container_fwd.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>

#include "luxrays/core/dataset.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/properties.h"
#include "slg/sdl/sdl.h"
#include "slg/sdl/scene.h"

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
}

Scene::Scene(const std::string &fileName, const float imageScale) {
	// Just in case there is an unexpected exception during the scene loading
    camera = NULL;
	envLight = NULL;
	sunLight = NULL;
	dataSet = NULL;
	lightsDistribution = NULL;

	imgMapCache.SetImageResize(imageScale);

	SDL_LOG("Reading scene: " << fileName);

	Properties scnProp(fileName);

	//--------------------------------------------------------------------------
	// Read camera position and target
	//--------------------------------------------------------------------------

	CreateCamera(scnProp);

	//--------------------------------------------------------------------------
	// Read all textures
	//--------------------------------------------------------------------------

	DefineTextures(scnProp);

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
		throw std::runtime_error("The scene doesn't include any light source");

	dataSet = NULL;
	accelType = ACCEL_AUTO;
	enableInstanceSupport = true;
}

Scene::~Scene() {
	delete camera;
	delete envLight;
	delete sunLight;

	for (std::vector<TriangleLight *>::const_iterator l = triLightDefs.begin(); l != triLightDefs.end(); ++l)
		delete *l;

	delete dataSet;
	delete lightsDistribution;
}

void Scene::Preprocess(Context *ctx) {
	// Rebuild the data set
	delete dataSet;
	dataSet = new DataSet(ctx);
	dataSet->SetInstanceSupport(enableInstanceSupport);
	dataSet->SetAcceleratorType(accelType);

	// Add all objects
	const std::vector<ExtMesh *> &objects = meshDefs.GetAllMesh();
	for (std::vector<ExtMesh *>::const_iterator obj = objects.begin(); obj != objects.end(); ++obj)
		dataSet->Add(*obj);

	dataSet->Preprocess();

	// Rebuild the data to power based light sampling
	const float worldRadius = lightWorldRadiusScale * dataSet->GetBSphere().rad * 1.01f;
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

	// Initialize the table to look light indices
	light2Index.clear();
	for (u_int i = 0; i < lightCount; ++i)
		light2Index[GetLightByIndex(i)] = i;
}

Properties Scene::ToProperties(const std::string &directoryName) {
		Properties props;

		// Write the camera information
		SDL_LOG("Saving camera information");
		props.Load(camera->ToProperties());

		if (envLight) {
			// Write the infinitelight/skylight information
			SDL_LOG("Saving infinitelight/skylight information");
			props.Load(envLight->ToProperties(imgMapCache));
		}
		
		if (sunLight) {
			// Write the sunlight information
			SDL_LOG("Saving sunlight information");
			props.Load(sunLight->ToProperties());
		}

		// Write the image map information
		SDL_LOG("Saving image map information:");
		std::vector<ImageMap *> ims;
		imgMapCache.GetImageMaps(ims);
		for (u_int i = 0; i < ims.size(); ++i) {
			const std::string fileName = directoryName + "/imagemap-" + (boost::format("%05d") % i).str() + ".exr";
			SDL_LOG("  " + fileName);
			ims[i]->WriteImage(fileName);
		}

		// Write the texture information
		SDL_LOG("Saving texture information:");
		for (u_int i = 0; i < texDefs.GetSize(); ++i) {
			const Texture *tex = texDefs.GetTexture(i);
			SDL_LOG("  " + tex->GetName());
			props.Load(tex->ToProperties(imgMapCache));
		}

		// Write the material information
		SDL_LOG("Saving material information:");
		for (u_int i = 0; i < matDefs.GetSize(); ++i) {
			const Material *mat = matDefs.GetMaterial(i);
			SDL_LOG("  " + mat->GetName());
			props.Load(mat->ToProperties());
		}

		// Write the mesh information
		SDL_LOG("Saving mesh information:");
		const std::vector<ExtMesh *> &meshes =  extMeshCache.GetMeshes();
		std::set<std::string> savedMeshes;
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
			const std::string fileName = directoryName + "/mesh-" + (boost::format("%05d") % meshIndex).str() + ".ply";

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
			props.Load(mesh->ToProperties(objectMaterials[i]->GetName(), extMeshCache));
		}

		return props;
}

std::vector<std::string> Scene::GetStringParameters(const Properties &prop, const std::string &paramName,
		const u_int paramCount, const std::string &defaultValue) {
	const std::vector<std::string> vs = prop.GetStringVector(paramName, defaultValue);
	if (vs.size() != paramCount) {
		std::stringstream ss;
		ss << "Syntax error in " << paramName << " (required " << paramCount << " parameters)";
		throw std::runtime_error(ss.str());
	}

	return vs;
}

std::vector<int> Scene::GetIntParameters(const Properties &prop, const std::string &paramName,
		const u_int paramCount, const std::string &defaultValue) {
	const std::vector<int> vi = prop.GetIntVector(paramName, defaultValue);
	if (vi.size() != paramCount) {
		std::stringstream ss;
		ss << "Syntax error in " << paramName << " (required " << paramCount << " parameters)";
		throw std::runtime_error(ss.str());
	}

	return vi;
}

std::vector<float> Scene::GetFloatParameters(const Properties &prop, const std::string &paramName,
		const u_int paramCount, const std::string &defaultValue) {
	const std::vector<float> vf = prop.GetFloatVector(paramName, defaultValue);
	if (vf.size() != paramCount) {
		std::stringstream ss;
		ss << "Syntax error in " << paramName << " (required " << paramCount << " parameters)";
		throw std::runtime_error(ss.str());
	}

	return vf;
}

//--------------------------------------------------------------------------
// Methods to build a scene from scratch
//--------------------------------------------------------------------------

void Scene::CreateCamera(const std::string &propsString) {
	Properties prop;
	prop.LoadFromString(propsString);

	CreateCamera(prop);
}

void Scene::CreateCamera(const Properties &props) {
	std::vector<float> vf = GetFloatParameters(props, "scene.camera.lookat", 6, "10.0 0.0 0.0  0.0 0.0 0.0");
	Point orig(vf.at(0), vf.at(1), vf.at(2));
	Point target(vf.at(3), vf.at(4), vf.at(5));

	SDL_LOG("Camera position: " << orig);
	SDL_LOG("Camera target: " << target);

	vf = GetFloatParameters(props, "scene.camera.up", 3, "0.0 0.0 0.1");
	const Vector up(vf.at(0), vf.at(1), vf.at(2));

	if (props.IsDefined("scene.camera.screenwindow")) {
		vf = GetFloatParameters(props, "scene.camera.screenwindow", 4, "0.0 1.0 0.0 1.0");

		camera = new PerspectiveCamera(orig, target, up, &vf[0]);
	} else
		camera = new PerspectiveCamera(orig, target, up);

	camera->clipHither = props.GetFloat("scene.camera.cliphither", 1e-3f);
	camera->clipYon = props.GetFloat("scene.camera.clipyon", 1e30f);
	camera->lensRadius = props.GetFloat("scene.camera.lensradius", 0.f);
	camera->focalDistance = props.GetFloat("scene.camera.focaldistance", 10.f);
	camera->fieldOfView = props.GetFloat("scene.camera.fieldofview", 45.f);

	if (props.GetBoolean("scene.camera.horizontalstereo.enable", false)) {
		SDL_LOG("Camera horizontal stereo enabled");
		camera->SetHorizontalStereo(true);

		const float eyesDistance = props.GetFloat("scene.camera.horizontalstereo.eyesdistance", .0626f);
		SDL_LOG("Camera horizontal stereo eyes distance: " << eyesDistance);
		camera->SetHorizontalStereoEyesDistance(eyesDistance);
		const float lesnDistance = props.GetFloat("scene.camera.horizontalstereo.lensdistance", .1f);
		SDL_LOG("Camera horizontal stereo lens distance: " << lesnDistance);
		camera->SetHorizontalStereoLensDistance(lesnDistance);


		// Check if I have to enable Oculus Rift Barrel post-processing
		if (props.GetBoolean("scene.camera.horizontalstereo.oculusrift.barrelpostpro.enable", false)) {
			SDL_LOG("Camera Oculus Rift Barrel post-processing enabled");
			camera->SetOculusRiftBarrel(true);
		} else {
			SDL_LOG("Camera Oculus Rift Barrel post-processing disabled");
			camera->SetOculusRiftBarrel(false);
		}
	} else {
		SDL_LOG("Camera horizontal stereo disabled");
		camera->SetHorizontalStereo(false);
	}
}

void Scene::DefineTextures(const std::string &propsString) {
	Properties prop;
	prop.LoadFromString(propsString);

	DefineTextures(prop);
}

void Scene::DefineTextures(const Properties &props) {
	std::vector<std::string> texKeys = props.GetAllKeys("scene.textures.");
	if (texKeys.size() == 0)
		return;

	for (std::vector<std::string>::const_iterator matKey = texKeys.begin(); matKey != texKeys.end(); ++matKey) {
		const std::string &key = *matKey;
		const size_t dot1 = key.find(".", std::string("scene.textures.").length());
		if (dot1 == std::string::npos)
			continue;

		// Extract the material name
		const std::string texName = Properties::ExtractField(key, 2);
		if (texName == "")
			throw std::runtime_error("Syntax error in texture definition: " + texName);

		// Check if it is a new material root otherwise skip
		if (texDefs.IsTextureDefined(texName))
			continue;

		SDL_LOG("Texture definition: " << texName);

		Texture *tex = CreateTexture(texName, props);
		texDefs.DefineTexture(texName, tex);
	}
}

void Scene::DefineMaterials(const std::string &propsString) {
	Properties prop;
	prop.LoadFromString(propsString);

	DefineMaterials(prop);
}

void Scene::DefineMaterials(const Properties &props) {
	std::vector<std::string> matKeys = props.GetAllKeys("scene.materials.");
	if (matKeys.size() == 0)
		throw std::runtime_error("No material definition found");

	for (std::vector<std::string>::const_iterator matKey = matKeys.begin(); matKey != matKeys.end(); ++matKey) {
		const std::string &key = *matKey;
		const size_t dot1 = key.find(".", std::string("scene.materials.").length());
		if (dot1 == std::string::npos)
			continue;

		// Extract the material name
		const std::string matName = Properties::ExtractField(key, 2);
		if (matName == "")
			throw std::runtime_error("Syntax error in material definition: " + matName);

		// Check if it is a new material root otherwise skip
		if (matDefs.IsMaterialDefined(matName))
			continue;

		SDL_LOG("Material definition: " << matName);

		Material *mat = CreateMaterial(matName, props);
		matDefs.DefineMaterial(matName, mat);
	}
}

void Scene::UpdateMaterial(const std::string &name, const std::string &propsString) {
	Properties prop;
	prop.LoadFromString(propsString);

	UpdateMaterial(name, prop);
}

void Scene::UpdateMaterial(const std::string &name, const Properties &props) {
	//SDL_LOG("Updating material " << name << " with:\n" << props.ToString());

	// Look for old material
	Material *oldMat = matDefs.GetMaterial(name);
	const bool wasLightSource = oldMat->IsLightSource();

	// Create new material
	Material *newMat = CreateMaterial(name, props);

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
		std::vector<TriangleLight *> newTriLights;
		std::vector<u_int> newMeshTriLightOffset;

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
		for (std::vector<TriangleLight *>::const_iterator l = triLightDefs.begin(); l != triLightDefs.end(); ++l)
			delete *l;

		// Use the new versions
		triLightDefs = newTriLights;
		meshTriLightDefsOffset = newMeshTriLightOffset;
	}
}

void Scene::AddObject(const std::string &objName, const std::string &meshName,
		const std::string &propsString) {
	Properties prop;
	prop.LoadFromString("scene.objects." + objName + ".ply = " + meshName + "\n");
	prop.LoadFromString(propsString);

	AddObject(objName, prop);
}

void Scene::AddObject(const std::string &objName, const Properties &props) {
	const std::string key = "scene.objects." + objName;

	// Extract the material name
	const std::string matName = GetStringParameters(props, key + ".material", 1, "").at(0);
	if (matName == "")
		throw std::runtime_error("Syntax error in object material reference: " + objName);

	// Build the object
	const std::string plyFileName = GetStringParameters(props, key + ".ply", 1, "").at(0);
	if (plyFileName == "")
		throw std::runtime_error("Syntax error in object .ply file name: " + objName);

	// Check if I have to calculate normal or not
	const bool usePlyNormals = props.GetBoolean(key + ".useplynormals", false);

	// Check if I have to use an instance mesh or not
	ExtMesh *meshObject;
	if (props.IsDefined(key + ".transformation")) {
		const std::vector<float> vf = GetFloatParameters(props, key + ".transformation", 16, "1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
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
		throw std::runtime_error("Unknown material: " + matName);
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

void Scene::UpdateObjectTransformation(const std::string &objName, const Transform &trans) {
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

void Scene::AddObjects(const std::string &propsString) {
	Properties prop;
	prop.LoadFromString(propsString);

	AddObjects(prop);
}

void Scene::AddObjects(const Properties &props) {
	std::vector<std::string> objKeys = props.GetAllKeys("scene.objects.");
	if (objKeys.size() == 0)
		throw std::runtime_error("Unable to find object definitions");

	double lastPrint = WallClockTime();
	u_int objCount = 0;
	for (std::vector<std::string>::const_iterator objKey = objKeys.begin(); objKey != objKeys.end(); ++objKey) {
		const std::string &key = *objKey;
		const size_t dot1 = key.find(".", std::string("scene.objects.").length());
		if (dot1 == std::string::npos)
			continue;

		// Extract the object name
		const std::string objName = Properties::ExtractField(key, 2);
		if (objName == "")
			throw std::runtime_error("Syntax error in " + key);

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

void Scene::AddInfiniteLight(const std::string &propsString) {
	Properties prop;
	prop.LoadFromString(propsString);

	AddInfiniteLight(prop);
}

void Scene::AddInfiniteLight(const Properties &props) {
	const std::vector<std::string> ilParams = props.GetStringVector("scene.infinitelight.file", "");

	if (ilParams.size() > 0) {
		if (envLight)
			throw std::runtime_error("Can not define an infinitelight when there is already an skylight defined");

		std::vector<float> vf = GetFloatParameters(props, "scene.infinitelight.transformation", 16, "1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
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
		il->SetSamples(Max(1, props.GetInt("scene.infinitelight.samples", 1)));
		il->Preprocess();

		envLight = il;
	} else
		envLight = NULL;
}

void Scene::AddSkyLight(const std::string &propsString) {
	Properties prop;
	prop.LoadFromString(propsString);

	AddSkyLight(prop);
}

void Scene::AddSkyLight(const Properties &props) {
	const std::vector<std::string> silParams = props.GetStringVector("scene.skylight.dir", "");

	if (silParams.size() > 0) {
		if (envLight)
			throw std::runtime_error("Can not define a skylight when there is already an infinitelight defined");

		std::vector<float> vf = GetFloatParameters(props, "scene.skylight.transformation", 16, "1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
		const Matrix4x4 mat(
				vf.at(0), vf.at(4), vf.at(8), vf.at(12),
				vf.at(1), vf.at(5), vf.at(9), vf.at(13),
				vf.at(2), vf.at(6), vf.at(10), vf.at(14),
				vf.at(3), vf.at(7), vf.at(11), vf.at(15));
		const Transform light2World(mat);

		std::vector<float> sdir = GetFloatParameters(props, "scene.skylight.dir", 3, "0.0 0.0 1.0");
		const float turb = props.GetFloat("scene.skylight.turbidity", 2.2f);
		std::vector<float> gain = GetFloatParameters(props, "scene.skylight.gain", 3, "1.0 1.0 1.0");

		SkyLight *sl = new SkyLight(light2World, turb, Vector(sdir.at(0), sdir.at(1), sdir.at(2)));
		sl->SetGain(Spectrum(gain.at(0), gain.at(1), gain.at(2)));
		sl->SetSamples(Max(1, props.GetInt("scene.skylight.samples", 1)));
		sl->Preprocess();

		envLight = sl;
	}
}

void Scene::AddSunLight(const std::string &propsString) {
	Properties prop;
	prop.LoadFromString(propsString);

	AddSunLight(prop);
}

void Scene::AddSunLight(const Properties &props) {
	const std::vector<std::string> sulParams = props.GetStringVector("scene.sunlight.dir", "");
	if (sulParams.size() > 0) {
		std::vector<float> vf = GetFloatParameters(props, "scene.sunlight.transformation", 16, "1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
		const Matrix4x4 mat(
				vf.at(0), vf.at(4), vf.at(8), vf.at(12),
				vf.at(1), vf.at(5), vf.at(9), vf.at(13),
				vf.at(2), vf.at(6), vf.at(10), vf.at(14),
				vf.at(3), vf.at(7), vf.at(11), vf.at(15));
		const Transform light2World(mat);

		std::vector<float> sdir = GetFloatParameters(props, "scene.sunlight.dir", 3, "0.0 0.0 1.0");
		const float turb = props.GetFloat("scene.sunlight.turbidity", 2.2f);
		const float relSize = props.GetFloat("scene.sunlight.relsize", 1.0f);
		std::vector<float> gain = GetFloatParameters(props, "scene.sunlight.gain", 3, "1.0 1.0 1.0");

		SunLight *sl = new SunLight(light2World, turb, relSize, Vector(sdir.at(0), sdir.at(1), sdir.at(2)));
		sl->SetGain(Spectrum(gain.at(0), gain.at(1), gain.at(2)));
		sl->SetSamples(Max(1, props.GetInt("scene.sunlight.samples", 1)));
		sl->Preprocess();

		sunLight = sl;
	} else
		sunLight = NULL;
}

void Scene::RemoveUnusedMaterials() {
	// Build a list of all referenced material names
	std::set<const Material *> referencedMats;
	for (std::vector<Material *>::const_iterator it = objectMaterials.begin(); it < objectMaterials.end(); ++it)
		(*it)->AddReferencedMaterials(referencedMats);

	// Get the list of all defined material
	std::vector<std::string> definedMats = matDefs.GetMaterialNames();
	for (std::vector<std::string>::const_iterator it = definedMats.begin(); it < definedMats.end(); ++it) {
		Material *m = matDefs.GetMaterial(*it);

		if (referencedMats.count(m) == 0) {
			SDL_LOG("Deleting unreferenced material: " << *it);
			matDefs.DeleteMaterial(*it);
		}
	}
}

void Scene::RemoveUnusedTextures() {
	// Build a list of all referenced textures names
	std::set<const Texture *> referencedTexs;
	for (std::vector<Material *>::const_iterator it = objectMaterials.begin(); it < objectMaterials.end(); ++it)
		(*it)->AddReferencedTextures(referencedTexs);

	// Get the list of all defined material
	std::vector<std::string> definedTexs = texDefs.GetTextureNames();
	for (std::vector<std::string>::const_iterator it = definedTexs.begin(); it < definedTexs.end(); ++it) {
		Texture *t = texDefs.GetTexture(*it);

		if (referencedTexs.count(t) == 0) {
			SDL_LOG("Deleting unreferenced texture: " << *it);
			texDefs.DeleteTexture(*it);
		}
	}
}

//------------------------------------------------------------------------------

TextureMapping2D *Scene::CreateTextureMapping2D(const std::string &prefixName, const Properties &props) {
	const std::string mapType = GetStringParameters(props, prefixName + ".type", 1, "uvmapping2d").at(0);

	if (mapType == "uvmapping2d") {
		const std::vector<float> uvScale = GetFloatParameters(props, prefixName + ".uvscale", 2, "1.0 1.0");
		const std::vector<float> uvDelta = GetFloatParameters(props, prefixName + ".uvdelta", 2, "0.0 0.0");

		return new UVMapping2D(uvScale.at(0), uvScale.at(1), uvDelta.at(0), uvDelta.at(1));
	} else
		throw std::runtime_error("Unknown 2D texture coordinate mapping type: " + mapType);
}

TextureMapping3D *Scene::CreateTextureMapping3D(const std::string &prefixName, const Properties &props) {
	const std::string mapType = GetStringParameters(props, prefixName + ".type", 1, "uvmapping3d").at(0);

	if (mapType == "uvmapping3d") {
		const std::vector<float> vf = GetFloatParameters(props, prefixName + ".transformation", 16, "1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
		const Matrix4x4 mat(
				vf.at(0), vf.at(4), vf.at(8), vf.at(12),
				vf.at(1), vf.at(5), vf.at(9), vf.at(13),
				vf.at(2), vf.at(6), vf.at(10), vf.at(14),
				vf.at(3), vf.at(7), vf.at(11), vf.at(15));
		const Transform trans(mat);

		return new UVMapping3D(trans);
	} else if (mapType == "globalmapping3d") {
		const std::vector<float> vf = GetFloatParameters(props, prefixName + ".transformation", 16, "1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
		const Matrix4x4 mat(
				vf.at(0), vf.at(4), vf.at(8), vf.at(12),
				vf.at(1), vf.at(5), vf.at(9), vf.at(13),
				vf.at(2), vf.at(6), vf.at(10), vf.at(14),
				vf.at(3), vf.at(7), vf.at(11), vf.at(15));
		const Transform trans(mat);

		return new GlobalMapping3D(trans);
	} else
		throw std::runtime_error("Unknown 3D texture coordinate mapping type: " + mapType);
}

Texture *Scene::CreateTexture(const std::string &texName, const Properties &props) {
	const std::string propName = "scene.textures." + texName;
	const std::string texType = GetStringParameters(props, propName + ".type", 1, "imagemap").at(0);

	if (texType == "imagemap") {
		const std::vector<std::string> vname = GetStringParameters(props, propName + ".file", 1, "");
		if (vname.at(0) == "")
			throw std::runtime_error("Missing image map file name for texture: " + texName);

		const std::vector<float> gamma = GetFloatParameters(props, propName + ".gamma", 1, "2.2");
		const std::vector<float> gain = GetFloatParameters(props, propName + ".gain", 1, "1.0");

		ImageMap *im = imgMapCache.GetImageMap(vname.at(0), gamma.at(0));
		return new ImageMapTexture(im, CreateTextureMapping2D(propName + ".mapping", props), gain.at(0));
	} else if (texType == "constfloat1") {
		const std::vector<float> v = GetFloatParameters(props, propName + ".value", 1, "1.0");
		return new ConstFloatTexture(v.at(0));
	} else if (texType == "constfloat3") {
		const std::vector<float> v = GetFloatParameters(props, propName + ".value", 3, "1.0 1.0 1.0");
		return new ConstFloat3Texture(Spectrum(v.at(0), v.at(1), v.at(2)));
	} else if (texType == "scale") {
		const std::string tex1Name = GetStringParameters(props, propName + ".texture1", 1, "1.0").at(0);
		const Texture *tex1 = GetTexture(tex1Name);
		const std::string tex2Name = GetStringParameters(props, propName + ".texture2", 1, "1.0").at(0);
		const Texture *tex2 = GetTexture(tex2Name);
		return new ScaleTexture(tex1, tex2);
	} else if (texType == "fresnelapproxn") {
		const std::string texName = GetStringParameters(props, propName + ".texture", 1, "0.5 0.5 0.5").at(0);
		const Texture *tex = GetTexture(texName);
		return new FresnelApproxNTexture(tex);
	} else if (texType == "fresnelapproxk") {
		const std::string texName = GetStringParameters(props, propName + ".texture", 1, "0.5 0.5 0.5").at(0);
		const Texture *tex = GetTexture(texName);
		return new FresnelApproxKTexture(tex);
	} else if (texType == "checkerboard2d") {
		const std::string tex1Name = GetStringParameters(props, propName + ".texture1", 1, "1.0").at(0);
		const Texture *tex1 = GetTexture(tex1Name);
		const std::string tex2Name = GetStringParameters(props, propName + ".texture2", 1, "0.0").at(0);
		const Texture *tex2 = GetTexture(tex2Name);

		return new CheckerBoard2DTexture(CreateTextureMapping2D(propName + ".mapping", props), tex1, tex2);
	} else if (texType == "checkerboard3d") {
		const std::string tex1Name = GetStringParameters(props, propName + ".texture1", 1, "1.0").at(0);
		const Texture *tex1 = GetTexture(tex1Name);
		const std::string tex2Name = GetStringParameters(props, propName + ".texture2", 1, "0.0").at(0);
		const Texture *tex2 = GetTexture(tex2Name);

		return new CheckerBoard3DTexture(CreateTextureMapping3D(propName + ".mapping", props), tex1, tex2);
	} else if (texType == "mix") {
		const std::string amtName = GetStringParameters(props, propName + ".amount", 1, "0.5").at(0);
		const Texture *amtTex = GetTexture(amtName);
		const std::string tex1Name = GetStringParameters(props, propName + ".texture1", 1, "0.0").at(0);
		const Texture *tex1 = GetTexture(tex1Name);
		const std::string tex2Name = GetStringParameters(props, propName + ".texture2", 1, "1.0").at(0);
		const Texture *tex2 = GetTexture(tex2Name);

		return new MixTexture(amtTex, tex1, tex2);
	} else if (texType == "fbm") {
		const int octaves = GetIntParameters(props, propName + ".octaves", 1, "8").at(0);
		const float omega = GetFloatParameters(props, propName + ".roughness", 1, "0.5").at(0);

		return new FBMTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega);
	} else if (texType == "marble") {
		const int octaves = GetIntParameters(props, propName + ".octaves", 1, "8").at(0);
		const float omega = GetFloatParameters(props, propName + ".roughness", 1, "0.5").at(0);
		const float scale = GetFloatParameters(props, propName + ".scale", 1, "1.0").at(0);
		const float variation = GetFloatParameters(props, propName + ".variation", 1, "0.2").at(0);

		return new MarbleTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega, scale, variation);
	} else if (texType == "dots") {
		const std::string insideTexName = GetStringParameters(props, propName + ".inside", 1, "1.0").at(0);
		const Texture *insideTex = GetTexture(insideTexName);
		const std::string outsideTexName = GetStringParameters(props, propName + ".outside", 1, "0.0").at(0);
		const Texture *outsideTex = GetTexture(outsideTexName);

		return new DotsTexture(CreateTextureMapping2D(propName + ".mapping", props), insideTex, outsideTex);
	} else if (texType == "brick") {
		const std::string tex1Name = GetStringParameters(props, propName + ".bricktex", 1, "1.0 1.0 1.0").at(0);
		const Texture *tex1 = GetTexture(tex1Name);
		const std::string tex2Name = GetStringParameters(props, propName + ".mortartex", 1, "0.2 0.2 0.2").at(0);
		const Texture *tex2 = GetTexture(tex2Name);
		const std::string tex3Name = GetStringParameters(props, propName + ".brickmodtex", 1, "1.0 1.0 1.0").at(0);
		const Texture *tex3 = GetTexture(tex3Name);

		const std::string brickbond = GetStringParameters(props, propName + ".brickbond", 1, "running").at(0);
		const float brickwidth = GetFloatParameters(props, propName + ".brickwidth", 1, "0.3").at(0);
		const float brickheight = GetFloatParameters(props, propName + ".brickheight", 1, "0.1").at(0);
		const float brickdepth = GetFloatParameters(props, propName + ".brickdepth", 1, "0.15").at(0);
		const float mortarsize = GetFloatParameters(props, propName + ".mortarsize", 1, "0.01").at(0);
		const float brickrun = GetFloatParameters(props, propName + ".brickrun", 1, "0.75").at(0);
		const float brickbevel = GetFloatParameters(props, propName + ".brickbevel", 1, "0.0").at(0);

		return new BrickTexture(CreateTextureMapping3D(propName + ".mapping", props), tex1, tex2, tex3,
				brickwidth, brickheight, brickdepth, mortarsize, brickrun, brickbevel, brickbond);
	} else if (texType == "add") {
		const std::string tex1Name = GetStringParameters(props, propName + ".texture1", 1, "1.0").at(0);
		const Texture *tex1 = GetTexture(tex1Name);
		const std::string tex2Name = GetStringParameters(props, propName + ".texture2", 1, "1.0").at(0);
		const Texture *tex2 = GetTexture(tex2Name);
		return new AddTexture(tex1, tex2);
	} else if (texType == "windy") {
		return new WindyTexture(CreateTextureMapping3D(propName + ".mapping", props));
	} else if (texType == "wrinkled") {
		const int octaves = GetIntParameters(props, propName + ".octaves", 1, "8").at(0);
		const float omega = GetFloatParameters(props, propName + ".roughness", 1, "0.5").at(0);

		return new WrinkledTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega);
	} else if (texType == "uv") {
		return new UVTexture(CreateTextureMapping2D(propName + ".mapping", props));
	} else if (texType == "band") {
		const std::string amtName = GetStringParameters(props, propName + ".amount", 1, "0.5").at(0);
		const Texture *amtTex = GetTexture(amtName);

		vector<float> offsets;
		vector<Spectrum> values;
		for (u_int i = 0; props.IsDefined(propName + ".offset" + ToString(i)); ++i) {
			const float offset = GetFloatParameters(props, propName + ".offset" + ToString(i), 1, "0.0").at(0);
			const std::vector<float> v = GetFloatParameters(props, propName + ".value" + ToString(i), 3, "1.0 1.0 1.0");
			const Spectrum value(v.at(0), v.at(1), v.at(2));

			offsets.push_back(offset);
			values.push_back(value);
		}
		if (offsets.size() == 0)
			throw std::runtime_error("Empty Band texture: " + texName);

		return new BandTexture(amtTex, offsets, values);
	} else if (texType == "hitpointcolor") {
		return new HitPointColorTexture();
	} else if (texType == "hitpointalpha") {
		return new HitPointAlphaTexture();
	} else if (texType == "hitpointgrey") {
		const int channel = GetIntParameters(props, propName + ".channel", 1, "-1").at(0);

		return new HitPointGreyTexture(((channel != 0) && (channel != 1) && (channel != 2)) ? 
			std::numeric_limits<u_int>::max() : static_cast<u_int>(channel));
	} else
		throw std::runtime_error("Unknown texture type: " + texType);
}

Texture *Scene::GetTexture(const std::string &name) {
	if (texDefs.IsTextureDefined(name))
		return texDefs.GetTexture(name);
	else {
		// Check if it is an implicit declaration of a constant texture
		try {
			std::vector<std::string> strs;
			boost::split(strs, name, boost::is_any_of("\t "));

			std::vector<float> floats;
			for (std::vector<std::string>::iterator it = strs.begin(); it != strs.end(); ++it) {
				if (it->length() != 0) {
					const double f = boost::lexical_cast<double>(*it);
					floats.push_back(static_cast<float>(f));
				}
			}

			if (floats.size() == 1) {
				ConstFloatTexture *tex = new ConstFloatTexture(floats.at(0));
				texDefs.DefineTexture("Implicit-ConstFloatTexture-" + boost::lexical_cast<std::string>(tex), tex);

				return tex;
			} else if (floats.size() == 3) {
				ConstFloat3Texture *tex = new ConstFloat3Texture(Spectrum(floats.at(0), floats.at(1), floats.at(2)));
				texDefs.DefineTexture("Implicit-ConstFloatTexture3-" + boost::lexical_cast<std::string>(tex), tex);

				return tex;
			} else
				throw std::runtime_error("Wrong number of arguments in the implicit definition of a constant texture: " +
						boost::lexical_cast<std::string>(floats.size()));
		} catch (boost::bad_lexical_cast) {
			throw std::runtime_error("Syntax error in texture name: " + name);
		}
	}
}

Material *Scene::CreateMaterial(const std::string &matName, const Properties &props) {
	const std::string propName = "scene.materials." + matName;
	const std::string matType = GetStringParameters(props, propName + ".type", 1, "matte").at(0);

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
			throw std::runtime_error("There is a loop in Mix material definition: " + matName);

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
			const std::string type = props.GetString(propName + ".preset", "aluminium");

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
				throw std::runtime_error("Unknown Metal2 preset: " + type);
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
		throw std::runtime_error("Unknown material type: " + matType);

	mat->SetEmittedSamples(Max(0, props.GetInt(propName + ".emission.samples", 1)));
	mat->SetIndirectDiffuseVisibility(props.GetBoolean(propName + ".visibility.indirect.diffuse.enable", true));
	mat->SetIndirectGlossyVisibility(props.GetBoolean(propName + ".visibility.indirect.glossy.enable", true));

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

	// Find the light index
	std::map<const LightSource *, u_int>::const_iterator it = light2Index.find(light);

	return lightsDistribution->Pdf(it->second);
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
