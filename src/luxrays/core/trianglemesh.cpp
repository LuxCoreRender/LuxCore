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

#include <cassert>
#include <deque>

#include "luxrays/core/trianglemesh.h"
#include "luxrays/core/exttrianglemesh.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// TriangleMesh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::Mesh)

//------------------------------------------------------------------------------
// TriangleMesh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::TriangleMesh)

TriangleMesh::TriangleMesh(const u_int meshVertCount,
		const u_int meshTriCount, Point *meshVertices,
		Triangle *meshTris) {
	assert (meshVertCount > 0);
	assert (meshTriCount > 0);
	assert (meshVertices != NULL);
	assert (meshTris != NULL);

	// Check if the buffer has been really allocated with AllocVerticesBuffer() or not.
	const float *vertBuff = (float *)meshVertices;
	if (vertBuff[3 * meshVertCount] != 1234.1234f)
		throw std::runtime_error("luxrays::TriangleMesh() used with a vertex buffer not allocated with luxrays::TriangleMesh::AllocVerticesBuffer()");

	vertCount = meshVertCount;
	triCount = meshTriCount;
	vertices = meshVertices;
	tris = meshTris;

	cachedBBoxValid = false;
}

BBox TriangleMesh::GetBBox() const {
	if (!cachedBBoxValid) {
		BBox bbox;
		for (u_int i = 0; i < vertCount; ++i)
			bbox = Union(bbox, vertices[i]);
		cachedBBox = bbox;
		
		cachedBBoxValid = true;
	}

	return cachedBBox;
}

void TriangleMesh::ApplyTransform(const Transform &trans) {
	for (u_int i = 0; i < vertCount; ++i)
		vertices[i] *= trans;

	cachedBBoxValid = false;
}

TriangleMesh *TriangleMesh::Merge(
	const std::deque<const Mesh *> &meshes,
	TriangleMeshID **preprocessedMeshIDs,
	TriangleID **preprocessedMeshTriangleIDs) {
	u_int totalVertexCount = 0;
	u_int totalTriangleCount = 0;

	for (std::deque<const Mesh *>::const_iterator m = meshes.begin(); m < meshes.end(); m++) {
		totalVertexCount += (*m)->GetTotalVertexCount();
		totalTriangleCount += (*m)->GetTotalTriangleCount();
	}

	assert (totalVertexCount > 0);
	assert (totalTriangleCount > 0);
	assert (meshes.size() > 0);

	Point *v = AllocVerticesBuffer(totalVertexCount);
	Triangle *i = AllocTrianglesBuffer(totalTriangleCount);

	if (preprocessedMeshIDs)
		*preprocessedMeshIDs = new TriangleMeshID[totalTriangleCount];
	if (preprocessedMeshTriangleIDs)
		*preprocessedMeshTriangleIDs = new TriangleID[totalTriangleCount];

	u_int vIndex = 0;
	u_int iIndex = 0;
	TriangleMeshID currentID = 0;
	for (std::deque<const Mesh *>::const_iterator m = meshes.begin(); m < meshes.end(); m++) {
		// Copy the mesh vertices
		memcpy(&v[vIndex], (*m)->GetVertices(), sizeof(Point) * (*m)->GetTotalVertexCount());

		const Triangle *tris = (*m)->GetTriangles();

		// Translate mesh indices
		for (u_int j = 0; j < (*m)->GetTotalTriangleCount(); j++) {
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
// InstanceTriangleMesh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::InstanceTriangleMesh)

InstanceTriangleMesh::InstanceTriangleMesh(TriangleMesh *m, const Transform &t) {
	assert (m != NULL);

	trans = t;
	mesh = m;

	cachedBBoxValid = false;
}

BBox InstanceTriangleMesh::GetBBox() const {
	if (!cachedBBoxValid) {
		cachedBBox = trans * mesh->GetBBox();
		cachedBBoxValid = true;
	}

	return cachedBBox;
}

//------------------------------------------------------------------------------
// MotionTriangleMesh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::MotionTriangleMesh)

MotionTriangleMesh::MotionTriangleMesh(TriangleMesh *m, const MotionSystem &ms) {
	assert (m != NULL);

	motionSystem = ms;
	mesh = m;

	cachedBBoxValid = false;
}

BBox MotionTriangleMesh::GetBBox() const {
	if (!cachedBBoxValid) {
		cachedBBox = motionSystem.Bound(mesh->GetBBox(), true);
		cachedBBoxValid = true;
	}

	return cachedBBox;
}

void MotionTriangleMesh::ApplyTransform(const Transform &trans) {
	motionSystem.ApplyTransform(trans);

	cachedBBoxValid = false;
}
