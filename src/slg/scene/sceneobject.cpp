/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#include <boost/format.hpp>

#include "slg/scene/scene.h"
#include "slg/lights/trianglelight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SceneObject
//------------------------------------------------------------------------------

void SceneObject::AddReferencedMeshes(boost::unordered_set<const luxrays::ExtMesh *> &referencedMesh) const {
	referencedMesh.insert(mesh);

	// Check if it is an instance and add referenced mesh
	if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
		ExtInstanceTriangleMesh *imesh = (ExtInstanceTriangleMesh *)mesh;
		referencedMesh.insert(imesh->GetExtTriangleMesh());
	}	
}

void SceneObject::UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
	if (mat == oldMat)
		mat = newMat;
}

bool SceneObject::UpdateMeshReference(const luxrays::ExtMesh *oldMesh, luxrays::ExtMesh *newMesh) {
	if (mesh == oldMesh) {
		mesh = newMesh;
		return true;
	} else
		return false;
}

Properties SceneObject::ToProperties(const ExtMeshCache &extMeshCache) const {
	Properties props;

	const std::string name = GetName();
    props.Set(Property("scene.objects." + name + ".material")(mat->GetName()));

    u_int meshIndex;
	if (mesh->GetType() == TYPE_EXT_TRIANGLE_MOTION) {
		const ExtMotionTriangleMesh *mot = (const ExtMotionTriangleMesh *)mesh;
		props.Set(mot->GetMotionSystem().ToProperties("scene.objects." + name + "."));
        meshIndex = extMeshCache.GetExtMeshIndex(mot->GetExtTriangleMesh());		
	} else if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
		const ExtInstanceTriangleMesh *inst = (const ExtInstanceTriangleMesh *)mesh;
		props << Property("scene.objects." + name + ".transformation")(inst->GetTransformation().m);
        meshIndex = extMeshCache.GetExtMeshIndex(inst->GetExtTriangleMesh());
    } else
        meshIndex = extMeshCache.GetExtMeshIndex(mesh);
	props.Set(Property("scene.objects." + name + ".ply")(
			"mesh-" + (boost::format("%05d") % meshIndex).str() + ".ply"));

	return props;
}

//------------------------------------------------------------------------------
// SceneObjectDefinitions
//------------------------------------------------------------------------------

SceneObjectDefinitions::SceneObjectDefinitions() { }

SceneObjectDefinitions::~SceneObjectDefinitions() {
	BOOST_FOREACH(SceneObject *o, objs)
		delete o;
}

void SceneObjectDefinitions::DefineSceneObject(const std::string &name, SceneObject *newObj) {
	if (IsSceneObjectDefined(name)) {
		const SceneObject *oldObj = GetSceneObject(name);

		// Update name/SceneObject definition
		const u_int index = GetSceneObjectIndex(name);
		objs[index] = newObj;
		objsByName.erase(name);
		objsByName.insert(std::make_pair(name, newObj));

		// Delete old SceneObject
		delete oldObj;
	} else {
		// Add the new SceneObject
		objs.push_back(newObj);
		objsByName.insert(std::make_pair(name, newObj));
	}
}

void SceneObjectDefinitions::DefineIntersectableLights(LightSourceDefinitions &lightDefs,
		const Material *mat) const {
	for (u_int i = 0; i < objs.size(); ++i) {
		if (objs[i]->GetMaterial() == mat)
			DefineIntersectableLights(lightDefs, objs[i]);
	}
}

void SceneObjectDefinitions::DefineIntersectableLights(LightSourceDefinitions &lightDefs,
		const SceneObject *obj) const {
	const ExtMesh *mesh = obj->GetExtMesh();

	// Add all new triangle lights
	for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i) {
		TriangleLight *tl = new TriangleLight();
		tl->lightMaterial = obj->GetMaterial();
		tl->mesh = mesh;
		tl->triangleIndex = i;
		tl->Preprocess();

		lightDefs.DefineLightSource(obj->GetName() + TRIANGLE_LIGHT_POSTFIX + ToString(i), tl);
	}
}

const SceneObject *SceneObjectDefinitions::GetSceneObject(const std::string &name) const {
	// Check if the SceneObject has been already defined
	boost::unordered_map<std::string, SceneObject *>::const_iterator it = objsByName.find(name);

	if (it == objsByName.end())
		throw std::runtime_error("Reference to an undefined SceneObject: " + name);
	else
		return it->second;
}

SceneObject *SceneObjectDefinitions::GetSceneObject(const std::string &name) {
	// Check if the SceneObject has been already defined
	boost::unordered_map<std::string, SceneObject *>::const_iterator it = objsByName.find(name);

	if (it == objsByName.end())
		throw std::runtime_error("Reference to an undefined SceneObject: " + name);
	else
		return it->second;
}

u_int SceneObjectDefinitions::GetSceneObjectIndex(const std::string &name) const {
	return GetSceneObjectIndex(GetSceneObject(name));
}

u_int SceneObjectDefinitions::GetSceneObjectIndex(const SceneObject *m) const {
	for (u_int i = 0; i < objs.size(); ++i) {
		if (m == objs[i])
			return i;
	}

	throw std::runtime_error("Reference to an undefined SceneObject: " + boost::lexical_cast<std::string>(m));
}

u_int SceneObjectDefinitions::GetSceneObjectIndex(const ExtMesh *mesh) const {
	for (u_int i = 0; i < objs.size(); ++i) {
		if (mesh == objs[i]->GetExtMesh())
			return i;
	}

	throw std::runtime_error("Reference to an undefined ExtMesh in a SceneObject: " + boost::lexical_cast<std::string>(mesh));
}

std::vector<std::string> SceneObjectDefinitions::GetSceneObjectNames() const {
	std::vector<std::string> names;
	names.reserve(objs.size());
	for (boost::unordered_map<std::string, SceneObject *>::const_iterator it = objsByName.begin(); it != objsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void SceneObjectDefinitions::UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
	// Replace old material direct references with new ones
	BOOST_FOREACH(SceneObject *o, objs)
		o->UpdateMaterialReferences(oldMat, newMat);
}

void SceneObjectDefinitions::UpdateMeshReferences(const ExtMesh *oldMesh, ExtMesh *newMesh,
		boost::unordered_set<SceneObject *> &modifiedObjsList) {
	BOOST_FOREACH(SceneObject *o, objs) {
		if (o->UpdateMeshReference(oldMesh, newMesh))
			modifiedObjsList.insert(o);
	}
}

void SceneObjectDefinitions::DeleteSceneObject(const std::string &name) {
	const u_int index = GetSceneObjectIndex(name);
	objs.erase(objs.begin() + index);
	objsByName.erase(name);
}
