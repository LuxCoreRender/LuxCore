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

#include <cassert>
#include <deque>

#include <boost/serialization/shared_ptr.hpp>

#include "luxrays/core/trianglemesh.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/shape.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// Mesh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::Mesh)

Mesh::Mesh(const Shape &s) : shape(s) {
}

Point Mesh::GetVertex(
	const luxrays::Transform &local2World,
	const u_int vertIndex
) const {
	return shape.GetVertices()[vertIndex];
}

std::span<Point> Mesh::GetVertices() const {
	return shape.GetVertices().GetObjects();
}

std::span<float> Mesh::GetVerticesAsFloats() const {
	return shape.GetVertices().GetSubObjects();
}

size_t Mesh::GetTotalVertexCount() const {
	return shape.GetVertices().Count();
}

const VertexBuffer &Mesh::GetVertexBuffer() const {
	return shape.GetVertices();
}

//------------------------------------------------------------------------------
// TriangleMesh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::TriangleMesh)

TriangleMesh::TriangleMesh(const Shape &shape, TriangleBuffer&& meshTris) :
	Mesh(shape),
	tris(std::move(meshTris))
{
	const auto meshTriCount = tris.Count();
	assert (meshTriCount > 0);

	appliedTransSwapsHandedness = false;

	// Check if the buffer has really been allocated with AllocVerticesBuffer() or not.
	if (not std::ranges::equal(shape.GetVertices().GetPad(), VERTEXPAD)) {
		std::string msg =
			"luxrays::TriangleMesh() used with a vertex buffer not allocated with "
			"luxrays::TriangleMesh::AllocVerticesBuffer().";
		throw runtime_error(msg);
	}

	Preprocess();
}

void TriangleMesh::Preprocess() {
	// Compute mesh area
	float local_area = 0.f;

	#pragma omp parallel for reduction(+:local_area)
	for (int i = 0; i < static_cast<int>(tris.Count()); ++i)
		local_area += tris[i].Area(GetVertices());

	area = local_area;
	cachedBBoxValid = false;
}

BBox TriangleMesh::GetBBox() const {
	if (!cachedBBoxValid) {
		BBox bbox;
		const auto verts = GetVertices();
		for (u_int i = 0; i < verts.size(); ++i)
			bbox = Union(bbox, verts[i]);
		cachedBBox = bbox;

		cachedBBoxValid = true;
	}

	return cachedBBox;
}

void TriangleMesh::OnShapeTransformed(const Transform &trans) {
	appliedTrans = appliedTrans * trans;
	appliedTransSwapsHandedness = appliedTrans.SwapsHandedness();

	// The vertices have already been transformed by the Shape: refresh the
	// derived data (area, bbox cache; ExtTriangleMesh also refreshes the
	// triangle normals)
	Preprocess();
}

std::span<const Triangle> TriangleMesh::GetTriangles() const {
	return tris.GetObjects();
}
std::span<const Triangle::subtype_t> TriangleMesh::GetTrianglesAsInts() const {
	return tris.GetSubObjects();
}
size_t TriangleMesh::GetTotalTriangleCount() const {
	return tris.Count();
}
float TriangleMesh::GetMeshArea(const luxrays::Transform &local2World) const {
	return area;
}

u_int TriangleMesh::GetUniqueVerticesMapping(
	std::vector<u_int> &uniqueVertices,
	bool (*CompareVertices)(const TriangleMesh& mesh, const u_int vertIndex1, const u_int vertIndex2)) const {

	const u_int originalVertCount = GetTotalVertexCount();

	// Find duplicate vertices

	vector<u_int> sortedVertIndices(originalVertCount);
	for (u_int i = 0; i < originalVertCount; ++i)
		sortedVertIndices[i] = i;

	auto compareVerts = [this](const u_int vert_idx_a, const u_int vert_idx_b) {
		const Point vert_a = this->GetVertex(Transform::TRANS_IDENTITY, vert_idx_a);
		const Point vert_b = this->GetVertex(Transform::TRANS_IDENTITY, vert_idx_b);
		if (vert_a == vert_b) {
			// Special case for doubles, so we ensure ordering.
			return vert_idx_a > vert_idx_b;
		}
		const float x1 = vert_a.x + vert_a.y + vert_a.z;
		const float x2 = vert_b.x + vert_b.y + vert_b.z;

		return x1 < x2;
	};
	sort(sortedVertIndices.begin(), sortedVertIndices.end(), compareVerts);

	// This array stores index of the original vertex for the given vertex index.
	uniqueVertices.resize(originalVertCount);
	u_int uniqueVertCount = 0;

	for (u_int sortedVertIndex = 0; sortedVertIndex < originalVertCount; ++sortedVertIndex) {
		const int vertIndex = sortedVertIndices[sortedVertIndex];
		const Point vertex = GetVertex(Transform::TRANS_IDENTITY, vertIndex);
		bool isDuplicate = false;

		for (u_int otherSortedVertIndex = sortedVertIndex + 1; otherSortedVertIndex < originalVertCount;
				++otherSortedVertIndex) {
			const u_int otherVertIndex = sortedVertIndices[otherSortedVertIndex];
			const Point otherVertex = GetVertex(Transform::TRANS_IDENTITY, otherVertIndex);

			if ((otherVertex.x + otherVertex.y + otherVertex.z)
				- (vertex.x + vertex.y + vertex.z) > 3 * FLT_EPSILON) {
				// We are too far away now, we wouldn't have a duplicate.
				break;
			}

			if (CompareVertices(*this, vertIndex, otherVertIndex)) {
				isDuplicate = true;
				uniqueVertices[vertIndex] = otherVertIndex;
				break;
			}
		}

		if (!isDuplicate) {
			uniqueVertices[vertIndex] = vertIndex;
			++uniqueVertCount;
		}
	}

	// Make sure we always points to the very first orig vertex.
	for (u_int i = 0; i < originalVertCount; ++i) {
		u_int origIndex = uniqueVertices[i];
		while (origIndex != uniqueVertices[origIndex]) {
			origIndex = uniqueVertices[origIndex];
		}
		uniqueVertices[i] = origIndex;
	}

	return uniqueVertCount;
}

//------------------------------------------------------------------------------
// InstanceTriangleMesh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::InstanceTriangleMesh)

InstanceTriangleMesh::InstanceTriangleMesh(TriangleMeshRef m, const Transform &t) :
	Mesh(m.GetShape())
{
	trans = t;
	transSwapsHandedness = t.SwapsHandedness();
	mesh = &m;

	// The mesh area is compute on demand and cached
	cachedArea = -1.f;

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

MotionTriangleMesh::MotionTriangleMesh(TriangleMeshRef m, const MotionSystem &ms) :
	Mesh(m.GetShape())
{
	motionSystem = ms;
	mesh = &m;

	// The mesh area is compute on demand and cached
	cachedArea = -1.f;

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

	// Invalidate the cached result
	cachedArea = -1.f;

	cachedBBoxValid = false;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
