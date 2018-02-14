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

#ifndef _SLG_SCENEOBJECT_H
#define	_SLG_SCENEOBJECT_H

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/namedobject.h"
#include "luxrays/utils/properties.h"
#include "slg/materials/material.h"
#include "slg/scene/extmeshcache.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/scene/sceneobject_types.cl"
}

//------------------------------------------------------------------------------
// SceneObject
//------------------------------------------------------------------------------

class SceneObject : public luxrays::NamedObject {
public:
	SceneObject(luxrays::ExtMesh *m, const Material *mt, const u_int id,
			const bool invisib) : NamedObject("obj"), mesh(m), mat(mt), objID(id),
			cameraInvisible(invisib) { }
	virtual ~SceneObject() { }

	const luxrays::ExtMesh *GetExtMesh() const { return mesh; }
	luxrays::ExtMesh *GetExtMesh() { return mesh; }
	const Material *GetMaterial() const { return mat; }
	u_int GetID() const { return objID; }
	bool IsCameraInvisible() const { return cameraInvisible; }

	void SetMaterial(const Material *newMat) { mat = newMat; }
	
	void AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const {
		mat->AddReferencedMaterials(referencedMats);
	}
	void AddReferencedMeshes(boost::unordered_set<const luxrays::ExtMesh *> &referencedMesh) const;

	// Update any reference to oldMat with newMat
	void UpdateMaterialReferences(const Material *oldMat, const Material *newMat);

	// Update any reference to oldMesh with newMesh. It returns also if the
	// referenced mesh has been updated or not.
	bool UpdateMeshReference(const luxrays::ExtMesh *oldMesh, luxrays::ExtMesh *newMesh);

	luxrays::Properties ToProperties(const ExtMeshCache &extMeshCache,
			const bool useRealFileName) const;

private:
	luxrays::ExtMesh *mesh;
	const Material *mat;
	const u_int objID;
	bool cameraInvisible;
};

}

#endif	/* _SLG_SCENEOBJECT_H */
