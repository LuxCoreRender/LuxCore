/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

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

		if (objDefs.IsSceneObjectDefined(objName)) {
			// A replacement for an existing object
			const SceneObject *oldObj = objDefs.GetSceneObject(objName);
			const bool wasLightSource = oldObj->GetMaterial()->IsLightSource();

			// Check if the old object was a light source
			if (wasLightSource) {
				editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);

				// Delete all old triangle lights
				lightDefs.DeleteLightSourceStartWith(oldObj->GetName() + TRIANGLE_LIGHT_POSTFIX);
			}
		}

		// In order to have harlequin colors with MATERIAL_ID output
		const u_int objID = ((u_int)(RadicalInverse(objDefs.GetSize() + 1, 2) * 255.f + .5f)) |
				(((u_int)(RadicalInverse(objDefs.GetSize() + 1, 3) * 255.f + .5f)) << 8) |
				(((u_int)(RadicalInverse(objDefs.GetSize() + 1, 5) * 255.f + .5f)) << 16);
		SceneObject *obj = CreateObject(objID, objName, props);
		objDefs.DefineSceneObject(objName, obj);

		// Check if it is a light source
		const Material *mat = obj->GetMaterial();
		if (mat->IsLightSource()) {
			SDL_LOG("The " << objName << " object is a light sources with " << obj->GetExtMesh()->GetTotalTriangleCount() << " triangles");

			objDefs.DefineIntersectableLights(lightDefs, obj);
		}

		++objCount;

		const double now = WallClockTime();
		if (now - lastPrint > 2.0) {
			SDL_LOG("Scene objects count: " << objCount);
			lastPrint = now;
		}
	}
	SDL_LOG("Scene objects count: " << objCount);

	editActions.AddActions(GEOMETRY_EDIT);
}

SceneObject *Scene::CreateObject(const u_int defaultObjID, const string &objName, const Properties &props) {
	const string propName = "scene.objects." + objName;

	// Extract the material name
	const string matName = props.Get(Property(propName + ".material")("")).Get<string>();
	if (matName == "")
		throw runtime_error("Syntax error in object material reference: " + objName);

	// Get the material
	if (!matDefs.IsMaterialDefined(matName))
		throw runtime_error("Unknown material: " + matName);
	const Material *mat = matDefs.GetMaterial(matName);

	// Get the mesh
	string meshName;
	if (props.IsDefined(propName + ".ply")) {
		// For compatibility with the past SDL syntax
		meshName = props.Get(Property(propName + ".ply")("")).Get<string>();

		if (!extMeshCache.IsExtMeshDefined(meshName)) {
			// It is a mesh to define
			ExtTriangleMesh *mesh = ExtTriangleMesh::LoadExtTriangleMesh(meshName);
			extMeshCache.DefineExtMesh(meshName, mesh);
		}
	} else if (props.IsDefined(propName + ".vertices")) {
		// For compatibility with the past SDL syntax
		meshName = "InlinedMesh-" + objName;
		
		if (!extMeshCache.IsExtMeshDefined(meshName)) {
			// It is a mesh to define
			ExtMesh *mesh = CreateInlinedMesh(meshName, propName, props);
			extMeshCache.DefineExtMesh(meshName, mesh);
		}
	} else if (props.IsDefined(propName + ".shape")) {
		meshName = props.Get(Property(propName + ".shape")("")).Get<string>();

		if (!extMeshCache.IsExtMeshDefined(meshName))
			throw runtime_error("Unknown shape: " + meshName);
	} else
		throw runtime_error("Missing shape in object definition: " + objName);

	// Check if I have to use a motion mesh, instance mesh or normal mesh
	ExtMesh *mesh;
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
		extMeshCache.DefineExtMesh(objName, meshName, ms);
		mesh = extMeshCache.GetExtMesh(objName);
	} else if (props.IsDefined(propName + ".transformation")) {
		const Matrix4x4 mat = props.Get(Property(propName +
			".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();

		extMeshCache.DefineExtMesh(objName, meshName, Transform(mat));
		mesh = extMeshCache.GetExtMesh(objName);
	} else
		mesh = extMeshCache.GetExtMesh(meshName);
	
	const u_int objID = props.Get(Property(propName + ".id")(defaultObjID)).Get<u_int>();

	// Build the scene object
	SceneObject *scnObj = new SceneObject(mesh, mat, objID);
	scnObj->SetName(objName);
	
	return scnObj;
}
