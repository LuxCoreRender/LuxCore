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

#include <cstdlib>
#include <istream>
#include <stdexcept>
#include <sstream>

#include <boost/detail/container_fwd.hpp>

#include "luxrays/core/dataset.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/sdl/scene.h"

using namespace luxrays;
using namespace luxrays::sdl;

Scene::Scene(Context *ctx, const std::string &fileName, const int accelType) {
	texMapCache = new TextureMapCache(ctx);

	LR_LOG(ctx, "Reading scene: " << fileName);

	scnProp = new Properties(fileName);

	//--------------------------------------------------------------------------
	// Read camera position and target
	//--------------------------------------------------------------------------

	std::vector<float> vf = GetParameters(*scnProp, "scene.camera.lookat", 6, "10.0 0.0 0.0  0.0 0.0 0.0");
	Point o(vf.at(0), vf.at(1), vf.at(2));
	Point t(vf.at(3), vf.at(4), vf.at(5));

	LR_LOG(ctx, "Camera postion: " << o);
	LR_LOG(ctx, "Camera target: " << t);

	vf = GetParameters(*scnProp, "scene.camera.up", 3, "0.0 0.0 0.1");
	const Vector up(vf.at(0), vf.at(1), vf.at(2));
	camera = new PerspectiveCamera(o, t, up);

	camera->lensRadius = scnProp->GetFloat("scene.camera.lensradius", 0.f);
	camera->focalDistance = scnProp->GetFloat("scene.camera.focaldistance", 10.f);
	camera->fieldOfView = scnProp->GetFloat("scene.camera.fieldofview", 45.f);

	// Check if camera motion blur is enabled
	if (scnProp->GetInt("scene.camera.motionblur.enable", 0)) {
		camera->motionBlur = true;

		vf = GetParameters(*scnProp, "scene.camera.motionblur.lookat", 6, "10.0 1.0 0.0  0.0 1.0 0.0");
		camera->mbOrig = Point(vf.at(0), vf.at(1), vf.at(2));
		camera->mbTarget = Point(vf.at(3), vf.at(4), vf.at(5));

		vf = GetParameters(*scnProp, "scene.camera.motionblur.up", 3, "0.0 0.0 0.1");
		camera->mbUp = Vector(vf.at(0), vf.at(1), vf.at(2));
	}

	//--------------------------------------------------------------------------
	// Read all materials
	//--------------------------------------------------------------------------

	std::vector<std::string> matKeys = scnProp->GetAllKeys("scene.materials.");
	if (matKeys.size() == 0)
		throw std::runtime_error("No material definition found");

	for (std::vector<std::string>::const_iterator matKey = matKeys.begin(); matKey != matKeys.end(); ++matKey) {
		const std::string &key = *matKey;
		const std::string matType = Properties::ExtractField(key, 2);
		if (matType == "")
			throw std::runtime_error("Syntax error in " + key);
		const std::string matName = Properties::ExtractField(key, 3);
		if (matName == "")
			throw std::runtime_error("Syntax error in " + key);
		LR_LOG(ctx, "Material definition: " << matName << " [" << matType << "]");

		Material *mat = CreateMaterial(key, *scnProp);

		materialIndices[matName] = materials.size();
		materials.push_back(mat);
	}

	//--------------------------------------------------------------------------
	// Read all objects .ply file
	//--------------------------------------------------------------------------

	std::vector<std::string> objKeys = scnProp->GetAllKeys("scene.objects.");
	if (objKeys.size() == 0)
		throw std::runtime_error("Unable to find object definitions");

	for (std::vector<std::string>::const_iterator objKey = objKeys.begin(); objKey != objKeys.end(); ++objKey) {
		const std::string &key = *objKey;

		// Check if it is the root of the definition of an object otherwise skip
		const size_t dot1 = key.find(".", std::string("scene.objects.").length());
		if (dot1 == std::string::npos)
			continue;
		const size_t dot2 = key.find(".", dot1 + 1);
		if (dot2 != std::string::npos)
			continue;

		// Build the object
		const std::vector<std::string> args = scnProp->GetStringVector(key, "");
		const std::string plyFileName = args.at(0);
		LR_LOG(ctx, "PLY objects file name: " << plyFileName);

		// Check if I have to calculate normal or not
		const bool usePlyNormals = (scnProp->GetInt(key + ".useplynormals", 0) != 0);
		ExtTriangleMesh *meshObject = ExtTriangleMesh::LoadExtTriangleMesh(ctx, plyFileName, usePlyNormals);
		objects.push_back(meshObject);

		// Get the material
		const std::string matName = Properties::ExtractField(key, 2);
		if (matName == "")
			throw std::runtime_error("Syntax error in material name: " + matName);
		if (materialIndices.count(matName) < 1)
			throw std::runtime_error("Unknown material: " + matName);
		Material *mat = materials[materialIndices[matName]];

		const std::string objName = Properties::ExtractField(key, 3);
		// Check if it is a light sources
		if (mat->IsLightSource()) {
			LR_LOG(ctx, "The " << objName << " objects is a light sources with " << meshObject->GetTotalTriangleCount() << " triangles");

			AreaLightMaterial *light = (AreaLightMaterial *)mat;
			for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i) {
				TriangleLight *tl = new TriangleLight(light, objects.size() - 1, i, objects);
				lights.push_back(tl);
				triangleMaterials.push_back(tl);
			}
		} else {
			SurfaceMaterial *surfMat = (SurfaceMaterial *)mat;
			for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i)
				triangleMaterials.push_back(surfMat);
		}

		// [old deprecated syntax] Check if there is a texture map associated to the object
		if (args.size() > 1) {
			// Check if the object has UV coords
			if (meshObject->GetUVs() == NULL)
				throw std::runtime_error("PLY object " + plyFileName + " is missing UV coordinates for texture mapping");

			TexMapInstance *tm = texMapCache->GetTexMapInstance(args.at(1));
			for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i) {
				triangleTexMaps.push_back(tm);
				triangleBumpMaps.push_back(NULL);
				triangleNormalMaps.push_back(NULL);
			}
		} else {
			// Check for if there is a texture map associated to the object with the new syntax
			const std::string texMap = scnProp->GetString(key + ".texmap", "");
			if (texMap != "") {
				// Check if the object has UV coords
				if (meshObject->GetUVs() == NULL)
					throw std::runtime_error("PLY object " + plyFileName + " is missing UV coordinates for texture mapping");

				TexMapInstance *tm = texMapCache->GetTexMapInstance(texMap);
				for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i)
					triangleTexMaps.push_back(tm);
			} else
				for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i)
					triangleTexMaps.push_back(NULL);

			// Check for if there is a bump map associated to the object
			const std::string bumpMap = scnProp->GetString(key + ".bumpmap", "");
			if (bumpMap != "") {
				// Check if the object has UV coords
				if (meshObject->GetUVs() == NULL)
					throw std::runtime_error("PLY object " + plyFileName + " is missing UV coordinates for bump mapping");

				const float scale = scnProp->GetFloat(key + ".bumpmap.scale", 1.f);

				BumpMapInstance *bm = texMapCache->GetBumpMapInstance(bumpMap, scale);
				for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i)
					triangleBumpMaps.push_back(bm);
			} else
				for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i)
					triangleBumpMaps.push_back(NULL);

			// Check for if there is a normal map associated to the object
			const std::string normalMap = scnProp->GetString(key + ".normalmap", "");
			if (normalMap != "") {
				// Check if the object has UV coords
				if (meshObject->GetUVs() == NULL)
					throw std::runtime_error("PLY object " + plyFileName + " is missing UV coordinates for normal mapping");

				NormalMapInstance *nm = texMapCache->GetNormalMapInstance(normalMap);
				for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i)
					triangleNormalMaps.push_back(nm);
			} else
				for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i)
					triangleNormalMaps.push_back(NULL);
		}
	}

	//--------------------------------------------------------------------------
	// Check if there is an infinitelight source defined
	//--------------------------------------------------------------------------

	const std::vector<std::string> ilParams = scnProp->GetStringVector("scene.infinitelight.file", "");
	if (ilParams.size() > 0) {
		TexMapInstance *tex = texMapCache->GetTexMapInstance(ilParams.at(0));

		// Check if I have to use InfiniteLightBF method
		if (scnProp->GetInt("scene.infinitelight.usebruteforce", 0)) {
			LR_LOG(ctx, "Using brute force infinite light sampling");
			infiniteLight = new InfiniteLightBF(tex);
			useInfiniteLightBruteForce = true;
		} else {
			if (ilParams.size() == 2)
				infiniteLight = new InfiniteLightPortal(ctx, tex, ilParams.at(1));
			else
				infiniteLight = new InfiniteLightIS(tex);

			// Add the infinite light to the list of light sources
			lights.push_back(infiniteLight);

			useInfiniteLightBruteForce = false;
		}

		std::vector<float> vf = GetParameters(*scnProp, "scene.infinitelight.gain", 3, "1.0 1.0 1.0");
		infiniteLight->SetGain(Spectrum(vf.at(0), vf.at(1), vf.at(2)));

		vf = GetParameters(*scnProp, "scene.infinitelight.shift", 2, "0.0 0.0");
		infiniteLight->SetShift(vf.at(0), vf.at(1));

		infiniteLight->Preprocess();
	} else {
		infiniteLight = NULL;
		useInfiniteLightBruteForce = false;
	}

	//--------------------------------------------------------------------------
	// Check if there is a SkyLight defined
	//--------------------------------------------------------------------------

	const std::vector<std::string> silParams = scnProp->GetStringVector("scene.skylight.dir", "");
	if (silParams.size() > 0) {
		if (infiniteLight)
			throw std::runtime_error("Can not define a skylight when there is already an infinitelight defined");

		std::vector<float> sdir = GetParameters(*scnProp, "scene.skylight.dir", 3, "");
		const float turb = scnProp->GetFloat("scene.skylight.turbidity", 2.2f);
		std::vector<float> gain = GetParameters(*scnProp, "scene.skylight.gain", 3, "1.0 1.0 1.0");

		infiniteLight = new SkyLight(turb, Vector(sdir.at(0), sdir.at(1), sdir.at(2)));
		infiniteLight->SetGain(Spectrum(gain.at(0), gain.at(1), gain.at(2)));

		useInfiniteLightBruteForce = true;
	}

	//--------------------------------------------------------------------------
	// Check if there is a SunLight defined
	//--------------------------------------------------------------------------

	const std::vector<std::string> sulParams = scnProp->GetStringVector("scene.sunlight.dir", "");
	if (sulParams.size() > 0) {
		std::vector<float> sdir = GetParameters(*scnProp, "scene.sunlight.dir", 3, "0.0 -1.0 0.0");
		const float turb = scnProp->GetFloat("scene.sunlight.turbidity", 2.2f);
		const float relSize = scnProp->GetFloat("scene.sunlight.relsize", 1.0f);
		std::vector<float> gain = GetParameters(*scnProp, "scene.sunlight.gain", 3, "1.0 1.0 1.0");

		SunLight *sunLight = new SunLight(turb, relSize, Vector(sdir.at(0), sdir.at(1), sdir.at(2)));
		sunLight->SetGain(Spectrum(gain.at(0), gain.at(1), gain.at(2)));

		lights.push_back(sunLight);
	}

	//--------------------------------------------------------------------------
	// Create the DataSet
	//--------------------------------------------------------------------------

	dataSet = new DataSet(ctx);

	// Check the type of accelerator to use
	switch (accelType) {
		case -1:
			// Use default settings
			break;
		case 0:
			dataSet->SetAcceleratorType(ACCEL_BVH);
			break;
		case 1:
		case 2:
			dataSet->SetAcceleratorType(ACCEL_QBVH);
			break;
		default:
			throw std::runtime_error("Unknown accelerator.type");
			break;
	}

	// Add all objects
	for (std::vector<ExtTriangleMesh *>::const_iterator obj = objects.begin(); obj != objects.end(); ++obj)
		dataSet->Add(*obj);

	dataSet->Preprocess();
}

Scene::~Scene() {
	delete camera;

	for (std::vector<LightSource *>::const_iterator l = lights.begin(); l != lights.end(); ++l)
		delete *l;
	if (useInfiniteLightBruteForce)
		delete infiniteLight;

	delete dataSet;

	for (std::vector<ExtTriangleMesh *>::const_iterator obj = objects.begin(); obj != objects.end(); ++obj) {
		(*obj)->Delete();
		delete *obj;
	}

	delete texMapCache;
	delete scnProp;
}

std::vector<float> Scene::GetParameters(const Properties &prop, const std::string &paramName,
		const unsigned int paramCount, const std::string &defaultValue) {
	const std::vector<float> vf = prop.GetFloatVector(paramName, defaultValue);
	if (vf.size() != paramCount) {
		std::stringstream ss;
		ss << "Syntax error in " << paramName << " (required " << paramCount << " parameters)";
		throw std::runtime_error(ss.str());
	}

	return vf;
}

Material *Scene::CreateMaterial(const std::string &propName, const Properties &prop) {
	const std::string matType = Properties::ExtractField(propName, 2);
	if (matType == "")
		throw std::runtime_error("Syntax error in " + propName);
	const std::string matName = Properties::ExtractField(propName, 3);
	if (matName == "")
		throw std::runtime_error("Syntax error in " + propName);

	if (matType == "matte") {
		const std::vector<float> vf = GetParameters(prop, propName, 3, "1.0 1.0 1.0");
		const Spectrum col(vf.at(0), vf.at(1), vf.at(2));

		return new MatteMaterial(col);
	} else if (matType == "light") {
		const std::vector<float> vf = GetParameters(prop, propName, 3, "1.0 1.0 1.0");
		const Spectrum gain(vf.at(0), vf.at(1), vf.at(2));

		return new AreaLightMaterial(gain);
	} else if (matType == "mirror") {
		const std::vector<float> vf = GetParameters(prop, propName, 4, "1.0 1.0 1.0 1.0");
		const Spectrum col(vf.at(0), vf.at(1), vf.at(2));

		return new MirrorMaterial(col, vf.at(3) != 0.f);
	} else if (matType == "mattemirror") {
		const std::vector<float> vf = GetParameters(prop, propName, 7, "1.0 1.0 1.0 1.0 1.0 1.0 1.0");
		const Spectrum Kd(vf.at(0), vf.at(1), vf.at(2));
		const Spectrum Kr(vf.at(3), vf.at(4), vf.at(5));

		return new MatteMirrorMaterial(Kd, Kr, vf.at(6) != 0.f);
	} else if (matType == "glass") {
		const std::vector<float> vf = GetParameters(prop, propName, 10, "1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.5 1.0 1.0");
		const Spectrum Krfl(vf.at(0), vf.at(1), vf.at(2));
		const Spectrum Ktrn(vf.at(3), vf.at(4), vf.at(5));

		return new GlassMaterial(Krfl, Ktrn, vf.at(6), vf.at(7), vf.at(8) != 0.f, vf.at(9) != 0.f);
	} else if (matType == "metal") {
		const std::vector<float> vf = GetParameters(prop, propName, 5, "1.0 1.0 1.0 10.0 1.0");
		const Spectrum col(vf.at(0), vf.at(1), vf.at(2));

		return new MetalMaterial(col, vf.at(3), vf.at(4) != 0.f);
	} else if (matType == "mattemetal") {
		const std::vector<float> vf = GetParameters(prop, propName, 8, "1.0 1.0 1.0 1.0 1.0 1.0 10.0 1.0");
		const Spectrum Kd(vf.at(0), vf.at(1), vf.at(2));
		const Spectrum Kr(vf.at(3), vf.at(4), vf.at(5));

		return new MatteMetalMaterial(Kd, Kr, vf.at(6), vf.at(7) != 0.f);
	} else if (matType == "archglass") {
		const std::vector<float> vf = GetParameters(prop, propName, 8, "1.0 1.0 1.0 1.0 1.0 1.0 1.0 1.0");
		const Spectrum Krfl(vf.at(0), vf.at(1), vf.at(2));
		const Spectrum Ktrn(vf.at(3), vf.at(4), vf.at(5));

		return new ArchGlassMaterial(Krfl, Ktrn, vf.at(6) != 0.f, vf.at(7) != 0.f);
	} else
		throw std::runtime_error("Unknown material type " + matType);
}
