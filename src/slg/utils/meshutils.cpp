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

#include "slg/cameras/camera.h"
#include "slg/utils/meshutils.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ScreenProjection()
//------------------------------------------------------------------------------

ExtTriangleMesh *ScreenProjection(const Camera &camera, const ExtTriangleMesh &mesh) {
	const u_int vertCount = mesh.GetTotalVertexCount();
	const u_int triCount = mesh.GetTotalTriangleCount();

	const Point *vertices = mesh.GetVertices();
	const Triangle *triangles = mesh.GetTriangles();

	Point *newVertices = ExtTriangleMesh::AllocVerticesBuffer(vertCount);
	for (u_int i = 0; i < vertCount; ++i) {
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

	Triangle *newTris = ExtTriangleMesh::AllocTrianglesBuffer(triCount);
	copy(triangles, triangles + triCount, newTris);

	return new ExtTriangleMesh(vertCount, triCount, newVertices, newTris);
}

//------------------------------------------------------------------------------
// ExtTriangleMeshBuilder
//------------------------------------------------------------------------------

ExtTriangleMesh *ExtTriangleMeshBuilder::GetExtTriangleMesh() const {
	const u_int vertCount = vertices.size();
	const u_int triCount = triangles.size();

	Point *newVertices = ExtTriangleMesh::AllocVerticesBuffer(vertCount);
	copy(vertices.begin(), vertices.end(), newVertices);

	Triangle *newTris = ExtTriangleMesh::AllocTrianglesBuffer(triCount);
	copy(triangles.begin(), triangles.end(), newTris);

	return new ExtTriangleMesh(vertCount, triCount, newVertices, newTris);
}
