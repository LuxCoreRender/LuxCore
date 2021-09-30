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

#include <boost/lexical_cast.hpp>

#include "slg/scene/scene.h"
#include "slg/lights/trianglelight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SceneObjectDefinitions
//------------------------------------------------------------------------------

void SceneObjectDefinitions::DefineSceneObject(SceneObject *newObj) {
	const SceneObject *oldObj = static_cast<const SceneObject *>(objs.DefineObj(newObj));

	// Delete the old object definition
	delete oldObj;
}

void SceneObjectDefinitions::DefineIntersectableLights(LightSourceDefinitions &lightDefs,
		const Material *mat) const {
	const u_int size = objs.GetSize();

	for (u_int i = 0; i < size; ++i) {
		const SceneObject *so = static_cast<const SceneObject *>(objs.GetObj(i));

		if (so->GetMaterial() == mat)
			DefineIntersectableLights(lightDefs, so);
	}
}

void SceneObjectDefinitions::DefineIntersectableLights(LightSourceDefinitions &lightDefs,
		const SceneObject *obj) const {
	const ExtMesh *mesh = obj->GetExtMesh();

	// Add all new triangle lights
	
	const string prefix = Scene::EncodeTriangleLightNamePrefix(obj->GetName());
	for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i) {
		TriangleLight *tl = new TriangleLight();
		
		// I use here boost::lexical_cast instead of ToString() because it is a
		// lot faster and there can not be locale related problems with integers
		//tl->SetName(prefix + ToString(i));
		tl->SetName(prefix + boost::lexical_cast<string>(i));

		tl->lightMaterial = obj->GetMaterial();
		tl->volume = tl->lightMaterial->GetExteriorVolume();
		tl->sceneObject = obj;
		// This is initialized in LightSourceDefinitions::Preprocess()
		tl->meshIndex = NULL_INDEX;
		tl->triangleIndex = i;
		tl->Preprocess();

		lightDefs.DefineLightSource(tl);
	}
}

void SceneObjectDefinitions::UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
	// Replace old material direct references with new ones
	for (auto o : objs.GetObjs())
		static_cast<SceneObject *>(o)->UpdateMaterialReferences(oldMat, newMat);
}

void SceneObjectDefinitions::UpdateMeshReferences(const ExtMesh *oldMesh, ExtMesh *newMesh,
		boost::unordered_set<SceneObject *> &modifiedObjsList) {
	for (auto o : objs.GetObjs()) {
		SceneObject *so = static_cast<SceneObject *>(o);

		if (so->UpdateMeshReference(oldMesh, newMesh))
			modifiedObjsList.insert(so);
	}
}
