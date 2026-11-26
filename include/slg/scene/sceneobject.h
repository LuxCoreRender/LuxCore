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

#ifndef _SLG_SCENEOBJECT_H
#define	_SLG_SCENEOBJECT_H

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/namedobject.h"
#include "luxrays/utils/properties.h"
#include "slg/materials/material.h"
#include "slg/scene/extmeshcache.h"
#include <functional>

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/scene/sceneobject_types.cl"
}

//------------------------------------------------------------------------------
// SceneObject
//------------------------------------------------------------------------------

typedef enum {
	COMBINED,
	LIGHTMAP
} BakeMapType;

class SceneObject : public luxrays::NamedObject {
public:
	SceneObject(
		luxrays::ExtMeshRef m,
		MaterialRef mt,
		const u_int id,
		const bool invisib)
	: NamedObject("obj"), mesh(m), mat(mt), objID(id), bakeMap(nullptr), cameraInvisible(invisib)
	{ }
	virtual ~SceneObject() { }

	luxrays::ExtMeshConstRef GetExtMesh() const { return mesh; }
	luxrays::ExtMeshRef GetExtMesh() { return mesh; }
	MaterialConstRef GetMaterial() const { return mat; }
	MaterialRef GetMaterial() { return mat; }
	u_int GetID() const { return objID; }
	bool IsCameraInvisible() const { return cameraInvisible; }

	void SetMaterial(MaterialRef newMat) {
		mat = newMat;
	}

	bool HasBakeMap(const BakeMapType type) const { return bool(bakeMap) && (bakeMapType == type); }
	BakeMapType GetBakeMapType() const { return bakeMapType; }
	void SetBakeMap(ImageMapConstRef map, const BakeMapType type, const u_int uvIndex);
	auto GetBakeMap() const { return bakeMap; }
	u_int GetBakeMapUVIndex() const { return bakeMapUVIndex; }
	luxrays::Spectrum GetBakeMapValue(const luxrays::UV &uv) const;

	void AddReferencedImageMaps(std::unordered_set<const ImageMap *> &referencedImgMaps) const;
	void AddReferencedMaterials(
		std::unordered_set<const Material *> &referencedMats
	) const;
	void AddReferencedMeshes(std::unordered_set<const luxrays::ExtMesh *> &referencedMesh) const;

	// Update any reference to oldMat with newMat
	void UpdateMaterialReferences(MaterialConstRef oldMat, MaterialRef newMat);

	// Update any reference to oldMesh with newMesh. It returns also if the
	// referenced mesh has been updated or not.
	bool UpdateMeshReference(luxrays::ExtMeshConstRef oldMesh, luxrays::ExtMeshRef newMesh);

	luxrays::PropertiesUPtr ToProperties(const ExtMeshCache &extMeshCache,
			const bool useRealFileName) const;

	luxrays::ExtMeshConstRef GetMesh() const { return mesh; }
	luxrays::ExtMeshRef GetMesh() { return mesh; }

private:
	std::reference_wrapper<luxrays::ExtMesh> mesh;
	std::reference_wrapper<Material> mat;  // Owned by the scene
	const u_int objID;

	ImageMapConstPtr bakeMap;
	BakeMapType bakeMapType;
	u_int bakeMapUVIndex;

	bool cameraInvisible;
};


}

#endif	/* _SLG_SCENEOBJECT_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
