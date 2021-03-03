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

#ifndef _LUXRAYS_TRIANGLEMESH_H
#define	_LUXRAYS_TRIANGLEMESH_H

#include <cassert>
#include <cstdlib>
#include <deque>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/triangle.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/geometry/motionsystem.h"
#include "luxrays/utils/serializationutils.h"

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

class Ray;
class RayHit;

class Mesh {
public:
	Mesh() : bevelRadius(0.f) { }
	virtual ~Mesh() { }

	virtual MeshType GetType() const = 0;

	virtual float GetBevelRadius() const { return bevelRadius; }
	virtual bool IntersectBevel(luxrays::Ray &ray, luxrays::RayHit rayHit) const { return true; }

	virtual BBox GetBBox() const = 0;
	virtual void GetLocal2World(const float time, luxrays::Transform &local2World) const = 0;
	virtual Point GetVertex(const luxrays::Transform &local2World, const u_int vertIndex) const = 0;

	virtual Point *GetVertices() const = 0;
	virtual Triangle *GetTriangles() const = 0;
	virtual u_int GetTotalVertexCount() const = 0;
	virtual u_int GetTotalTriangleCount() const = 0;

	// This can be a very expansive function to run
	virtual float GetMeshArea(const luxrays::Transform &local2World) const = 0;
	virtual float GetTriangleArea(const luxrays::Transform &local2World, const unsigned int triIndex) const = 0;
	virtual void Sample(const luxrays::Transform &local2World, const u_int triIndex, const float u0, const float u1,
		Point *p, float *b0, float *b1, float *b2) const = 0;

	virtual void ApplyTransform(const Transform &trans) = 0;

	friend class boost::serialization::access;

protected:
	float bevelRadius;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & bevelRadius;
	}
};

class TriangleMesh : virtual public Mesh {
public:
	// NOTE: deleting meshVertices and meshIndices is up to the application
	TriangleMesh(const u_int meshVertCount,
		const u_int meshTriCount, Point *meshVertices,
		Triangle *meshTris,
		const float bRadius);
	virtual ~TriangleMesh() { };
	void Delete() {
		delete[] vertices;
		delete[] tris;
	}

	virtual MeshType GetType() const { return TYPE_TRIANGLE; }

	virtual BBox GetBBox() const;
	virtual void GetLocal2World(const float time, luxrays::Transform &local2World) const {
		local2World = appliedTrans;
	}
	virtual Point GetVertex(const luxrays::Transform &local2World, const u_int vertIndex) const { return vertices[vertIndex]; }

	virtual Point *GetVertices() const { return vertices; }
	virtual Triangle *GetTriangles() const { return tris; }
	virtual u_int GetTotalVertexCount() const { return vertCount; }
	virtual u_int GetTotalTriangleCount() const { return triCount; }

	virtual float GetMeshArea(const luxrays::Transform &local2World) const {
		return area;
	}
	
	virtual float GetTriangleArea(const luxrays::Transform &local2World, const unsigned int triIndex) const {
		return tris[triIndex].Area(vertices);
	}
	virtual void Sample(const luxrays::Transform &local2World, const u_int triIndex,
			const float u0, const float u1,
			Point *p, float *b0, float *b1, float *b2) const  {
		const Triangle &tri = tris[triIndex];
		tri.Sample(vertices, u0, u1, p, b0, b1, b2);
	}

	virtual void ApplyTransform(const Transform &trans);

	u_int GetUniqueVerticesMapping(std::vector<u_int> &uniqueVertices,
			bool (*CompareVertices)(const TriangleMesh &mesh,
				const u_int vertIndex1, const u_int vertIndex2)) const;

	static Point *AllocVerticesBuffer(const u_int meshVertCount) {
		// Embree requires a float padding field at the end
		float *buffer = new float[3 * meshVertCount + 1];

		// This is a trick so I can check if the buffer has been really allocated
		// with AllocVerticesBuffer() or not. It is useful for debugging LuxCore
		// applications.
		buffer[3 * meshVertCount] = 1234.1234f;
		
		return (Point *)buffer;
	}
	static Triangle *AllocTrianglesBuffer(const u_int meshTriCount) {
		return new Triangle[meshTriCount];
	}

	static TriangleMesh *Merge(
		const std::deque<const Mesh *> &meshes,
		TriangleMeshID **preprocessedMeshIDs = NULL,
		TriangleID **preprocessedMeshTriangleIDs = NULL);

protected:
	void Preprocess();

	u_int vertCount;
	u_int triCount;
	Point *vertices;
	Triangle *tris;
	float area;
	
	// The transformation that was applied to the vertices
	// (needed e.g. for LocalMapping3D evaluation)
	Transform appliedTrans;
	bool appliedTransSwapsHandedness;

	mutable BBox cachedBBox;
	mutable bool cachedBBoxValid;

	friend class boost::serialization::access;

protected:
	// Used by serialization
	TriangleMesh() {
	}

private:
	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Mesh);

		ar & vertCount;
		for (u_int i = 0; i < vertCount; ++i)
			ar & vertices[i];

		ar & triCount;
		for (u_int i = 0; i < triCount; ++i)
			ar & tris[i];

		ar & appliedTrans;
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Mesh);

		ar & vertCount;
		vertices = new Point[vertCount];
		for (u_int i = 0; i < vertCount; ++i)
			ar & vertices[i];

		ar & triCount;
		tris = new Triangle[triCount];
		for (u_int i = 0; i < triCount; ++i)
			ar & tris[i];

		ar & appliedTrans;

		Preprocess();
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()
};

class InstanceTriangleMesh : virtual public Mesh {
public:
	InstanceTriangleMesh(TriangleMesh *m, const Transform &t);
	virtual ~InstanceTriangleMesh() { }

	virtual MeshType GetType() const { return TYPE_TRIANGLE_INSTANCE; }

	virtual BBox GetBBox() const;
	virtual void GetLocal2World(const float time, luxrays::Transform &local2World) const {
		local2World = trans;
	}
	virtual Point GetVertex(const luxrays::Transform &local2World, const u_int vertIndex) const {
		return trans * mesh->GetVertex(local2World, vertIndex);
	}

	virtual Point *GetVertices() const { return mesh->GetVertices(); }
	virtual Triangle *GetTriangles() const { return mesh->GetTriangles(); }
	virtual u_int GetTotalVertexCount() const { return mesh->GetTotalVertexCount(); }
	virtual u_int GetTotalTriangleCount() const { return mesh->GetTotalTriangleCount(); }

	virtual float GetMeshArea(const luxrays::Transform &local2World) const {
		if (cachedArea < 0.f) {
			float area = 0.f;
			for (u_int i = 0; i < GetTotalTriangleCount(); ++i)
				area += GetTriangleArea(local2World, i);

			// Cache the result
			cachedArea = area;
		}

		return cachedArea;
	}

	virtual float GetTriangleArea(const luxrays::Transform &local2World, const u_int triIndex) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::Area(
				GetVertex(local2World, tri.v[0]),
				GetVertex(local2World, tri.v[1]),
				GetVertex(local2World, tri.v[2]));
	}
	virtual void Sample(const luxrays::Transform &local2World, const u_int triIndex,
			const float u0, const float u1,
			Point *p, float *b0, float *b1, float *b2) const  {
		mesh->Sample(local2World, triIndex, u0, u1, p , b0, b1, b2);
		*p *= trans;
	}

	virtual void ApplyTransform(const Transform &t) {
		trans = trans * t;
		transSwapsHandedness = t.SwapsHandedness();
		cachedBBoxValid = false;

		// Invalidate the cached result
		cachedArea = -1.f;
	}

	const Transform &GetTransformation() const { return trans; }
	void SetTransformation(const Transform &t) {
		trans = t;
		transSwapsHandedness = t.SwapsHandedness();
		cachedBBoxValid = false;

		// Invalidate the cached result
		cachedArea = -1.f;
	}

	TriangleMesh *GetTriangleMesh() const { return mesh; };
	
	friend class boost::serialization::access;

protected:
	// Used by serialization
	InstanceTriangleMesh() {
	}

	Transform trans;
	bool transSwapsHandedness;
	TriangleMesh *mesh;

	mutable float cachedArea;
	mutable BBox cachedBBox;
	mutable bool cachedBBoxValid;

private:
	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Mesh);
		ar & trans;
		ar & transSwapsHandedness;
		ar & mesh;
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Mesh);
		ar & trans;
		ar & transSwapsHandedness;
		ar & mesh;

		cachedArea = -1.f;
		cachedBBoxValid = false;
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()
};

class MotionTriangleMesh : virtual public Mesh {
public:
	MotionTriangleMesh(TriangleMesh *m, const MotionSystem &ms);
	virtual ~MotionTriangleMesh() { }

	virtual MeshType GetType() const { return TYPE_TRIANGLE_MOTION; }

	virtual BBox GetBBox() const;
	virtual void GetLocal2World(const float time, luxrays::Transform &local2World) const {
		const Matrix4x4 m = motionSystem.SampleInverse(time);
		local2World = Transform(m);
	}
	virtual Point GetVertex(const luxrays::Transform &local2World, const u_int vertIndex) const {
		return local2World * mesh->GetVertex(local2World, vertIndex);
	}

	virtual Point *GetVertices() const { return mesh->GetVertices(); }
	virtual Triangle *GetTriangles() const { return mesh->GetTriangles(); }
	virtual u_int GetTotalVertexCount() const { return mesh->GetTotalVertexCount(); }
	virtual u_int GetTotalTriangleCount() const { return mesh->GetTotalTriangleCount(); }

	virtual float GetMeshArea(const luxrays::Transform &local2World) const {
		if (cachedArea < 0.f) {
			float area = 0.f;
			for (u_int i = 0; i < GetTotalTriangleCount(); ++i)
				area += GetTriangleArea(local2World, i);

			// Cache the result
			cachedArea = area;
		}

		return cachedArea;
	}

	virtual float GetTriangleArea(const luxrays::Transform &local2World, const u_int triIndex) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::Area(
				GetVertex(local2World, tri.v[0]),
				GetVertex(local2World, tri.v[1]),
				GetVertex(local2World, tri.v[2]));
	}
	virtual void Sample(const luxrays::Transform &local2World,
			const u_int triIndex, const float u0, const float u1,
			Point *p, float *b0, float *b1, float *b2) const  {
		mesh->Sample(local2World, triIndex, u0, u1, p , b0, b1, b2);
		*p = local2World * (*p);
	}

	virtual void ApplyTransform(const Transform &t);

	TriangleMesh *GetTriangleMesh() const { return mesh; };
	const MotionSystem &GetMotionSystem() const { return motionSystem; }

	friend class boost::serialization::access;

protected:
	// Used by serialization
	MotionTriangleMesh() {
	}

	MotionSystem motionSystem;
	TriangleMesh *mesh;

	mutable float cachedArea;
	mutable BBox cachedBBox;
	mutable bool cachedBBoxValid;

private:
	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Mesh);
		ar & motionSystem;
		ar & mesh;
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Mesh);
		ar & motionSystem;
		ar & mesh;

		cachedArea = -1.f;
		cachedBBoxValid = false;
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()
};

}

// Required for tracking diamond inheritance
BOOST_CLASS_TRACKING(luxrays::Mesh, boost::serialization::track_always)
BOOST_SERIALIZATION_ASSUME_ABSTRACT(luxrays::Mesh)

BOOST_CLASS_VERSION(luxrays::TriangleMesh, 2)
BOOST_CLASS_VERSION(luxrays::InstanceTriangleMesh, 2)
BOOST_CLASS_VERSION(luxrays::MotionTriangleMesh, 1)

BOOST_CLASS_EXPORT_KEY(luxrays::TriangleMesh)
BOOST_CLASS_EXPORT_KEY(luxrays::InstanceTriangleMesh)
BOOST_CLASS_EXPORT_KEY(luxrays::MotionTriangleMesh)

#endif	/* _LUXRAYS_TRIANGLEMESH_H */
