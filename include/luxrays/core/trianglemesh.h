/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_TRIANGLEMESH_H
#define	_LUXRAYS_TRIANGLEMESH_H

#include <cassert>
#include <cstdlib>
#include <deque>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/triangle.h"
#include "geometry/transform.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/trianglemesh_types.cl"
}

typedef unsigned int TriangleMeshID;
typedef unsigned int TriangleID;

typedef enum {
	TYPE_TRIANGLE, TYPE_TRIANGLE_INSTANCE,
	TYPE_EXT_TRIANGLE, TYPE_EXT_TRIANGLE_INSTANCE
} MeshType;

class Mesh {
public:
	Mesh() { }
	virtual ~Mesh() { }

	virtual MeshType GetType() const = 0;

	virtual unsigned int GetTotalVertexCount() const = 0;
	virtual unsigned int GetTotalTriangleCount() const = 0;

	virtual BBox GetBBox() const = 0;
	virtual Point GetVertex(const unsigned int vertIndex) const = 0;
	virtual float GetTriangleArea(const unsigned int triIndex) const = 0;

	virtual Point *GetVertices() const = 0;
	virtual Triangle *GetTriangles() const = 0;

	virtual void ApplyTransform(const Transform &trans) = 0;
};

class TriangleMesh : public Mesh {
public:
	// NOTE: deleting meshVertices and meshIndices is up to the application
	TriangleMesh(const unsigned int meshVertCount,
		const unsigned int meshTriCount, Point *meshVertices,
		Triangle *meshTris) {
		assert (meshVertCount > 0);
		assert (meshTriCount > 0);
		assert (meshVertices != NULL);
		assert (meshTris != NULL);

		vertCount = meshVertCount;
		triCount = meshTriCount;
		vertices = meshVertices;
		tris = meshTris;
	}
	virtual ~TriangleMesh() { };
	virtual void Delete() {
		delete[] vertices;
		delete[] tris;
	}

	virtual MeshType GetType() const { return TYPE_TRIANGLE; }
	unsigned int GetTotalVertexCount() const { return vertCount; }
	unsigned int GetTotalTriangleCount() const { return triCount; }

	BBox GetBBox() const;
	Point GetVertex(const unsigned int vertIndex) const { return vertices[vertIndex]; }
	float GetTriangleArea(const unsigned int triIndex) const { return tris[triIndex].Area(vertices); }

	Point *GetVertices() const { return vertices; }
	Triangle *GetTriangles() const { return tris; }

	virtual void ApplyTransform(const Transform &trans);

	static TriangleMesh *Merge(
		const std::deque<const Mesh *> &meshes,
		TriangleMeshID **preprocessedMeshIDs = NULL,
		TriangleID **preprocessedMeshTriangleIDs = NULL);
	static TriangleMesh *Merge(
		const unsigned int totalVerticesCount,
		const unsigned int totalIndicesCount,
		const std::deque<const Mesh *> &meshes,
		TriangleMeshID **preprocessedMeshIDs = NULL,
		TriangleID **preprocessedMeshTriangleIDs = NULL);

protected:
	unsigned int vertCount;
	unsigned int triCount;
	Point *vertices;
	Triangle *tris;
};

class InstanceTriangleMesh : public Mesh {
public:
	InstanceTriangleMesh(TriangleMesh *m, const Transform &t) {
		assert (m != NULL);

		trans = t;
		mesh = m;
	};
	virtual ~InstanceTriangleMesh() { };

	virtual MeshType GetType() const { return TYPE_TRIANGLE_INSTANCE; }
	unsigned int GetTotalVertexCount() const { return mesh->GetTotalVertexCount(); }
	unsigned int GetTotalTriangleCount() const { return mesh->GetTotalTriangleCount(); }

	BBox GetBBox() const {
		return trans * mesh->GetBBox();
	}
	Point GetVertex(const unsigned int vertIndex) const {
		return trans * mesh->GetVertex(vertIndex);
	}
	float GetTriangleArea(const unsigned int triIndex) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::Area(GetVertex(tri.v[0]), GetVertex(tri.v[1]), GetVertex(tri.v[2]));
	}

	virtual void ApplyTransform(const Transform &t) { trans = trans * t; }

	const Transform &GetTransformation() const { return trans; }
	Point *GetVertices() const { return mesh->GetVertices(); }
	Triangle *GetTriangles() const { return mesh->GetTriangles(); }
	TriangleMesh *GetTriangleMesh() const { return mesh; };

protected:
	Transform trans;
	TriangleMesh *mesh;
};

}

#endif	/* _LUXRAYS_TRIANGLEMESH_H */
