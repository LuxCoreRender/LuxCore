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

#ifndef _LUXRAYS_TRIANGLEMESH_H
#define	_LUXRAYS_TRIANGLEMESH_H

#include <cassert>
#include <cstdlib>
#include <deque>
#include <span>
#include <string>
#include <tuple>

#include "luxrays/luxrays.h"
#include "luxrays/usings.h"
#include "luxrays/core/geometry/triangle.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/geometry/motionsystem.h"
#include "luxrays/utils/serializationutils.h"
#include "luxrays/utils/buffer.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/trianglemesh_types.cl"
}

// Forward declaration: Mesh holds a const reference to its Shape
class Shape;

typedef u_int TriangleMeshID;
typedef u_int TriangleID;

typedef enum {
	TYPE_TRIANGLE, TYPE_TRIANGLE_INSTANCE, TYPE_TRIANGLE_MOTION,
	TYPE_EXT_TRIANGLE, TYPE_EXT_TRIANGLE_INSTANCE, TYPE_EXT_TRIANGLE_MOTION
} MeshType;


// Base class for meshes, mostly virtual.
// A Mesh does NOT own the vertex data: the vertices are owned by the
// mesh's Shape, of which Mesh only holds a const reference (the Shape
// outlives the mesh, e.g. it is owned by the scene shape definitions).
class Mesh {
public:
	Mesh(const Shape &shape);
	virtual ~Mesh() = default;

	// Delete copy operations (if you don't want copying)
	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;

	// Declare default move operations
	Mesh(Mesh&&) = default;
	Mesh& operator=(Mesh&&) = default;

	virtual MeshType GetType() const = 0;

	// The Shape owning the (shared) vertex buffer of this mesh
	const Shape &GetShape() const { return shape; }

	// Vertex accessors (the vertices are stored in the Shape)
	virtual Point GetVertex(const Transform &local2World, const u_int vertIndex) const;
	virtual std::span<Point> GetVertices() const;
	virtual std::span<float> GetVerticesAsFloats() const;
	virtual size_t GetTotalVertexCount() const;
	virtual const VertexBuffer &GetVertexBuffer() const;

	virtual BBox GetBBox() const = 0;
	virtual std::span<const Triangle> GetTriangles() const = 0;
	virtual size_t GetTotalTriangleCount() const = 0;

	virtual luxrays::Transform GetLocal2World(const float time) const = 0;

	// This can be a very expansive function to run
	virtual float GetMeshArea(const luxrays::Transform &local2World) const = 0;
	virtual float GetTriangleArea(const luxrays::Transform &local2World, const unsigned int triIndex) const = 0;
	virtual std::tuple<Point, float, float, float> Sample(const luxrays::Transform &local2World,
		const u_int triIndex, const float u0, const float u1) const = 0;

	// Only instance/motion meshes transform themselves (they own no
	// vertices); plain triangle meshes are transformed through their
	// Shape (see Shape::ApplyTransform)
	virtual void ApplyTransform(const Transform &trans) { }

	friend class boost::serialization::access;

private:
	const Shape &shape;

	template<class Archive> void serialize(Archive &ar, const u_int version) {
	}
};


class TriangleMesh : virtual public Mesh {
public:
	// Build a TriangleMesh on a luxrays::Shape: the Shape supplies its
	// canonical vertex buffer (by const ref, so the Shape is not copied),
	// and the caller supplies the triangles separately. The vertex storage
	// is the Shape's canonical buffer.
	TriangleMesh(const Shape &shape, TriangleBuffer&& meshTris);
	~TriangleMesh() = default;

	MeshType GetType() const override { return TYPE_TRIANGLE; }

	BBox GetBBox() const override;

	Transform GetLocal2World(const float time) const override { return appliedTrans; }

	std::span<const Triangle> GetTriangles() const override;
	std::span<const Triangle::subtype_t> GetTrianglesAsInts() const;
	size_t GetTotalTriangleCount() const override;
	float GetMeshArea(const luxrays::Transform &local2World) const override;

	float GetTriangleArea(
		const luxrays::Transform &local2World,
		const unsigned int triIndex) const override {
		return tris[triIndex].Area(GetVertices());
	}

	std::tuple<Point, float, float, float> Sample(
		const luxrays::Transform &local2World,
		const u_int triIndex,
		const float u0,
		const float u1
	) const override {
		const Triangle &tri = tris[triIndex];
		return tri.Sample(GetVertices(), u0, u1);
	}

	// Called by Shape::ApplyTransform() after the Shape has transformed the
	// shared vertex buffer: refresh this mesh's derived data (triangle
	// normals, area, bbox cache, applied transform). ExtTriangleMesh
	// overrides it to also transform the vertex normals.
	virtual void OnShapeTransformed(const Transform &trans);

	u_int GetUniqueVerticesMapping(std::vector<u_int> &uniqueVertices,
			bool (*CompareVertices)(const TriangleMesh &mesh,
				const u_int vertIndex1, const u_int vertIndex2)) const;

	// Used by serialization (see boost::serialization load_construct_data)
	explicit TriangleMesh(const Shape &shape) : Mesh(shape) { }

protected:
	void Preprocess();

	TriangleBuffer tris;
	float area;

	// The transformation that was applied to the vertices
	// (needed e.g. for LocalMapping3D evaluation)
	Transform appliedTrans;
	bool appliedTransSwapsHandedness;

	mutable BBox cachedBBox;
	mutable bool cachedBBoxValid;

	friend class boost::serialization::access;

private:

	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Mesh);

		auto triCount = tris.Count();

		ar & triCount;
		for (u_int i = 0; i < triCount; ++i)
			ar & tris[i];

		ar & appliedTrans;
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Mesh);

		auto triCount = tris.Count();

		ar & triCount;
		tris.Allocate(triCount);
		for (u_int i = 0; i < triCount; ++i)
			ar & tris[i];

		ar & appliedTrans;

		Preprocess();
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()
};

class InstanceTriangleMesh : virtual public Mesh {
public:
	InstanceTriangleMesh(TriangleMeshRef m, const Transform &t);
	virtual ~InstanceTriangleMesh() { }

	virtual MeshType GetType() const { return TYPE_TRIANGLE_INSTANCE; }

	BBox GetBBox() const;
	Transform GetLocal2World(const float time) const override { return trans; }
	Point GetVertex(const luxrays::Transform &local2World, const u_int vertIndex) const override {
		return trans * mesh->GetVertex(local2World, vertIndex);
	}

	std::span<const Triangle> GetTriangles() const override { return mesh->GetTriangles(); }
	size_t GetTotalTriangleCount() const override { return mesh->GetTotalTriangleCount(); }

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
	virtual std::tuple<Point, float, float, float>
	Sample(const luxrays::Transform &local2World, const u_int triIndex,
		const float u0, const float u1) const override
	{
		auto [p, b0, b1, b2] = mesh->Sample(local2World, triIndex, u0, u1);
		p *= trans;
		return std::make_tuple(p, b0, b1, b2);
	}

	void ApplyTransform(const Transform &t) override {
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

	TriangleMeshConstRef GetTriangleMesh() const { return *mesh; };

	friend class boost::serialization::access;

	// Used by serialization (see boost::serialization load_construct_data)
	explicit InstanceTriangleMesh(const Shape &shape) : Mesh(shape) { }

protected:
	Transform trans;
	bool transSwapsHandedness;
	TriangleMeshPtr mesh;

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
	MotionTriangleMesh(TriangleMeshRef m, const MotionSystem &ms);
	virtual ~MotionTriangleMesh() { }

	virtual MeshType GetType() const { return TYPE_TRIANGLE_MOTION; }

	BBox GetBBox() const;
	Transform GetLocal2World(const float time) const override {
		return Transform(motionSystem.SampleInverse(time));
	}
	Point GetVertex(const luxrays::Transform &local2World, const u_int vertIndex) const override {
		return local2World * mesh->GetVertex(local2World, vertIndex);
	}

	std::span<const Triangle> GetTriangles() const override { return mesh->GetTriangles(); }
	size_t GetTotalTriangleCount() const override { return mesh->GetTotalTriangleCount(); }

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

	virtual float GetTriangleArea
	(const luxrays::Transform &local2World, const u_int triIndex) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::Area(
				GetVertex(local2World, tri.v[0]),
				GetVertex(local2World, tri.v[1]),
				GetVertex(local2World, tri.v[2]));
	}

	virtual std::tuple<Point, float, float, float> Sample(
		const luxrays::Transform &local2World,
		const u_int triIndex,
		const float u0,
		const float u1
	) const override
	{
		auto [p, b0, b1, b2] = mesh->Sample(local2World, triIndex, u0, u1);
		p = local2World * p;
		return std::make_tuple(p, b0, b1, b2);
	}

	void ApplyTransform(const Transform &t) override;

	TriangleMeshConstRef GetTriangleMesh() const { return *mesh; };
	const MotionSystem &GetMotionSystem() const { return motionSystem; }

	friend class boost::serialization::access;

	// Used by serialization (see boost::serialization load_construct_data)
	explicit MotionTriangleMesh(const Shape &shape) : Mesh(shape) { }

protected:
	MotionSystem motionSystem;
	TriangleMeshPtr mesh;

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

// The Mesh classes hold a const reference to their Shape: per the Boost
// Serialization docs (serialization of references), the reference is
// serialized as a pointer through save/load_construct_data overrides, and
// the referenced Shape is tracked so that loading rebinds the reference to
// the (single) loaded Shape object.
namespace boost {
namespace serialization {

template<class Archive>
inline void save_construct_data(Archive &ar, const luxrays::TriangleMesh *t, const unsigned int version) {
	ar << const_cast<luxrays::Shape *>(&t->GetShape());
}
template<class Archive>
inline void load_construct_data(Archive &ar, luxrays::TriangleMesh *t, const unsigned int version) {
	luxrays::Shape *s = nullptr;
	ar >> s;
	::new(t) luxrays::TriangleMesh(*s);
}

template<class Archive>
inline void save_construct_data(Archive &ar, const luxrays::InstanceTriangleMesh *t, const unsigned int version) {
	ar << const_cast<luxrays::Shape *>(&t->GetShape());
}
template<class Archive>
inline void load_construct_data(Archive &ar, luxrays::InstanceTriangleMesh *t, const unsigned int version) {
	luxrays::Shape *s = nullptr;
	ar >> s;
	::new(t) luxrays::InstanceTriangleMesh(*s);
}

template<class Archive>
inline void save_construct_data(Archive &ar, const luxrays::MotionTriangleMesh *t, const unsigned int version) {
	ar << const_cast<luxrays::Shape *>(&t->GetShape());
}
template<class Archive>
inline void load_construct_data(Archive &ar, luxrays::MotionTriangleMesh *t, const unsigned int version) {
	luxrays::Shape *s = nullptr;
	ar >> s;
	::new(t) luxrays::MotionTriangleMesh(*s);
}

} // namespace serialization
} // namespace boost

#endif	/* _LUXRAYS_TRIANGLEMESH_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
