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

#ifndef _SLG_SCENEOBJECTDEFS_H
#define	_SLG_SCENEOBJECTDEFS_H

#include <string>
#include <vector>

#include <boost/unordered_map.hpp>

#include "slg/scene/sceneobject.h"

namespace slg {

//------------------------------------------------------------------------------
// SceneObjectDefinitions
//------------------------------------------------------------------------------

class SceneObjectDefinitions {
public:
	SceneObjectDefinitions();
	~SceneObjectDefinitions();

	bool IsSceneObjectDefined(const std::string &name) const {
		return (objsByName.count(name) > 0);
	}
	void DefineSceneObject(SceneObject *m);
	void DefineIntersectableLights(LightSourceDefinitions &lightDefs, const Material *newMat) const;
	void DefineIntersectableLights(LightSourceDefinitions &lightDefs, const SceneObject *obj) const;

	const SceneObject *GetSceneObject(const std::string &name) const;
	SceneObject *GetSceneObject(const std::string &name);
	const SceneObject *GetSceneObject(const u_int index) const {
		return objs[index];
	}
	SceneObject *GetSceneObject(const u_int index) {
		return objs[index];
	}
	u_int GetSceneObjectIndex(const std::string &name) const;
	u_int GetSceneObjectIndex(const SceneObject *m) const;
	u_int GetSceneObjectIndex(const luxrays::ExtMesh *mesh) const;

	u_int GetSize() const { return static_cast<u_int>(objs.size()); }
	std::vector<std::string> GetSceneObjectNames() const;

	// Update any reference to oldMat with newMat
	void UpdateMaterialReferences(const Material *oldMat, const Material *newMat);
	// Update any reference to oldMesh with newMesh. It returns also the
	// list of modified objects
	void UpdateMeshReferences(const luxrays::ExtMesh *oldMesh, luxrays::ExtMesh *newMesh,
		boost::unordered_set<SceneObject *> &modifiedObjsList);

	void DeleteSceneObject(const std::string &name);
  
private:
	std::vector<SceneObject *> objs;
	boost::unordered_map<std::string, SceneObject *> objsByName;
};

}

#endif	/* _SLG_SCENEOBJECTDEFS_H */
