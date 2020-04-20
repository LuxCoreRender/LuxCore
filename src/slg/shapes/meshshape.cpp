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

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/meshshape.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

MeshShape::MeshShape(ExtTriangleMesh *m) {
	mesh = m;
}

MeshShape::MeshShape(const string &fileName) {
	mesh = ExtTriangleMesh::Load(fileName);
}

MeshShape::~MeshShape() {
	if (!refined)
		delete mesh;
}

void MeshShape::SetLocal2World(const luxrays::Transform &trans) {
	mesh->SetLocal2World(trans);
}

void MeshShape::ApplyTransform(const Transform &trans) {
	mesh->ApplyTransform(trans);
}

ExtTriangleMesh *MeshShape::RefineImpl(const Scene *scene) {
	return mesh;
}
