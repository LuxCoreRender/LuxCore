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

#include <boost/format.hpp>
#include <memory>

#include "slg/scene/sceneobject.h"
#include "luxrays/core/namedobjectvector.h"
#include "slg/lights/trianglelight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SceneObject
//------------------------------------------------------------------------------

void SceneObject::AddReferencedMeshes(std::unordered_set<const ExtMesh *> &referencedMesh) const {
	referencedMesh.insert(std::addressof(GetMesh()));

	// Check if it is an instance and add referenced mesh
	if (GetMesh().GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
		auto& imesh = static_cast<const ExtInstanceTriangleMesh &>(GetMesh());
		referencedMesh.insert(&imesh.GetExtTriangleMesh());
	}
}

void SceneObject::UpdateMaterialReferences(MaterialConstRef oldMat, MaterialRef newMat) {
	if (mat == oldMat)
		mat = newMat;
}

bool SceneObject::UpdateMeshReference(luxrays::ExtMeshConstRef oldMesh, luxrays::ExtMeshRef newMesh) {
	if (mesh == oldMesh) {
		mesh = newMesh;
		return true;
	} else
		return false;
}

PropertiesUPtr SceneObject::ToProperties(const ExtMeshCache &extMeshCache,
		const bool useRealFileName) const {
	auto props = std::make_unique<Properties>();

	const std::string name = GetName();
    props->Set(Property("scene.objects." + name + ".material")(GetMaterial().GetName()));
	const string fileName = useRealFileName ?
		extMeshCache.GetRealFileName(mesh) : extMeshCache.GetSequenceFileName(mesh);
	props->Set(Property("scene.objects." + name + ".ply")(fileName));
	props->Set(Property("scene.objects." + name + ".camerainvisible")(cameraInvisible));
	props->Set(Property("scene.objects." + name + ".id")(objID));

	switch (GetMesh().GetType()) {
		case TYPE_EXT_TRIANGLE: {
			// I have to output the applied transformation
			auto& extMesh = static_cast<const ExtTriangleMesh &>(GetMesh());
			Transform trans;
trans = 			extMesh.GetLocal2World(0.f);

			props->Set(Property("scene.objects." + name + ".appliedtransformation")(trans.m));
			break;
		}
		case TYPE_EXT_TRIANGLE_INSTANCE: {
			// I have to output also the transformation
			auto& inst = static_cast<const ExtInstanceTriangleMesh &>(GetMesh());
			props->Set(Property("scene.objects." + name + ".transformation")(inst.GetTransformation().m));
			break;
		}
		case TYPE_EXT_TRIANGLE_MOTION: {
			// I have to output also the motion blur key transformations
			auto& mot = static_cast<const ExtMotionTriangleMesh &>(GetMesh());
			props->Set(mot.GetMotionSystem().ToProperties("scene.objects." + name, true));
			break;
		}
		default:
			// Nothing to do
			break;
	}

	if (bakeMap) {
		switch (bakeMapType) {
			case COMBINED:
				props->Set(bakeMap->ToProperties("scene.objects." + name + ".bake.combined", useRealFileName));
				props->Set(Property("scene.objects." + name + ".bake.combined.uvindex")(bakeMapUVIndex));
				break;
			case LIGHTMAP:
				props->Set(bakeMap->ToProperties("scene.objects." + name + ".bake.lightmap", useRealFileName));
				props->Set(Property("scene.objects." + name + ".bake.lightmap.uvindex")(bakeMapUVIndex));
				break;
			default:
				throw runtime_error("Unknown bake map type in SceneObject::ToProperties(): " + ToString(bakeMapType));
		}
	}

	return props;
}

void SceneObject::SetBakeMap(ImageMapConstRef map, const BakeMapType type, const u_int uvIndex) {
	bakeMap = &map;
	bakeMapType = type;
	bakeMapUVIndex = uvIndex;
}

Spectrum SceneObject::GetBakeMapValue(const UV &uv) const {
	assert (bakeMap);

	return bakeMap->GetSpectrum(uv);
}

void SceneObject::AddReferencedImageMaps(std::unordered_set<const ImageMap *> &referencedImgMaps) const {
	if (bakeMap)
		referencedImgMaps.insert(bakeMap.get());
}

void SceneObject::AddReferencedMaterials(
	std::unordered_set<const Material *> &referencedMats
) const {
	GetMaterial().AddReferencedMaterials(referencedMats);
}


// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
