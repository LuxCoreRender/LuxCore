/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_SCENEOBJECTDEFS_H
#define	_SLG_SCENEOBJECTDEFS_H

#include <string>
#include <vector>

#include "luxrays/core/namedobjectvector.h"
#include "slg/scene/sceneobject.h"
#include "slg/usings.h"

namespace slg {

//------------------------------------------------------------------------------
// SceneObjectDefinitions
//------------------------------------------------------------------------------

class SceneObjectDefinitions {
public:
	SceneObjectDefinitions() { }
	~SceneObjectDefinitions() { }

	bool IsSceneObjectDefined(const std::string &name) const {
		return objs.IsObjDefined(name);
	}
	std::tuple<SceneObject&, SceneObjectUPtr> DefineSceneObject(SceneObjectUPtr&& m);
	void DefineIntersectableLights(LightSourceDefinitions &lightDefs, MaterialConstRef newMat) const;
	void DefineIntersectableLights(LightSourceDefinitions &lightDefs, SceneObjectConstRef obj) const;

	SceneObjectConstRef GetSceneObject(const std::string &name) const {
		return dynamic_cast<SceneObjectConstRef>(objs.GetObj(name));
	}
	SceneObjectRef GetSceneObject(const std::string &name) {
		return dynamic_cast<SceneObjectRef>(objs.GetObj(name));
	}
	SceneObjectConstRef GetSceneObject(const u_int index) const {
		return dynamic_cast<SceneObjectConstRef>(objs.GetObj(index));
	}
	SceneObjectRef GetSceneObject(const u_int index) {
		return dynamic_cast<SceneObjectRef>(objs.GetObj(index));
	}
	u_int GetSceneObjectIndex(const std::string &name) const {
		return objs.GetIndex(name);
	}
	u_int GetSceneObjectIndex(SceneObjectConstRef so) const {
		return objs.GetIndex(so);
	}

	u_int GetSize() const {
		return objs.GetSize();
	}
	auto GetSceneObjectNames() const {
		return objs.GetNames();
	}

	// Update any reference to oldMat with newMat
	void UpdateMaterialReferences(MaterialConstRef oldMat, MaterialRef newMat);
	// Update any reference to oldMesh with newMesh. It returns also the
	// list of modified objects
	void UpdateMeshReferences(
		luxrays::ExtMeshConstRef oldMesh,
		luxrays::ExtMeshRef newMesh,
		std::unordered_set<const SceneObject *> &modifiedObjsList
	);

	SceneObjectUPtr DeleteSceneObject(const std::string &name) {
		return objs.DeleteObj<SceneObject>(name);
	}

	void DeleteSceneObjects(const std::vector<std::string> &names) {
		objs.DeleteObjs(names);
	}
 
	friend class slg::ExtMeshCache;

private:
	luxrays::NamedObjectVector objs;

	// mapping mesh.name -> scene object list using it
	std::unordered_multimap<std::string, std::string> meshToSceneObjects;
};

}

#endif	/* _SLG_SCENEOBJECTDEFS_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
