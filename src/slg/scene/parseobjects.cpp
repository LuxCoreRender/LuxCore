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

#include "slg/scene/scene.h"
#include "slg/utils/filenameresolver.h"

using namespace std;
using namespace luxrays;
using namespace slg;

namespace slg {
atomic<u_int> defaultObjectIDIndex(0);
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

		if (objDefs.IsSceneObjectDefined(objName)) {
			// A replacement for an existing object
			const SceneObject *oldObj = objDefs.GetSceneObject(objName);
			const bool wasLightSource = oldObj->GetMaterial()->IsLightSource();

			// Check if the old object was a light source
			if (wasLightSource) {
				editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);

				// Delete all old triangle lights
				lightDefs.DeleteLightSourceStartWith(Scene::EncodeTriangleLightNamePrefix(oldObj->GetName()));
			}
		}

		// In order to have harlequin colors with OBJECT_ID output
		const u_int index = defaultObjectIDIndex++;
		const u_int objID = ((u_int)(RadicalInverse(index + 1, 2) * 255.f + .5f)) |
				(((u_int)(RadicalInverse(index + 1, 3) * 255.f + .5f)) << 8) |
				(((u_int)(RadicalInverse(index + 1, 5) * 255.f + .5f)) << 16);
		SceneObject *obj = CreateObject(objID, objName, props);
		objDefs.DefineSceneObject(obj);

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
	string shapeName;
	if (props.IsDefined(propName + ".ply")) {
		// For compatibility with the past SDL syntax
		shapeName = props.Get(Property(propName + ".ply")("")).Get<string>();

		if (!extMeshCache.IsExtMeshDefined(shapeName)) {
			// It is a mesh to define
			ExtTriangleMesh *mesh = ExtTriangleMesh::Load(SLG_FileNameResolver.ResolveFile(shapeName));
			mesh->SetName(shapeName);

			const Matrix4x4 mat = props.Get(Property(propName +
				".appliedtransformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
			mesh->SetLocal2World(Transform(mat));

			DefineMesh(mesh);
		}
	} else if (props.IsDefined(propName + ".vertices")) {
		// For compatibility with the past SDL syntax
		shapeName = "InlinedMesh-" + objName;

		if (!extMeshCache.IsExtMeshDefined(shapeName)) {
			// It is a mesh to define
			ExtMesh *mesh = CreateInlinedMesh(shapeName, propName, props);
			mesh->SetName(shapeName);
			DefineMesh(mesh);
		}
	} else if (props.IsDefined(propName + ".shape")) {
		shapeName = props.Get(Property(propName + ".shape")("")).Get<string>();

		if (!extMeshCache.IsExtMeshDefined(shapeName))
			throw runtime_error("Unknown shape: " + shapeName);
	} else
		throw runtime_error("Missing shape in object definition: " + objName);

	// Check if I have to use a motion mesh, instance mesh or normal mesh
	ExtMesh *mesh;
	if (props.IsDefined(propName + ".motion.0.time")) {
		// Build the motion system
		vector<float> times;
		vector<Transform> transforms;
		for (u_int i = 0;; ++i) {
			const string prefix = propName + ".motion." + ToString(i);
			if (!props.IsDefined(prefix +".time"))
				break;

			const float t = props.Get(prefix +".time").Get<float>();
			if (i > 0 && t <= times.back())
				throw runtime_error(objName + " motion time must be monotonic");
			times.push_back(t);

			const Matrix4x4 mat = props.Get(Property(prefix +
				".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
			// NOTE: Transform for MotionSystem are global2local for scene objects
			// and not local2global for camera
			transforms.push_back(Inverse(Transform(mat)));
		}

		MotionSystem ms(times, transforms);
		const string motionShapeName = "MotionMesh-" + objName;
		DefineMesh(motionShapeName, shapeName, ms);

		mesh = extMeshCache.GetExtMesh(motionShapeName);
	} else if (props.IsDefined(propName + ".transformation")) {
		const Matrix4x4 mat = props.Get(Property(propName +
			".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();

		const string InstanceShapeName = "InstanceMesh-" + objName;
		DefineMesh(InstanceShapeName, shapeName, Transform(mat));

		mesh = extMeshCache.GetExtMesh(InstanceShapeName);
	} else
		mesh = extMeshCache.GetExtMesh(shapeName);

	const u_int objID = props.Get(Property(propName + ".id")(defaultObjID)).Get<u_int>();
	const bool cameraInvisible = props.Get(Property(propName + ".camerainvisible")(false)).Get<bool>();

	// Build the scene object
	SceneObject *scnObj = new SceneObject(mesh, mat, objID, cameraInvisible);
	scnObj->SetName(objName);

	if (props.IsDefined(propName + ".bake.combined.file")) {
		ImageMap *imgMap = ImageMap::FromProperties(props, propName + ".bake.combined");

		// Add the image map to the cache
		const string name ="LUXCORE_BAKEMAP_COMBINED_" + propName;
		imgMap->SetName(name);
		imgMapCache.DefineImageMap(imgMap, false);

		const u_int uvIndex = Clamp(props.Get(Property(propName + ".bake.combined.uvindex")(0)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		scnObj->SetBakeMap(imgMap, COMBINED, uvIndex);
	} else if (props.IsDefined(propName + ".bake.lightmap.file")) {
		ImageMap *imgMap = ImageMap::FromProperties(props, propName + ".bake.lightmap");

		// Add the image map to the cache
		const string name ="LUXCORE_BAKEMAP_LIGHTMAP_" + propName;
		imgMap->SetName(name);
		imgMapCache.DefineImageMap(imgMap, false);

		const u_int uvIndex = Clamp(props.Get(Property(propName + ".bake.lightmap.uvindex")(0)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		scnObj->SetBakeMap(imgMap, LIGHTMAP, uvIndex);
	}

	return scnObj;
}

void Scene::DuplicateObject(const std::string &srcObjName, const std::string &dstObjName,
		const luxrays::Transform &trans, const u_int dstObjID) {
	const SceneObject *srcObj = objDefs.GetSceneObject(srcObjName);

	// Check the type of mesh
	ExtMesh *newMesh;
	const ExtMesh *srcMesh = srcObj->GetExtMesh();
	switch (srcMesh->GetType()) {
		case TYPE_EXT_TRIANGLE: {
			// Create an instance of the mesh
			const string instanceShapeName = "InstanceMesh-" + dstObjName;
			DefineMesh(instanceShapeName, srcMesh->GetName(), trans);

			newMesh = extMeshCache.GetExtMesh(instanceShapeName);
			break;
		}
		case TYPE_EXT_TRIANGLE_INSTANCE: {
			// Get the instanced mesh
			const ExtInstanceTriangleMesh *srcInstanceMesh = static_cast<const ExtInstanceTriangleMesh *>(srcMesh);
			const ExtTriangleMesh *baseMesh = static_cast<const ExtTriangleMesh *>(srcInstanceMesh->GetTriangleMesh());

			// Create the new instance of the base mesh
			const string instanceShapeName = "InstanceMesh-" + dstObjName;
			DefineMesh(instanceShapeName, baseMesh->GetName(), trans);

			newMesh = extMeshCache.GetExtMesh(instanceShapeName);
			break;
		}
		case TYPE_EXT_TRIANGLE_MOTION: {
			// Get the motion mesh
			const ExtMotionTriangleMesh *srcMotionMesh = static_cast<const ExtMotionTriangleMesh *>(srcMesh);
			const ExtTriangleMesh *baseMesh = static_cast<const ExtTriangleMesh *>(srcMotionMesh->GetTriangleMesh());

			// Create the new instance of the base mesh
			const string instanceShapeName = "InstanceMesh-" + dstObjName;
			DefineMesh(instanceShapeName, baseMesh->GetName(), trans);

			newMesh = extMeshCache.GetExtMesh(instanceShapeName);
			break;
		}
		default:
			throw runtime_error("Unknown mesh type in Scene::DuplicateObject(): " + ToString(srcMesh->GetType()));
	}

	// If the Null index was passed as ID, copy the ID of the source object
	const u_int objID = (dstObjID == 0xffffffff) ? srcObj->GetID() : dstObjID;

	SceneObject *dstObj = new SceneObject(newMesh, srcObj->GetMaterial(), objID,
			srcObj->IsCameraInvisible());
	dstObj->SetName(dstObjName);
	objDefs.DefineSceneObject(dstObj);

	// Check if it is a light source
	const Material *mat = dstObj->GetMaterial();
	if (mat->IsLightSource()) {
		SDL_LOG("The " << dstObjName << " object is a light sources with " << dstObj->GetExtMesh()->GetTotalTriangleCount() << " triangles");

		objDefs.DefineIntersectableLights(lightDefs, dstObj);
	}

	editActions.AddActions(GEOMETRY_EDIT);
}

void Scene::DuplicateObject(const std::string &srcObjName, const std::string &dstObjName,
		const MotionSystem &ms, const u_int dstObjID) {
	const SceneObject *srcObj = objDefs.GetSceneObject(srcObjName);

	// Check the type of mesh
	ExtMesh *newMesh;
	const ExtMesh *srcMesh = srcObj->GetExtMesh();
	switch (srcMesh->GetType()) {
		case TYPE_EXT_TRIANGLE: {
			// Create an instance of the mesh
			const string motionShapeName = "MotionMesh-" + dstObjName;
			DefineMesh(motionShapeName, srcMesh->GetName(), ms);

			newMesh = extMeshCache.GetExtMesh(motionShapeName);
			break;
		}
		case TYPE_EXT_TRIANGLE_INSTANCE: {
			// Get the instanced mesh
			const ExtInstanceTriangleMesh *srcInstanceMesh = static_cast<const ExtInstanceTriangleMesh *>(srcMesh);
			const ExtTriangleMesh *baseMesh = static_cast<const ExtTriangleMesh *>(srcInstanceMesh->GetTriangleMesh());

			// Create the new instance of the base mesh
			const string motionShapeName = "MotionMesh-" + dstObjName;
			DefineMesh(motionShapeName, baseMesh->GetName(), ms);

			newMesh = extMeshCache.GetExtMesh(motionShapeName);
			break;
		}
		case TYPE_EXT_TRIANGLE_MOTION: {
			// Get the motion mesh
			const ExtMotionTriangleMesh *srcMotionMesh = static_cast<const ExtMotionTriangleMesh *>(srcMesh);
			const ExtTriangleMesh *baseMesh = static_cast<const ExtTriangleMesh *>(srcMotionMesh->GetTriangleMesh());

			// Create the new instance of the base mesh
			const string motionShapeName = "MotionMesh-" + dstObjName;
			DefineMesh(motionShapeName, baseMesh->GetName(), ms);

			newMesh = extMeshCache.GetExtMesh(motionShapeName);
			break;
		}
		default:
			throw runtime_error("Unknown mesh type in Scene::DuplicateObject(): " + ToString(srcMesh->GetType()));
	}

	// If the Null index was passed as ID, copy the ID of the source object
	const u_int objID = (dstObjID == 0xffffffff) ? srcObj->GetID() : dstObjID;

	SceneObject *dstObj = new SceneObject(newMesh, srcObj->GetMaterial(), objID,
			srcObj->IsCameraInvisible());
	dstObj->SetName(dstObjName);
	objDefs.DefineSceneObject(dstObj);

	// Check if it is a light source
	const Material *mat = dstObj->GetMaterial();
	if (mat->IsLightSource()) {
		SDL_LOG("The " << dstObjName << " object is a light sources with " << dstObj->GetExtMesh()->GetTotalTriangleCount() << " triangles");

		objDefs.DefineIntersectableLights(lightDefs, dstObj);
	}

	editActions.AddActions(GEOMETRY_EDIT);
}
