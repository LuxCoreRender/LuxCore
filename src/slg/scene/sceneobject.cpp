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

	if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
		// I have to output also the transformation
		const ExtInstanceTriangleMesh *inst = (const ExtInstanceTriangleMesh *)mesh;
		props.Set(Property("scene.objects." + name + ".transformation")(inst->GetTransformation().m));
	} else if (mesh->GetType() == TYPE_EXT_TRIANGLE_MOTION) {
		// I have to output also the motion blur key transformations
		const ExtMotionTriangleMesh *mot = (const ExtMotionTriangleMesh *)mesh;
		props.Set(mot->GetMotionSystem().ToProperties("scene.objects." + name));
	}

	return props;
}
