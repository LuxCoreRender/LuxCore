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
#include "luxrays/core/shape.h"  // complete ExtTriangleMesh for the make_unique below
#include "luxrays/core/trianglemesh.h"
#include "slg/cameras/camera.h"
#include "slg/utils/meshutils.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ScreenProjection()
//------------------------------------------------------------------------------

ShapeUPtr ScreenProjection(const Camera &camera, const ExtTriangleMesh &mesh) {
	const u_int vertCount = mesh.GetTotalVertexCount();
	const u_int triCount = mesh.GetTotalTriangleCount();

	const auto vertices = mesh.GetVertices();
	const auto triangles = mesh.GetTriangles();

	VertexBuffer newVertices(vertCount);
	for (auto i = 0; i < vertCount; ++i) {
		const Point &oldVertex = vertices[i];

		Point newVertex;
		if (!camera.GetSamplePosition(oldVertex, &newVertex.x, &newVertex.y))
			newVertex = oldVertex[i];
		else {
			// Normalize
			newVertex.x /= camera.filmWidth;
			newVertex.y /= camera.filmHeight;
		}

		newVertices[i] = newVertex;
	}

	TriangleBuffer newTris(triangles);

	auto shape = std::make_unique<Shape>("mesh", std::move(newVertices));
	auto m = std::make_unique<ExtTriangleMesh>(*shape, std::move(newTris), NormalBuffer());
	shape->AddSubmesh(std::move(m));
	return shape;
}

//------------------------------------------------------------------------------
// ExtTriangleMeshBuilder
//------------------------------------------------------------------------------

ShapeUPtr ExtTriangleMeshBuilder::GetExtTriangleMesh() const {

	VertexBuffer newVertices(vertices);
	TriangleBuffer newTris(triangles);

	auto shape = std::make_unique<Shape>("mesh", std::move(newVertices));
	auto m = std::make_unique<ExtTriangleMesh>(*shape, std::move(newTris), NormalBuffer());
	shape->AddSubmesh(std::move(m));
	return shape;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
