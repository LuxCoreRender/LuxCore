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

#include "luxrays/core/dataset.h"
#include "luxrays/core/intersectiondevice.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void Scene::UpdateObjectTransformation(const string &objName, const Transform &trans) {
	if (!objDefs.IsSceneObjectDefined(objName))
		throw runtime_error("Unknown object in Scene::UpdateObjectTransformation(): " + objName);

	SceneObject *obj = objDefs.GetSceneObject(objName);
	ExtMesh *mesh = obj->GetExtMesh();

	ExtInstanceTriangleMesh *instanceMesh = dynamic_cast<ExtInstanceTriangleMesh *>(mesh);
	if (instanceMesh) {
		instanceMesh->SetTransformation(trans);
		editActions.AddAction(GEOMETRY_TRANS_EDIT);
	} else {
		mesh->ApplyTransform(trans);
		editActions.AddAction(GEOMETRY_EDIT);
	}

	// Check if it is a light source
	if (obj->GetMaterial()->IsLightSource()) {
		// Have to update all light sources using this mesh
		const string prefix = Scene::EncodeTriangleLightNamePrefix(obj->GetName());
		for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i)
			lightDefs.GetLightSource(prefix + ToString(i))->Preprocess();

		editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
	}
}

void Scene::UpdateObjectMaterial(const string &objName, const string &matName) {
	if (!objDefs.IsSceneObjectDefined(objName))
		throw runtime_error("Unknown object in Scene::UpdateObjectMaterial(): " + objName);
	if (!matDefs.IsMaterialDefined(matName))
		throw runtime_error("Unknown material in Scene::UpdateObjectMaterial(): " + matName);

	SceneObject *obj = objDefs.GetSceneObject(objName);

	// Check if the object is a light source
	if (obj->GetMaterial()->IsLightSource()) {
		// Delete all old triangle lights
		lightDefs.DeleteLightSourceStartWith(Scene::EncodeTriangleLightNamePrefix(obj->GetName()));

		editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
	}
	
	// Get the material
	const Material *mat = matDefs.GetMaterial(matName);
	obj->SetMaterial(mat);
	
	// Check if the object is now a light source
	if (mat->IsLightSource()) {
		SDL_LOG("The " << objName << " object is a light sources with " << obj->GetExtMesh()->GetTotalTriangleCount() << " triangles");

		objDefs.DefineIntersectableLights(lightDefs, obj);

		editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
	}

	editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
}
