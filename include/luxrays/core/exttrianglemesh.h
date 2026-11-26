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

#ifndef _LUXRAYS_EXTTRIANGLEMESH_H
#define	_LUXRAYS_EXTTRIANGLEMESH_H

#include <cassert>
#include <memory>
#include <span>

#include <boost/serialization/shared_ptr.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/extmeshprop.h"
#include "luxrays/core/bvh/bvhbuild.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/geometry/triangle.h"
#include "luxrays/core/geometry/motionsystem.h"
#include "luxrays/core/trianglemesh.h"
#include "luxrays/core/namedobject.h"
#include "luxrays/utils/serializationutils.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/exttrianglemesh_types.cl"
}

class Ray;
class RayHit;

/*
 * The inheritance scheme used here:
 *
 *         | =>    TriangleMesh      => |
 * Mesh => |                            | => ExtTriangleMesh
 *         | =>       ExtMesh        => |
 *
 *         | => InstanceTriangleMesh => |
 * Mesh => |                            | => ExtInstanceTriangleMesh
 *         | =>       ExtMesh        => |
 *
 *         | => MotionTriangleMesh   => |
 * Mesh => |                            | => ExtMotionTriangleMesh
 *         | =>       ExtMesh        => |
 */

class ExtMesh : virtual public Mesh, public NamedObject {
public:
	ExtMesh(const Shape &shape, float bRadius = 0.f, const std::string &shapeName = "") :
		Mesh(shape), bevelRadius(bRadius), shapeName(shapeName) { }
	virtual ~ExtMesh() = default;

	// Name of the luxrays::Shape this mesh belongs to (empty = not grouped;
	// Scene::DefineMesh then uses the mesh name as shape name)
	const std::string &GetShapeName() const { return shapeName; }
	void SetShapeName(const std::string &n) { shapeName = n; }

	virtual float GetBevelRadius() const { return bevelRadius; }
	virtual bool IntersectBevel(const luxrays::Ray &ray, const luxrays::RayHit &rayHit,
			bool &continueToTrace, float &rayHitT,
			luxrays::Point &p, luxrays::Normal &n) const;

	virtual bool HasNormals() const = 0;
	virtual bool HasUVs(const u_int layerIndex) const = 0;
	virtual bool HasColors(const u_int layerIndex) const = 0;
	virtual bool HasAlphas(const u_int layerIndex) const = 0;

	virtual bool HasVertexAOV(const u_int layerIndex) const = 0;
	virtual bool HasTriAOV(const u_int layerIndex) const = 0;

	virtual Normal GetGeometryNormal(const luxrays::Transform &local2World, const u_int triIndex) const = 0;
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int triIndex, const u_int vertIndex) const = 0;
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int vertIndex) const = 0;

	virtual UV GetUV(const u_int vertIndex, const u_int layerIndex) const = 0;
	virtual Spectrum GetColor(const u_int vertIndex, const u_int layerIndex) const = 0;
	virtual float GetAlpha(const u_int vertIndex, const u_int layerIndex) const = 0;

	virtual float GetVertexAOV(const u_int vertIndex, const u_int layerIndex) const = 0;
	virtual float GetTriAOV(const u_int triIndex, const u_int layerIndex) const = 0;

	virtual bool GetTriBaryCoords(const luxrays::Transform &local2World, const u_int triIndex, const Point &hitPoint, float *b1, float *b2) const = 0;
    virtual void GetDifferentials(const luxrays::Transform &local2World,
			const u_int triIndex, const Normal &shadeNormal, const u_int layerIndex,
			Vector *dpdu, Vector *dpdv,
			Normal *dndu, Normal *dndv) const;

	virtual Normal InterpolateTriNormal(const luxrays::Transform &local2World,
			const u_int triIndex, const float b1, const float b2) const = 0;
	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const = 0;
	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const = 0;
	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const = 0;

	virtual float InterpolateTriVertexAOV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const = 0;

	virtual void Delete() = 0;
	virtual void Save(const std::string &fileName) const = 0;

	friend class boost::serialization::access;

protected:
	float bevelRadius;
	// Runtime-only (not serialized): on load, DefineMesh re-registers the
	// mesh into a shape named after the mesh when no shape name is set
	std::string shapeName;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Mesh);
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(NamedObject);

		ar & bevelRadius;
	}
};

class ExtTriangleMesh : public TriangleMesh, public ExtMesh {
public:
	// The mesh is built on a Shape: the Shape owns the canonical vertex
	// buffer (shared by all the submeshes of the shape), the caller
	// supplies the triangles (and the other per-mesh data) separately.
	ExtTriangleMesh(
		const Shape &shape,
		TriangleBuffer&& meshTris,
		NormalBuffer&& meshNormals,
		ExtMeshProp<UV>::Layer meshUVs = nullptr,
		ExtMeshProp<Spectrum>::Layer meshCols = nullptr,
		ExtMeshProp<float>::Layer meshAlphas = nullptr,
		const float bRadius = 0.f,
		const std::string &shapeName = "",
		bool compactVertices = true
	);
	ExtTriangleMesh(
		const Shape &shape,
		TriangleBuffer&& meshTris,
		NormalBuffer&& meshNormals,
		std::optional<std::span<UV>> meshUVs,
		std::optional<std::span<Spectrum>> meshCols,
		std::optional<std::span<float>> meshAlphas,
		const float bRadius = 0.f,
		bool compactVertices = true
	);
	ExtTriangleMesh(
		const Shape &shape,
		TriangleBuffer&& meshTris,
		NormalBuffer&& meshNormals,
		std::optional<ExtMeshProp<UV>> meshUVs,
		std::optional<ExtMeshProp<Spectrum>> meshCols,
		std::optional<ExtMeshProp<float>> meshAlphas,
		const float bRadius = 0.f,
		bool compactVertices = true
	);
	~ExtTriangleMesh() { };
	void Delete() override;

	const NormalBuffer& GetNormals() const { return normals; }
	const NormalBuffer& GetTriNormals() const { return triNormals; }

	// AOV
	void SetVertexAOV(
		const u_int layerIndex,
		std::shared_ptr<float[]> values,
		size_t size
	) {
		vertAOV.SetLayer(layerIndex, values, size);
	}
	void SetVertexAOV(
		const u_int layerIndex,
		std::span<float> dataSpan
	) {
		vertAOV.SetLayer(layerIndex, dataSpan);
	}
	void DeleteVertexAOV(const u_int layerIndex) { vertAOV.DeleteLayer(layerIndex); }
	auto GetVertexAOVs(const u_int layerIndex) const { return vertAOV.GetLayer(layerIndex); }
	const auto& GetAllVertexAOVs() const { return vertAOV; }
	bool HasVertexAOV(const u_int layerIndex) const override {
		return vertAOV.LayerHasValues(layerIndex);
	}

	// Triangle AOV
	void SetTriAOV(
		const u_int layerIndex, std::shared_ptr<float[]> values, size_t size
	) {
		triAOV.SetLayer(layerIndex, values, size);
	}
	void SetTriAOV(
		const u_int layerIndex, std::span<float> dataSpan
	) {
		triAOV.SetLayer(layerIndex, dataSpan);
	}
	void DeleteTriAOV(const u_int layerIndex) { triAOV.DeleteLayer(layerIndex); }
	auto GetTriAOVs(const u_int layerIndex) const { return triAOV.GetLayer(layerIndex); }
	bool HasTriAOV(const u_int layerIndex) const override {
		return triAOV.LayerHasValues(layerIndex);
	}

	// UV
	void SetUVs(const u_int layerIndex, std::shared_ptr<UV[]> values, size_t size) {
		uvs.SetLayer(layerIndex, values, size);
	}
	void DeleteUVs(const u_int layerIndex) { uvs.DeleteLayer(layerIndex); }
	auto GetUVs(const u_int layerIndex) const { return uvs.GetLayer(layerIndex); }
	bool HasUVs(const u_int layerIndex) const override { return uvs.LayerHasValues(layerIndex); }
	const auto& GetAllUVs() const { return uvs; }

	// Vertex colors
	void SetColors(const u_int layerIndex, std::shared_ptr<Spectrum[]> values, size_t size) {
		cols.SetLayer(layerIndex, values, size);
	}
	void DeleteColors(const u_int layerIndex) { cols.DeleteLayer(layerIndex); }
	auto GetColors(const u_int layerIndex) const { return cols.GetLayer(layerIndex); }
	bool HasColors(const u_int layerIndex) const override {
		return cols.LayerHasValues(layerIndex);
	}
	const auto& GetAllColors() const { return cols; }

	// Vertex alphas
	void SetAlphas(const u_int layerIndex, std::shared_ptr<float[]> values, size_t size) {
		alphas.SetLayer(layerIndex, values, size);
	}
	void DeleteAlphas(const u_int layerIndex) { alphas.DeleteLayer(layerIndex); }
	auto GetAlphas(const u_int layerIndex) const { return alphas.GetLayer(layerIndex); }
	bool HasAlphas(const u_int layerIndex) const override { return alphas.LayerHasValues(layerIndex); }
	const auto& GetAllAlphas() const { return alphas; }

	// Imported creases: vertex index pairs (flat v0, v1, ...) with one
	// sharpness weight per pair, in OpenSubdiv units (0 = smooth,
	// >= 10 = infinite crease). Populated e.g. from a PLY 'edge' element
	// or via pyluxcore Scene.DefineMeshExt(crease_pairs=..., crease_weights=...)
	void SetCreases(std::vector<u_int> &&pairs, std::vector<float> &&weights);
	const std::vector<u_int>& GetCreasePairs() const { return creasePairs; }
	const std::vector<float>& GetCreaseWeights() const { return creaseWeights; }
	bool HasCreases() const { return !creaseWeights.empty(); }

	// Vertex map (local->global): the mesh stores only the vertices its
	// triangles actually reference (compacted), triangle indices are local
	// (0..countVertices-1) and vertexMap[local] is the index in the Shape
	// buffer. Built at construction from the triangle buffer (always, even
	// when identity). Used at runtime by the vertex accessors and by
	// MakeGlobalView to rebuild global-indexed views.
	void SetVertexMap(std::vector<u_int> &&m) { vertexMap = std::move(m); }
	const std::vector<u_int>& GetVertexMap() const { return vertexMap; }
	bool HasVertexMap() const { return !vertexMap.empty(); }

	// Build vertexMap from the triangle buffer, remap the triangle indices
	// to local ones and compact all the per-vertex data (positions, normals,
	// UVs, colors, alphas, vertex AOVs) through the map. Triangle topology
	// and triangle AOVs are not touched.
	void CompactVertexData();

	// Vertex accessors return the mapped (compacted) data (see the
	// definitions in exttrianglemesh.cpp: before CompactVertexData() has
	// run, they fall back to the Shape's buffer with the still-global
	// triangle indices)
	Point GetVertex(const Transform &local2World, const u_int vertIndex) const override;
	std::span<Point> GetVertices() const override;
	std::span<float> GetVerticesAsFloats() const override;
	size_t GetTotalVertexCount() const override;


	// Computes the vertex normals in place: the normal buffer of the mesh is
	// always valid afterwards
	void ComputeNormals();

	MeshType GetType() const override { return TYPE_EXT_TRIANGLE; }

	bool HasNormals() const override { return bool(normals); }

	Normal GetGeometryNormal(const luxrays::Transform &local2World, const u_int triIndex) const override {
		// Pre-computed geometry normals already factor appliedTransSwapsHandedness
		return triNormals[triIndex];
	}
	Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int triIndex, const u_int vertIndex) const override {
		return (appliedTransSwapsHandedness ? -1.f : 1.f) * normals[tris[triIndex].v[vertIndex]];
	}
	Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int vertIndex) const override {
		return (appliedTransSwapsHandedness ? -1.f : 1.f) * normals[vertIndex];
	}

	UV GetUV(const u_int vertIndex, const u_int layerIndex) const override { return uvs[layerIndex][vertIndex]; }
	Spectrum GetColor(const u_int vertIndex, const u_int layerIndex) const override { return cols[layerIndex][vertIndex]; }
	float GetAlpha(const u_int vertIndex, const u_int layerIndex) const override;
	float GetVertexAOV(const u_int vertIndex, const u_int layerIndex) const override;
	float GetTriAOV(const u_int triIndex, const u_int layerIndex) const override;

	bool GetTriBaryCoords(const luxrays::Transform &local2World, const u_int triIndex, const Point &hitPoint, float *b1, float *b2) const override;
	void SetLocal2World(const luxrays::Transform &t);

	// Called by Shape::ApplyTransform() after the Shape transformed the
	// shared vertex buffer: rotate the vertex normals (copy-on-write when
	// shared) and refresh the derived data
	void OnShapeTransformed(const Transform &trans) override;

	Normal InterpolateTriNormal(const luxrays::Transform &local2World, const u_int triIndex,
			const float b1, const float b2) const override;

	UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override;

	Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override;

	float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override;

	float InterpolateTriVertexAOV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override;

	void Save(const std::string &fileName) const override;

	void CopyAOV(ExtTriangleMeshRef destMesh) const;

	// The copy is built on a new Shape owning the (copied or shared) vertex
	// buffer; the returned Shape owns the copied mesh
	ShapeUPtr CopyExt(
		std::optional<VertexBuffer> meshVertices,
		std::optional<TriangleBuffer> meshTris,
		std::optional<NormalBuffer> meshNormals,
		std::optional<ExtMeshProp<UV>> meshUVs,
		std::optional<ExtMeshProp<Spectrum>> meshCols,
		std::optional<ExtMeshProp<float>> meshAlphas,
		const float bRadius = 0.f
	) const;

	ShapeUPtr Copy(
		std::optional<VertexBuffer> meshVertices,
		std::optional<TriangleBuffer> meshTris,
		std::optional<NormalBuffer> meshNormals,
		std::optional<std::span<UV>> mUVs,
		std::optional<std::span<Spectrum>> mCols,
		std::optional<std::span<float>> mAlphas,
		const float bRadius = 0.f
	) const;

	ShapeUPtr Copy(const float bRadius = 0.f) const;

	bool IntersectBevel(const luxrays::Ray &ray, const luxrays::RayHit &rayHit,
			bool &continueToTrace, float &rayHitT,
			luxrays::Point &p, luxrays::Normal &n) const override;

	// Load/Merge/Copy return a Shape owning the produced mesh (the mesh
	// references the Shape through Mesh::GetShape(), so the Shape must
	// outlive the mesh)
	static ShapeUPtr Load(const std::string &fileName);

	static ShapeUPtr Merge(
		std::vector<std::reference_wrapper<const ExtTriangleMesh>> meshes,
		std::optional<std::vector<luxrays::Transform>> trans
	);

	friend class ExtInstanceTriangleMesh;
	friend class ExtMotionTriangleMesh;
	friend class boost::serialization::access;

public:
	class BevelCylinder {
	public:
		BevelCylinder() { }
		BevelCylinder(const luxrays::Point &cv0, const luxrays::Point &cv1) {
			v0 = cv0;
			v1 = cv1;
		}

		float Intersect(const luxrays::Ray &ray, const float bevelRadius) const;
		void IntersectNormal(const luxrays::Point &pos, const float bevelRadius,
				luxrays::Normal &n) const;

		luxrays::Point v0, v1;
	};

	class BevelBoundingCylinder {
	public:
		BevelBoundingCylinder() { }
		BevelBoundingCylinder(const luxrays::Point &cv0, const luxrays::Point &cv1,
		const float r) {
			v0 = cv0;
			v1 = cv1;
			radius = r;
		}

		luxrays::BBox GetBBox() const;
		bool IsInside(const luxrays::Point &p) const;
		float Intersect(const luxrays::Ray &ray, const float bevelRadius) const;
		void IntersectNormal(const luxrays::Point &pos, const float bevelRadius,
				luxrays::Normal &n) const;

		luxrays::Point v0, v1;
		float radius;
	};

	static ShapeUPtr LoadPly(const std::string &fileName);
	static ShapeUPtr LoadSerialized(const std::string &fileName);

	// Used by serialization (see boost::serialization load_construct_data)
	explicit ExtTriangleMesh(const Shape &s) : Mesh(s), TriangleMesh(s), ExtMesh(s) { }

	void Init(
		NormalBuffer&& meshNormals,
		std::optional<ExtMeshProp<UV>> meshUVs,
		std::optional<ExtMeshProp<Spectrum>> meshCols,
		std::optional<ExtMeshProp<float>> meshAlphas,
		bool compactVertices = true
	);

	void Preprocess();
	void PreprocessBevel();

	virtual void SavePly(const std::string &fileName) const;
	virtual void SaveSerialized(const std::string &fileName) const;

	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(TriangleMesh);
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ExtMesh);

		const bool hasNormals = HasNormals();
		ar & hasNormals;
		auto vertCount = GetTotalVertexCount();
		auto triCount = tris.Count();
		ar & vertexMap;
		if (HasNormals())
			for (u_int i = 0; i < vertCount; ++i)
				ar & normals[i];

		for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
			uvs.Serialize(i, ar, vertCount);
			cols.Serialize(i, ar, vertCount);
			alphas.Serialize(i, ar, vertCount);
			vertAOV.Serialize(i, ar, vertCount);
			triAOV.Serialize(i, ar, triCount);
		}
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(TriangleMesh);
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ExtMesh);

		bool hasNormals;
		ar & hasNormals;
		ar & vertexMap;
		auto vertCount = vertexMap.size();
		auto triCount = tris.Count();
		if (hasNormals) {
			normals.Allocate(vertCount);
			for (auto i = 0; i < vertCount; ++i)
				ar & normals[i];
		} else
			normals = NormalBuffer();
		triNormals.Allocate(triCount);

		for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
			uvs.Deserialize(i, ar, vertCount);
			cols.Deserialize(i, ar, vertCount);
			alphas.Deserialize(i, ar, vertCount);
			vertAOV.Deserialize(i, ar, vertCount);
			triAOV.Deserialize(i, ar, triCount);
		}

		bevelCylinders = nullptr;
		bevelBoundingCylinders = nullptr;
		bevelBVHArrayNodes = nullptr;

		// Rebuild the compacted (mapped) vertex buffer from the Shape's
		// canonical buffer and the vertex map
		mappedVertices.Allocate(vertexMap.size());
		const auto shapeVerts = GetShape().GetVertices();
		for (size_t i = 0; i < vertexMap.size(); ++i)
			mappedVertices[i] = shapeVerts[vertexMap[i]];

		TriangleMesh::Preprocess();
		Preprocess();
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	NormalBuffer normals; // Vertices normals
	NormalBuffer triNormals; // Triangle normals

	ExtMeshProp<UV> uvs; // Vertex uvs
	ExtMeshProp<Spectrum> cols; // Vertex colors
	ExtMeshProp<float> alphas; // Vertex alphas

	ExtMeshProp<float> vertAOV; // Vertex AOV
	ExtMeshProp<float> triAOV; // Triangle AOV

	// Imported creases (edge sharpness, OpenSubdiv units)
	std::vector<u_int> creasePairs;    // flat (v0, v1) pairs
	std::vector<float> creaseWeights;  // one sharpness per pair

	std::vector<u_int> vertexMap;      // local->global vertex map (see above)
	VertexBuffer mappedVertices;       // compacted positions (mappedVertices[local])

	std::unique_ptr<BevelCylinder[]> bevelCylinders;
	std::unique_ptr<BevelBoundingCylinder[]> bevelBoundingCylinders;
	std::unique_ptr<luxrays::ocl::IndexBVHArrayNode[]> bevelBVHArrayNodes;
};

class ExtInstanceTriangleMesh : public InstanceTriangleMesh, public ExtMesh {
public:
	ExtInstanceTriangleMesh(ExtTriangleMesh& m, const Transform &t) :
		Mesh(m.GetShape()), InstanceTriangleMesh(m, t), ExtMesh(m.GetShape()) { }
	~ExtInstanceTriangleMesh() { };
	void Delete() override {	}

	MeshType GetType() const override { return TYPE_EXT_TRIANGLE_INSTANCE; }

	float GetBevelRadius() const override { return static_cast<const ExtTriangleMesh&>(*mesh).GetBevelRadius(); }

	bool HasNormals() const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasNormals(); }
	bool HasUVs(const u_int layerIndex) const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasUVs(layerIndex); }
	bool HasColors(const u_int layerIndex) const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasColors(layerIndex); }
	bool HasAlphas(const u_int layerIndex) const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasAlphas(layerIndex); }

	bool HasVertexAOV(const u_int layerIndex) const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasVertexAOV(layerIndex); }
	bool HasTriAOV(const u_int layerIndex) const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasTriAOV(layerIndex); }

	Normal GetGeometryNormal(const luxrays::Transform &local2World, const u_int triIndex) const override {
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetGeometryNormal(Transform::TRANS_IDENTITY, triIndex));
	}
	Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int triIndex, const u_int vertIndex) const override {
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetShadeNormal(Transform::TRANS_IDENTITY, triIndex, vertIndex));
	}
	Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int vertIndex) const override {
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetShadeNormal(Transform::TRANS_IDENTITY, vertIndex));
	}
	UV GetUV(const unsigned vertIndex, const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetUV(vertIndex, layerIndex);
	}
	Spectrum GetColor(const unsigned vertIndex, const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetColor(vertIndex, layerIndex);
	}
	float GetAlpha(const unsigned vertIndex, const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetAlpha(vertIndex, layerIndex);
	}

	float GetVertexAOV(const unsigned vertIndex, const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetVertexAOV(vertIndex, layerIndex);
	}
	float GetTriAOV(const unsigned triIndex, const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetTriAOV(triIndex, layerIndex);
	}

	bool GetTriBaryCoords(const luxrays::Transform &local2World, const u_int triIndex,
			const Point &hitPoint, float *b1, float *b2) const override;

	Normal InterpolateTriNormal(const luxrays::Transform &local2World,
			const u_int triIndex, const float b1, const float b2) const override {
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(trans * static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriNormal(
				Transform::TRANS_IDENTITY, triIndex, b1, b2));
	}

	UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriUV(triIndex,
				b1, b2, layerIndex);
	}

	Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriColor(triIndex,
				b1, b2, layerIndex);
	}

	float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriAlpha(triIndex,
				b1, b2, layerIndex);
	}

	float InterpolateTriVertexAOV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriVertexAOV(triIndex,
				b1, b2, layerIndex);
	}

	bool IntersectBevel(const luxrays::Ray &ray, const luxrays::RayHit &rayHit,
			bool &continueToTrace, float &rayHitT,
			luxrays::Point &p, luxrays::Normal &n) const override;

	void Save(const std::string &fileName) const override { static_cast<const ExtTriangleMesh&>(*mesh).Save(fileName); }

	const Transform &GetTransformation() const { return trans; }
	const ExtTriangleMesh& GetExtTriangleMesh() const { return static_cast<const ExtTriangleMesh&>(*mesh); };

	void UpdateMeshReferences(const ExtTriangleMesh& oldMesh, ExtTriangleMesh& newMesh);

	friend class boost::serialization::access;

private:
	// Used by serialization (see boost::serialization load_construct_data)
	friend class boost::serialization::access;

public:
	explicit ExtInstanceTriangleMesh(const Shape &s) : Mesh(s), InstanceTriangleMesh(s), ExtMesh(s) { }

private:

	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(InstanceTriangleMesh);
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ExtMesh);
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(InstanceTriangleMesh);
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ExtMesh);
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()
};

class ExtMotionTriangleMesh : public MotionTriangleMesh, public ExtMesh {
public:
	ExtMotionTriangleMesh(ExtTriangleMesh& m, const MotionSystem &ms) :
		Mesh(m.GetShape()), MotionTriangleMesh(m, ms), ExtMesh(m.GetShape()) { }
	~ExtMotionTriangleMesh() { }
	void Delete() override {	}

	MeshType GetType() const override { return TYPE_EXT_TRIANGLE_MOTION; }

	float GetBevelRadius() const override { return static_cast<const ExtTriangleMesh&>(*mesh).GetBevelRadius(); }

	bool HasNormals() const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasNormals(); }
	bool HasUVs(const u_int layerIndex) const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasUVs(layerIndex); }
	bool HasColors(const u_int layerIndex) const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasColors(layerIndex); }
	bool HasAlphas(const u_int layerIndex) const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasAlphas(layerIndex); }

	bool HasVertexAOV(const u_int layerIndex) const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasVertexAOV(layerIndex); }
	bool HasTriAOV(const u_int layerIndex) const override { return static_cast<const ExtTriangleMesh&>(*mesh).HasTriAOV(layerIndex); }

	Normal GetGeometryNormal(const luxrays::Transform &local2World, const u_int triIndex) const override {
		const bool transSwapsHandedness = local2World.SwapsHandedness();
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetGeometryNormal(local2World, triIndex));
	}
	Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int triIndex, const u_int vertIndex) const override {
		const bool transSwapsHandedness = local2World.SwapsHandedness();
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetShadeNormal(local2World, triIndex, vertIndex));
	}
	Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int vertIndex) const override {
		const bool transSwapsHandedness = local2World.SwapsHandedness();
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetShadeNormal(local2World, vertIndex));
	}
	UV GetUV(const unsigned vertIndex, const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetUV(vertIndex, layerIndex);
	}
	Spectrum GetColor(const unsigned vertIndex, const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetColor(vertIndex, layerIndex);
	}
	float GetAlpha(const unsigned vertIndex, const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetAlpha(vertIndex, layerIndex);
	}

	float GetVertexAOV(const unsigned vertIndex, const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetVertexAOV(vertIndex, layerIndex);
	}
	float GetTriAOV(const unsigned triIndex, const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetTriAOV(triIndex, layerIndex);
	}

	bool GetTriBaryCoords(const luxrays::Transform &local2World, const u_int triIndex,
			const Point &hitPoint, float *b1, float *b2) const override;

	Normal InterpolateTriNormal(const luxrays::Transform &local2World,
			const u_int triIndex, const float b1, const float b2) const override {
		const bool transSwapsHandedness = local2World.SwapsHandedness();
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriNormal(
				local2World, triIndex, b1, b2));
	}

	UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriUV(triIndex,
				b1, b2, layerIndex);
	}

	Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriColor(triIndex,
				b1, b2, layerIndex);
	}

	float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriAlpha(triIndex,
				b1, b2, layerIndex);
	}

	float InterpolateTriVertexAOV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const override {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriVertexAOV(triIndex,
				b1, b2, layerIndex);
	}

	void Save(const std::string &fileName) const override { static_cast<const ExtTriangleMesh&>(*mesh).Save(fileName); }

	const MotionSystem &GetMotionSystem() const { return motionSystem; }
	const ExtTriangleMesh& GetExtTriangleMesh() const { return static_cast<const ExtTriangleMesh&>(*mesh); };

	void UpdateMeshReferences(const ExtTriangleMesh& oldMesh, ExtTriangleMesh& newMesh);

	friend class boost::serialization::access;

private:
	// Used by serialization (see boost::serialization load_construct_data)
	friend class boost::serialization::access;

public:
	explicit ExtMotionTriangleMesh(const Shape &s) : Mesh(s), MotionTriangleMesh(s), ExtMesh(s) { }

private:

	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(MotionTriangleMesh);
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ExtMesh);
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(MotionTriangleMesh);
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ExtMesh);
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()
};

}

BOOST_SERIALIZATION_ASSUME_ABSTRACT(luxrays::ExtMesh)

BOOST_CLASS_VERSION(luxrays::ExtTriangleMesh, 4)
BOOST_CLASS_VERSION(luxrays::ExtInstanceTriangleMesh, 4)
BOOST_CLASS_VERSION(luxrays::ExtMotionTriangleMesh, 4)

BOOST_CLASS_EXPORT_KEY(luxrays::ExtTriangleMesh)
BOOST_CLASS_EXPORT_KEY(luxrays::ExtInstanceTriangleMesh)
BOOST_CLASS_EXPORT_KEY(luxrays::ExtMotionTriangleMesh)

// Reference member serialization (see trianglemesh.h)
namespace boost {
namespace serialization {

template<class Archive>
inline void save_construct_data(Archive &ar, const luxrays::ExtTriangleMesh *t, const unsigned int version) {
	ar << const_cast<luxrays::Shape *>(&t->GetShape());
}
template<class Archive>
inline void load_construct_data(Archive &ar, luxrays::ExtTriangleMesh *t, const unsigned int version) {
	luxrays::Shape *s = nullptr;
	ar >> s;
	::new(t) luxrays::ExtTriangleMesh(*s);
}

template<class Archive>
inline void save_construct_data(Archive &ar, const luxrays::ExtInstanceTriangleMesh *t, const unsigned int version) {
	ar << const_cast<luxrays::Shape *>(&t->GetShape());
}
template<class Archive>
inline void load_construct_data(Archive &ar, luxrays::ExtInstanceTriangleMesh *t, const unsigned int version) {
	luxrays::Shape *s = nullptr;
	ar >> s;
	::new(t) luxrays::ExtInstanceTriangleMesh(*s);
}

template<class Archive>
inline void save_construct_data(Archive &ar, const luxrays::ExtMotionTriangleMesh *t, const unsigned int version) {
	ar << const_cast<luxrays::Shape *>(&t->GetShape());
}
template<class Archive>
inline void load_construct_data(Archive &ar, luxrays::ExtMotionTriangleMesh *t, const unsigned int version) {
	luxrays::Shape *s = nullptr;
	ar >> s;
	::new(t) luxrays::ExtMotionTriangleMesh(*s);
}

} // namespace serialization
} // namespace boost

#endif	/* _LUXRAYS_EXTTRIANGLEMESH_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
