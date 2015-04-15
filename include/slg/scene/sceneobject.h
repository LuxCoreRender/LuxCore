/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_SCENEOBJECT_H
#define	_SLG_SCENEOBJECT_H

#include <string>
#include <vector>

#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/utils/properties.h"
#include "luxrays/core/color/color.h"
#include "luxrays/utils/mc.h"
#include "slg/materials/material.h"
#include "slg/bsdf/bsdfevents.h"
#include "slg/bsdf/hitpoint.h"
#include "slg/scene/extmeshcache.h"

namespace slg {

//------------------------------------------------------------------------------
// SceneObject
//------------------------------------------------------------------------------

class SceneObject {
public:
	SceneObject(luxrays::ExtMesh *m, const Material *mt) : mesh(m), mat(mt) { }
	virtual ~SceneObject() { }

	std::string GetName() const { return "obj-" + boost::lexical_cast<std::string>(this); }

	const luxrays::ExtMesh *GetExtMesh() const { return mesh; }
	luxrays::ExtMesh *GetExtMesh() { return mesh; }
	const Material *GetMaterial() const { return mat; }

	void AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const {
		mat->AddReferencedMaterials(referencedMats);
	}
	void AddReferencedMeshes(boost::unordered_set<const luxrays::ExtMesh *> &referencedMesh) const;

	// Update any reference to oldMat with newMat
	void UpdateMaterialReferences(const Material *oldMat, const Material *newMat);

	// Update any reference to oldMesh with newMesh. It returns also if the
	// referenced mesh has been updated or not.
	bool UpdateMeshReference(const luxrays::ExtMesh *oldMesh, luxrays::ExtMesh *newMesh);

	luxrays::Properties ToProperties(const ExtMeshCache &extMeshCache) const;

private:
	luxrays::ExtMesh *mesh;
	const Material *mat;
};

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
	void DefineSceneObject(const std::string &name, SceneObject *m);

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

#endif	/* _SLG_SCENEOBJECT_H */
