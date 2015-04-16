/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include <cassert>
#include <deque>

#include "luxrays/core/trianglemesh.h"
#include "luxrays/core/exttrianglemesh.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// TriangleMesh
//------------------------------------------------------------------------------

BBox TriangleMesh::GetBBox() const {
	BBox bbox;
	for (unsigned int i = 0; i < vertCount; ++i)
		bbox = Union(bbox, vertices[i]);

	return bbox;
}

void TriangleMesh::ApplyTransform(const Transform &trans) {
	for (unsigned int i = 0; i < vertCount; ++i)
		vertices[i] *= trans;
}

TriangleMesh *TriangleMesh::Merge(
	const std::deque<const Mesh *> &meshes,
	TriangleMeshID **preprocessedMeshIDs,
	TriangleID **preprocessedMeshTriangleIDs) {
	unsigned int totalVertexCount = 0;
	unsigned int totalTriangleCount = 0;

	for (std::deque<const Mesh *>::const_iterator m = meshes.begin(); m < meshes.end(); m++) {
		totalVertexCount += (*m)->GetTotalVertexCount();
		totalTriangleCount += (*m)->GetTotalTriangleCount();
	}

	return Merge(totalVertexCount, totalTriangleCount, meshes, preprocessedMeshIDs, preprocessedMeshTriangleIDs);
}

TriangleMesh *TriangleMesh::Merge(
	const unsigned int totalVertexCount,
	const unsigned int totalTriangleCount,
	const std::deque<const Mesh *> &meshes,
	TriangleMeshID **preprocessedMeshIDs,
	TriangleID **preprocessedMeshTriangleIDs) {
	assert (totalVertexCount > 0);
	assert (totalTriangleCount > 0);
	assert (meshes.size() > 0);

	Point *v = AllocVerticesBuffer(totalVertexCount);
	Triangle *i = AllocTrianglesBuffer(totalTriangleCount);

	if (preprocessedMeshIDs)
		*preprocessedMeshIDs = new TriangleMeshID[totalTriangleCount];
	if (preprocessedMeshTriangleIDs)
		*preprocessedMeshTriangleIDs = new TriangleID[totalTriangleCount];

	unsigned int vIndex = 0;
	unsigned int iIndex = 0;
	TriangleMeshID currentID = 0;
	for (std::deque<const Mesh *>::const_iterator m = meshes.begin(); m < meshes.end(); m++) {
		// Copy the mesh vertices
		memcpy(&v[vIndex], (*m)->GetVertices(), sizeof(Point) * (*m)->GetTotalVertexCount());

		const Triangle *tris = (*m)->GetTriangles();

		// Translate mesh indices
		for (unsigned int j = 0; j < (*m)->GetTotalTriangleCount(); j++) {
			i[iIndex].v[0] = tris[j].v[0] + vIndex;
			i[iIndex].v[1] = tris[j].v[1] + vIndex;
			i[iIndex].v[2] = tris[j].v[2] + vIndex;

			if (preprocessedMeshIDs)
				(*preprocessedMeshIDs)[iIndex] = currentID;
			if (preprocessedMeshTriangleIDs)
				(*preprocessedMeshTriangleIDs)[iIndex] = j;

			++iIndex;
		}

		vIndex += (*m)->GetTotalVertexCount();
		if (preprocessedMeshIDs) {
			// To avoid compiler warning
			currentID = currentID + 1;
		}
	}

	return new TriangleMesh(totalVertexCount, totalTriangleCount, v, i);
}

//------------------------------------------------------------------------------
// TriangleMesh
//------------------------------------------------------------------------------

void MotionTriangleMesh::ApplyTransform(const Transform &trans) {
	motionSystem.ApplyTransform(trans);
}
