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

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/shape.h"
#include "slg/shapes/meshshape.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

MeshShape::MeshShape(ShapeUPtr&& s) {
	shape = std::move(s);
	auto meshes = shape->ReleaseOwnedMeshes();
	mesh = std::move(meshes[0]);
}

MeshShape::MeshShape(const string &fileName) {
	shape = ExtTriangleMesh::Load(fileName);
	auto meshes = shape->ReleaseOwnedMeshes();
	mesh = std::move(meshes[0]);
}

MeshShape::~MeshShape() {
}

void MeshShape::SetLocal2World(const luxrays::Transform &trans) {
	mesh->SetLocal2World(trans);
}

void MeshShape::ApplyTransform(const Transform &trans) {
	// The mesh is a submesh of its shape: transform the shape (vertices +
	// submesh normals)
	shape->ApplyTransform(trans);
}

ExtTriangleMeshUPtr MeshShape::RefineImpl(SceneConstRef scene) {
	return std::move(mesh);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
