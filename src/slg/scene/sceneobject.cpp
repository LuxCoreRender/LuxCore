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

#include <boost/format.hpp>

#include "slg/scene/sceneobject.h"
#include "slg/lights/trianglelight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SceneObject
//------------------------------------------------------------------------------

void SceneObject::AddReferencedMeshes(boost::unordered_set<const luxrays::ExtMesh *> &referencedMesh) const {
	referencedMesh.insert(mesh);

	// Check if it is an instance and add referenced mesh
	if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
		ExtInstanceTriangleMesh *imesh = (ExtInstanceTriangleMesh *)mesh;
		referencedMesh.insert(imesh->GetExtTriangleMesh());
	}	
}

void SceneObject::UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
	if (mat == oldMat)
		mat = newMat;
}

bool SceneObject::UpdateMeshReference(const luxrays::ExtMesh *oldMesh, luxrays::ExtMesh *newMesh) {
	if (mesh == oldMesh) {
		mesh = newMesh;
		return true;
	} else
		return false;
}

Properties SceneObject::ToProperties(const ExtMeshCache &extMeshCache,
		const bool useRealFileName) const {
	Properties props;

	const std::string name = GetName();
    props.Set(Property("scene.objects." + name + ".material")(mat->GetName()));
	const string fileName = useRealFileName ?
		extMeshCache.GetRealFileName(mesh) : extMeshCache.GetSequenceFileName(mesh);
	props.Set(Property("scene.objects." + name + ".ply")(fileName));
	props.Set(Property("scene.objects." + name + ".camerainvisible")(cameraInvisible));
	props.Set(Property("scene.objects." + name + ".id")(objID));

	switch (mesh->GetType()) {
		case TYPE_EXT_TRIANGLE: {
			// I have to output the applied transformation
			const ExtTriangleMesh *extMesh = (const ExtTriangleMesh *)mesh;

			Transform trans;
			extMesh->GetLocal2World(0.f, trans);

			props.Set(Property("scene.objects." + name + ".appliedtransformation")(trans.m));			
			break;
		}
		case TYPE_EXT_TRIANGLE_INSTANCE: {
			// I have to output also the transformation
			const ExtInstanceTriangleMesh *inst = (const ExtInstanceTriangleMesh *)mesh;
			props.Set(Property("scene.objects." + name + ".transformation")(inst->GetTransformation().m));
			break;
		}
		case TYPE_EXT_TRIANGLE_MOTION: {
			// I have to output also the motion blur key transformations
			const ExtMotionTriangleMesh *mot = (const ExtMotionTriangleMesh *)mesh;
			props.Set(mot->GetMotionSystem().ToProperties("scene.objects." + name, true));
			break;
		}
		default:
			// Nothing to do
			break;
	}

	if (combinedBakeMap) {
		props.Set(combinedBakeMap->ToProperties("scene.objects." + name + ".bake.combined", useRealFileName));
		props.Set(Property("scene.objects." + name + ".bake.combined.uvindex")(combinedBakeMapUVIndex));
	}

	return props;
}

Spectrum SceneObject::GetCombinedBakeMapValue(const UV &uv) const {
	assert (combinedBakeMap);

	return combinedBakeMap->GetSpectrum(uv);
}

void SceneObject::AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
	if (combinedBakeMap)
		referencedImgMaps.insert(combinedBakeMap);
}

void SceneObject::AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const {
	mat->AddReferencedMaterials(referencedMats);
}
