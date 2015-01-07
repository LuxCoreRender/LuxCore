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
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/geometry/motionsystem.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/trianglemesh_types.cl"
}

typedef u_int TriangleMeshID;
typedef u_int TriangleID;

typedef enum {
	TYPE_TRIANGLE, TYPE_TRIANGLE_INSTANCE, TYPE_TRIANGLE_MOTION,
	TYPE_EXT_TRIANGLE, TYPE_EXT_TRIANGLE_INSTANCE, TYPE_EXT_TRIANGLE_MOTION
} MeshType;

class Mesh {
public:
	Mesh() { }
	virtual ~Mesh() { }

	virtual MeshType GetType() const = 0;

	virtual BBox GetBBox() const = 0;
	virtual Point GetVertex(const float time, const u_int vertIndex) const = 0;

	virtual Point *GetVertices() const = 0;
	virtual Triangle *GetTriangles() const = 0;
	virtual u_int GetTotalVertexCount() const = 0;
	virtual u_int GetTotalTriangleCount() const = 0;

	virtual void ApplyTransform(const Transform &trans) = 0;
};

class TriangleMesh : virtual public Mesh {
public:
	// NOTE: deleting meshVertices and meshIndices is up to the application
	TriangleMesh(const u_int meshVertCount,
		const u_int meshTriCount, Point *meshVertices,
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
	void Delete() {
		delete[] vertices;
		delete[] tris;
	}

	virtual MeshType GetType() const { return TYPE_TRIANGLE; }

	virtual BBox GetBBox() const;
	virtual Point GetVertex(const float time, const u_int vertIndex) const { return vertices[vertIndex]; }

	virtual Point *GetVertices() const { return vertices; }
	virtual Triangle *GetTriangles() const { return tris; }
	virtual u_int GetTotalVertexCount() const { return vertCount; }
	virtual u_int GetTotalTriangleCount() const { return triCount; }

	virtual void ApplyTransform(const Transform &trans);

	static Point *AllocVerticesBuffer(const u_int meshVertCount) {
		// Embree requires a float padding field at the end
		return (Point *)new float[3 * meshVertCount + 1];
	}
	static Triangle *AllocTrianglesBuffer(const u_int meshTriCount) {
		return new Triangle[meshTriCount];
	}

	static TriangleMesh *Merge(
		const std::deque<const Mesh *> &meshes,
		TriangleMeshID **preprocessedMeshIDs = NULL,
		TriangleID **preprocessedMeshTriangleIDs = NULL);
	static TriangleMesh *Merge(
		const u_int totalVerticesCount,
		const u_int totalIndicesCount,
		const std::deque<const Mesh *> &meshes,
		TriangleMeshID **preprocessedMeshIDs = NULL,
		TriangleID **preprocessedMeshTriangleIDs = NULL);

protected:
	u_int vertCount;
	u_int triCount;
	Point *vertices;
	Triangle *tris;
};

class InstanceTriangleMesh : virtual public Mesh {
public:
	InstanceTriangleMesh(TriangleMesh *m, const Transform &t) {
		assert (m != NULL);

		trans = t;
		mesh = m;
	};
	virtual ~InstanceTriangleMesh() { };

	virtual MeshType GetType() const { return TYPE_TRIANGLE_INSTANCE; }

	virtual BBox GetBBox() const {
		return trans * mesh->GetBBox();
	}
	virtual Point GetVertex(const float time, const u_int vertIndex) const {
		return trans * mesh->GetVertex(time, vertIndex);
	}

	virtual Point *GetVertices() const { return mesh->GetVertices(); }
	virtual Triangle *GetTriangles() const { return mesh->GetTriangles(); }
	virtual u_int GetTotalVertexCount() const { return mesh->GetTotalVertexCount(); }
	virtual u_int GetTotalTriangleCount() const { return mesh->GetTotalTriangleCount(); }

	virtual void ApplyTransform(const Transform &t) { trans = trans * t; }

	const Transform &GetTransformation() const { return trans; }
	TriangleMesh *GetTriangleMesh() const { return mesh; };

protected:
	Transform trans;
	TriangleMesh *mesh;
};

class MotionTriangleMesh : virtual public Mesh {
public:
	MotionTriangleMesh(TriangleMesh *m, const MotionSystem &ms) {
		assert (m != NULL);

		motionSystem = ms;
		mesh = m;
	};
	virtual ~MotionTriangleMesh() { };

	virtual MeshType GetType() const { return TYPE_TRIANGLE_MOTION; }

	BBox GetBBox() const {
		return motionSystem.Bound(mesh->GetBBox(), true);
	}
	virtual Point GetVertex(const float time, const u_int vertIndex) const {
		return motionSystem.Sample(time).Inverse() * mesh->GetVertex(time, vertIndex);
	}

	virtual Point *GetVertices() const { return mesh->GetVertices(); }
	virtual Triangle *GetTriangles() const { return mesh->GetTriangles(); }
	virtual u_int GetTotalVertexCount() const { return mesh->GetTotalVertexCount(); }
	virtual u_int GetTotalTriangleCount() const { return mesh->GetTotalTriangleCount(); }

	virtual void ApplyTransform(const Transform &t);

	TriangleMesh *GetTriangleMesh() const { return mesh; };	
	const MotionSystem &GetMotionSystem() const { return motionSystem; }

protected:
	MotionSystem motionSystem;
	TriangleMesh *mesh;
};

}

#endif	/* _LUXRAYS_TRIANGLEMESH_H */
