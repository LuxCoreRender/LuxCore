/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _SLG_SCENEOBJECT_H
#define	_SLG_SCENEOBJECT_H

#include <vector>

#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/utils/properties.h"
#include "luxrays/core/spectrum.h"
#include "slg/sdl/bsdfevents.h"
#include "slg/core/mc.h"
#include "slg/sdl/material.h"
#include "slg/sdl/hitpoint.h"

namespace slg {

//------------------------------------------------------------------------------
// SceneObject
//------------------------------------------------------------------------------

class SceneObject {
public:
	SceneObject(luxrays::ExtMesh *m, const slg::Material *mt) : mesh(m), mat(mt) { }
	virtual ~SceneObject() { }

	std::string GetName() const { return "obj-" + boost::lexical_cast<std::string>(this); }

	const luxrays::ExtMesh *GetExtMesh() const { return mesh; }
	luxrays::ExtMesh *GetExtMesh() { return mesh; }
	const slg::Material *GetMaterial() const { return mat; }

	void AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const {
		mat->AddReferencedMaterials(referencedMats);
	}

	// Update any reference to oldMat with newMat
	void UpdateMaterialReferences(const Material *oldMat, const Material *newMat);

	luxrays::Properties ToProperties(const luxrays::ExtMeshCache &extMeshCache) const;

private:
	luxrays::ExtMesh *mesh;
	const slg::Material *mat;
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

	u_int GetSize() const { return static_cast<u_int>(objs.size()); }
	std::vector<std::string> GetSceneObjectNames() const;

	// Update any reference to oldMat with newMat
	void UpdateMaterialReferences(const Material *oldMat, const Material *newMat);

	void DeleteSceneObject(const std::string &name);
  
private:
	std::vector<SceneObject *> objs;
	std::map<std::string, SceneObject *> objsByName;
};

}

#endif	/* _SLG_SCENEOBJECT_H */
