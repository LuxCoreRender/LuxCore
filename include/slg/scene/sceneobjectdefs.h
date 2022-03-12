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

#ifndef _SLG_SCENEOBJECTDEFS_H
#define	_SLG_SCENEOBJECTDEFS_H

#include <string>
#include <vector>

#include "luxrays/core/namedobjectvector.h"
#include "slg/scene/sceneobject.h"

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
	void DefineSceneObject(SceneObject *m);
	void DefineIntersectableLights(LightSourceDefinitions &lightDefs, const Material *newMat) const;
	void DefineIntersectableLights(LightSourceDefinitions &lightDefs, const SceneObject *obj) const;

	const SceneObject *GetSceneObject(const std::string &name) const {
		return static_cast<const SceneObject *>(objs.GetObj(name));
	}
	SceneObject *GetSceneObject(const std::string &name) {
		std::vector<luxrays::NamedObject *> &v = objs.GetObjs();
		return static_cast<SceneObject *>(v[objs.GetIndex(name)]);
	}
	const SceneObject *GetSceneObject(const u_int index) const {
		return static_cast<const SceneObject *>(objs.GetObj(index));
	}
	u_int GetSceneObjectIndex(const std::string &name) const {
		return objs.GetIndex(name);
	}
	u_int GetSceneObjectIndex(const SceneObject *so) const {
		return objs.GetIndex(so);
	}

	u_int GetSize() const {
		return objs.GetSize();
	}
	void GetSceneObjectNames(std::vector<std::string> &names) const {
		objs.GetNames(names);
	}

	// Update any reference to oldMat with newMat
	void UpdateMaterialReferences(const Material *oldMat, const Material *newMat);
	// Update any reference to oldMesh with newMesh. It returns also the
	// list of modified objects
	void UpdateMeshReferences(const luxrays::ExtMesh *oldMesh, luxrays::ExtMesh *newMesh,
		boost::unordered_set<SceneObject *> &modifiedObjsList);

	void DeleteSceneObject(const std::string &name) {
		objs.DeleteObj(name);
	}

	void DeleteSceneObjects(const std::vector<std::string> &names) {
		objs.DeleteObjs(names);
	}
  
private:
	luxrays::NamedObjectVector objs;
};

}

#endif	/* _SLG_SCENEOBJECTDEFS_H */
