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

#include "luxrays/usings.h"
#include "slg/scene/scene.h"
#include "slg/lights/trianglelight.h"
#include "slg/usings.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SceneObjectDefinitions
//------------------------------------------------------------------------------

std::tuple<SceneObject&, SceneObjectUPtr>
SceneObjectDefinitions::DefineSceneObject(SceneObjectUPtr&& newObj) {

	meshToSceneObjects.insert(
		make_pair(newObj->GetExtMesh().GetName() , newObj->GetName())
	);

	return objs.DefineObj<SceneObject>(std::move(newObj));
}

void SceneObjectDefinitions::DefineIntersectableLights(
	LightSourceDefinitions &lightDefs,
	MaterialConstRef mat) const
{
	const u_int size = objs.GetSize();

	for (u_int i = 0; i < size; ++i) {
		auto& so = static_cast<const SceneObject &>(objs.GetObj(i));

		if (so.GetMaterial() == mat)
			DefineIntersectableLights(lightDefs, so);
	}
}

void SceneObjectDefinitions::DefineIntersectableLights
	(LightSourceDefinitions &lightDefs,
	SceneObjectConstRef obj
) const {
	auto& mesh = obj.GetExtMesh();

	// Add all new triangle lights

	const string prefix = Scene::EncodeTriangleLightNamePrefix(obj.GetName());
	for (u_int i = 0; i < mesh.GetTotalTriangleCount(); ++i) {
		auto tl = std::make_unique<TriangleLight>();

		// I use here boost::lexical_cast instead of ToString() because it is a
		// lot faster and there can not be locale related problems with integers
		//tl->SetName(prefix + ToString(i));
		tl->SetName(prefix + boost::lexical_cast<string>(i));

		tl->lightMaterial = &obj.GetMaterial();
		tl->volume = tl->lightMaterial->GetExteriorVolume();
		tl->autoVolume = !tl->volume;
		tl->sceneObject = &obj;
		// This is initialized in LightSourceDefinitions::Preprocess()
		tl->meshIndex = NULL_INDEX;
		tl->triangleIndex = i;
		tl->Preprocess();

		lightDefs.DefineLightSource(std::move(tl));
	}
}

void SceneObjectDefinitions::UpdateMaterialReferences(MaterialConstRef oldMat, MaterialRef newMat) {
	// Replace old material direct references with new ones
	for (NamedObjectRef o : objs.GetObjs())
		dynamic_cast<SceneObjectRef>(o).UpdateMaterialReferences(oldMat, newMat);
}

void SceneObjectDefinitions::UpdateMeshReferences(
	ExtMeshConstRef oldMesh,
	ExtMeshRef newMesh,
	std::unordered_set<const SceneObject *>& modifiedObjsList
) {

	auto p = meshToSceneObjects.equal_range(oldMesh.GetName());
	auto it = p.first;

	while(it != p.second)
	{
		bool updated = false;
		std::string soName = it->second;
		if (objs.IsObjDefined(soName))
		{
			auto& so = static_cast<SceneObject&>(objs.GetObj(soName));
			if (so.UpdateMeshReference(oldMesh, newMesh))
			{
				updated = true;
				modifiedObjsList.insert(&so);
				meshToSceneObjects.erase(it++);

				// change index
				meshToSceneObjects.insert(make_pair(newMesh.GetName(), so.GetName()));
			}
		}
		if(!updated) ++it;
	}
}


// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
