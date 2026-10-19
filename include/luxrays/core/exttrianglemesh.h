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

#ifndef _LUXRAYS_EXTTRIANGLEMESH_H
#define	_LUXRAYS_EXTTRIANGLEMESH_H

#include <cassert>
#include <cstdlib>
#include <array>
#include <memory>
#include <span>
#include <execution>

#include <boost/lexical_cast.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/array.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/bvh/bvhbuild.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/geometry/triangle.h"
#include "luxrays/core/geometry/frame.h"
#include "luxrays/core/geometry/motionsystem.h"
#include "luxrays/core/trianglemesh.h"
#include "luxrays/core/namedobject.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/serializationutils.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/exttrianglemesh_types.cl"
}

class Ray;
class RayHit;

// ExtMeshProp: a container for a property of Extended Mesh (uv, colors, alphas...)
//
// We use shared pointer, as it allows shallow copy (can be useful for such
// large data sets)
// All methods run on a per layer basis, except those suffixed by 'All', which
// run on all layers
template <typename T>
class ExtMeshProp : std::array<std::shared_ptr<T[]>, EXTMESH_MAX_DATA_COUNT> {
public:
	using Layer = std::shared_ptr<T[]>;
	using Base = std::array<Layer, EXTMESH_MAX_DATA_COUNT>;

	ExtMeshProp() { this->fill(nullptr); }

	// Construct from a single layer of data
	ExtMeshProp(Layer layer) : ExtMeshProp() {
		if (layer) (*this)[0] = layer;
	}

	// Per-layer
	void AllocateLayer(const u_int layerIndex, size_t layerSize) {
		assert(!_size || _size == layerSize);  // Should always be the same size once it
										  // has been set
		// A zero-size array is not null: keep the layer null or LayerHasValues
		// would report it as having values
		(*this)[layerIndex] = (layerSize > 0) ? std::make_shared<T[]>(layerSize) : Layer(nullptr);
		if (layerSize) _size = layerSize;
	}
	auto GetLayer(const u_int layerIndex) const {
		assert(layerIndex < GetMaxLayerNumber());
		return (*this)[layerIndex];
	}
	void SetLayer(const u_int layerIndex, Layer values, size_t layerSize) {
		assert(!_size || _size == layerSize);
		(*this)[layerIndex] = values;
		if (layerSize) _size = layerSize;
	}
	void SetLayer(const u_int layerIndex, std::tuple<Layer, size_t> in) {
		auto [layer, layerSize] = in;
		SetLayer(layerIndex, layer, layerSize);
	}
	void SetLayer(const u_int layerIndex, std::span<T> in) {
		auto layerSize = in.size();
		assert(!_size || _size == layerSize);

		AllocateLayer(layerIndex, layerSize);
		auto layerSpan = GetLayerSpan(layerIndex);

		std::copy(
#if !defined(__clang__)
			std::execution::par,
#endif
			in.begin(),
			in.end(),
			layerSpan.begin()
		);
		if (layerSize) _size = layerSize;
	}

	void DeleteLayer(const u_int layerIndex) {
		(*this)[layerIndex].reset();
		(*this)[layerIndex] = nullptr;
	}
	bool LayerHasValues(const u_int layerIndex) const {
		return (*this)[layerIndex] != nullptr;
	}

	std::span<T> GetLayerSpan(const u_int layerIndex) const {
		auto& layer = (*this)[layerIndex];
		if (!layer) return std::span<T>();
		return std::span<T>(layer.get(), _size);
	}

	std::tuple<Layer, size_t> CopyLayer(
		const u_int layerIndex,
		const std::optional<ExtMeshProp<T>> force = std::nullopt
	) const {
		if (force) {
			return std::make_tuple(force->GetLayer(layerIndex), force->GetLayerSize());
		}

		if (LayerHasValues(layerIndex)) {
			auto res = std::make_shared<T[]>(GetLayerSize());

			auto srcSpan = GetLayerSpan(layerIndex);
			auto dstSpan = std::span(res.get(), GetLayerSize());
			std::copy(
#if !defined(__clang__)
				std::execution::par,
#endif
				srcSpan.begin(),
				srcSpan.end(),
				dstSpan.begin()
			);

			return std::make_tuple(res, GetLayerSize());
		}

		return std::make_tuple(Layer(nullptr), 0);
	};

	// All layers
	void DeleteAll() {
		for (size_t i = 0; i < this->size(); ++i)
			DeleteLayer(i);
	}
	size_t GetLayerSize() const { return _size; }

	// Serialization
	template<typename Archive>
	void Serialize(const u_int layerIndex, Archive& ar, size_t count) const {
		const bool hasValues = LayerHasValues(layerIndex);
		ar & hasValues;
		if (hasValues) {
			auto data = (*this)[layerIndex].get();
			ar & boost::serialization::make_array(data, count);
		}
	}

	template<typename Archive>
	void Deserialize(const u_int layerIndex, Archive& ar, size_t count) {
		bool hasValues;
		ar & hasValues;
		if (hasValues) {
			AllocateLayer(layerIndex, count);
			auto data = (*this)[layerIndex].get();
			ar & boost::serialization::make_array(data, count);
		} else {
			(*this)[layerIndex] = nullptr;
		}
	}

	// Base
	using Base::operator[];  // TODO Make private
	auto GetMaxLayerNumber() const { return Base::size(); }  // Size of the array

private:
	size_t _size = 0;

};


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
	ExtMesh(float bRadius = 0.f) : bevelRadius(bRadius) { }
	virtual ~ExtMesh() = default;

	virtual float GetBevelRadius() const { return bevelRadius; }
	virtual bool IntersectBevel(const luxrays::Ray &ray, const luxrays::RayHit &rayHit,
			bool &continueToTrace, float &rayHitT,
			luxrays::Point &p, luxrays::Normal &n) const {
		continueToTrace = false;
		return false;
	}

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

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Mesh);
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(NamedObject);

		ar & bevelRadius;
	}
};

class ExtTriangleMesh : public TriangleMesh, public ExtMesh {
public:
	ExtTriangleMesh(
		VertexBuffer&& meshVertices,
		TriangleBuffer&& meshTris,
		NormalBuffer&& meshNormals,
		ExtMeshProp<UV>::Layer meshUVs = nullptr,
		ExtMeshProp<Spectrum>::Layer meshCols = nullptr,
		ExtMeshProp<float>::Layer meshAlphas = nullptr,
		const float bRadius = 0.f
	);
	ExtTriangleMesh(
		VertexBuffer&& meshVertices,
		TriangleBuffer&& meshTris,
		NormalBuffer&& meshNormals,
		std::optional<std::span<UV>> meshUVs,
		std::optional<std::span<Spectrum>> meshCols,
		std::optional<std::span<float>> meshAlphas,
		const float bRadius = 0.f
	);
	ExtTriangleMesh(
		VertexBuffer&& meshVertices,
		TriangleBuffer&& meshTris,
		NormalBuffer&& meshNormals,
		std::optional<ExtMeshProp<UV>> meshUVs,
		std::optional<ExtMeshProp<Spectrum>> meshCols,
		std::optional<ExtMeshProp<float>> meshAlphas,
		const float bRadius = 0.f
	);
	~ExtTriangleMesh() { };
	virtual void Delete();

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
	virtual bool HasVertexAOV(const u_int layerIndex) const {
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
	virtual bool HasTriAOV(const u_int layerIndex) const {
		return triAOV.LayerHasValues(layerIndex);
	}

	// UV
	void SetUVs(const u_int layerIndex, std::shared_ptr<UV[]> values, size_t size) {
		uvs.SetLayer(layerIndex, values, size);
	}
	void DeleteUVs(const u_int layerIndex) { uvs.DeleteLayer(layerIndex); }
	auto GetUVs(const u_int layerIndex) const { return uvs.GetLayer(layerIndex); }
	virtual bool HasUVs(const u_int layerIndex) const { return uvs.LayerHasValues(layerIndex); }
	const auto& GetAllUVs() const { return uvs; }

	// Vertex colors
	void SetColors(const u_int layerIndex, std::shared_ptr<Spectrum[]> values, size_t size) {
		cols.SetLayer(layerIndex, values, size);
	}
	void DeleteColors(const u_int layerIndex) { cols.DeleteLayer(layerIndex); }
	auto GetColors(const u_int layerIndex) const { return cols.GetLayer(layerIndex); }
	virtual bool HasColors(const u_int layerIndex) const {
		return cols.LayerHasValues(layerIndex);
	}
	const auto& GetAllColors() const { return cols; }

	// Vertex alphas
	void SetAlphas(const u_int layerIndex, std::shared_ptr<float[]> values, size_t size) {
		alphas.SetLayer(layerIndex, values, size);
	}
	void DeleteAlphas(const u_int layerIndex) { alphas.DeleteLayer(layerIndex); }
	auto GetAlphas(const u_int layerIndex) const { return alphas.GetLayer(layerIndex); }
	virtual bool HasAlphas(const u_int layerIndex) const { return alphas.LayerHasValues(layerIndex); }
	const auto& GetAllAlphas() const { return alphas; }


	// Computes the vertex normals in place: the normal buffer of the mesh is
	// always valid afterwards
	void ComputeNormals();

	virtual MeshType GetType() const { return TYPE_EXT_TRIANGLE; }

	virtual bool HasNormals() const { return bool(normals); }

	virtual Normal GetGeometryNormal(const luxrays::Transform &local2World, const u_int triIndex) const {
		// Pre-computed geometry normals already factor appliedTransSwapsHandedness
		return triNormals[triIndex];
	}
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int triIndex, const u_int vertIndex) const {
		return (appliedTransSwapsHandedness ? -1.f : 1.f) * normals[tris[triIndex].v[vertIndex]];
	}
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int vertIndex) const {
		return (appliedTransSwapsHandedness ? -1.f : 1.f) * normals[vertIndex];
	}

	virtual UV GetUV(const u_int vertIndex, const u_int layerIndex) const { return uvs[layerIndex][vertIndex]; }
	virtual Spectrum GetColor(const u_int vertIndex, const u_int layerIndex) const { return cols[layerIndex][vertIndex]; }
	virtual float GetAlpha(const u_int vertIndex, const u_int layerIndex) const {
		assert(vertIndex < alphas.GetLayerSize());
		return alphas[layerIndex][vertIndex];
	}
	virtual float GetVertexAOV(const u_int vertIndex, const u_int layerIndex) const {
		if (HasTriAOV(layerIndex))
			return vertAOV[layerIndex][vertIndex];
		else
			return 0.f;
	}
	virtual float GetTriAOV(const u_int triIndex, const u_int layerIndex) const {
		if (HasTriAOV(layerIndex))
			return triAOV[layerIndex][triIndex];
		else
			return 0.f;
	}

	virtual bool GetTriBaryCoords(const luxrays::Transform &local2World, const u_int triIndex, const Point &hitPoint, float *b1, float *b2) const {
		const Triangle &tri = tris[triIndex];
		return tri.GetBaryCoords(vertices, hitPoint, b1, b2);
	}
	void SetLocal2World(const luxrays::Transform &t) {
		appliedTrans = t;
		appliedTransSwapsHandedness = appliedTrans.SwapsHandedness();
	}

	virtual void ApplyTransform(const Transform &trans);

	virtual Normal InterpolateTriNormal(const luxrays::Transform &local2World, const u_int triIndex,
			const float b1, const float b2) const {
		if (not bool(normals))
			return GetGeometryNormal(local2World, triIndex);
		const Triangle &tri = tris[triIndex];
		const float b0 = 1.f - b1 - b2;
		return (appliedTransSwapsHandedness ? -1.f : 1.f) * Normalize(b0 * normals[tri.v[0]] + b1 * normals[tri.v[1]] + b2 * normals[tri.v[2]]);
	}

	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		if (HasUVs(layerIndex)) {
			const Triangle &tri = tris[triIndex];
			const float b0 = 1.f - b1 - b2;
			return b0 * uvs[layerIndex][tri.v[0]] + b1 * uvs[layerIndex][tri.v[1]] + b2 * uvs[layerIndex][tri.v[2]];
		} else
			return UV(0.f, 0.f);
	}

	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		if (HasColors(layerIndex)) {
			const Triangle &tri = tris[triIndex];
			const float b0 = 1.f - b1 - b2;
			return b0 * cols[layerIndex][tri.v[0]] + b1 * cols[layerIndex][tri.v[1]] + b2 * cols[layerIndex][tri.v[2]];
		} else
			return Spectrum(1.f);
	}

	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		if (HasAlphas(layerIndex)) {
			const Triangle &tri = tris[triIndex];
			const float b0 = 1.f - b1 - b2;
			return b0 * alphas[layerIndex][tri.v[0]] + b1 * alphas[layerIndex][tri.v[1]] + b2 * alphas[layerIndex][tri.v[2]];
		} else
			return 1.f;
	}
	
	virtual float InterpolateTriVertexAOV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		if (HasVertexAOV(layerIndex)) {
			const Triangle &tri = tris[triIndex];
			const float b0 = 1.f - b1 - b2;
			return b0 * vertAOV[layerIndex][tri.v[0]] + b1 * vertAOV[layerIndex][tri.v[1]] + b2 * vertAOV[layerIndex][tri.v[2]];
		} else
			return 0.f;
	}

	virtual void Save(const std::string &fileName) const;

	void CopyAOV(ExtTriangleMeshRef destMesh) const;

	ExtTriangleMeshUPtr CopyExt(
		std::optional<VertexBuffer> meshVertices,
		std::optional<TriangleBuffer> meshTris,
		std::optional<NormalBuffer> meshNormals,
		std::optional<ExtMeshProp<UV>> meshUVs,
		std::optional<ExtMeshProp<Spectrum>> meshCols,
		std::optional<ExtMeshProp<float>> meshAlphas,
		const float bRadius = 0.f
	) const;

	ExtTriangleMeshUPtr Copy(
		std::optional<VertexBuffer> meshVertices,
		std::optional<TriangleBuffer> meshTris,
		std::optional<NormalBuffer> meshNormals,
		std::optional<std::span<UV>> mUVs,
		std::optional<std::span<Spectrum>> mCols,
		std::optional<std::span<float>> mAlphas,
		const float bRadius = 0.f
	) const;

	ExtTriangleMeshUPtr Copy(const float bRadius = 0.f) const {
		return CopyExt(
			std::nullopt,
			std::nullopt,
			std::nullopt,
			std::nullopt,
			std::nullopt,
			std::nullopt,
			bRadius
		);
	}

	virtual bool IntersectBevel(const luxrays::Ray &ray, const luxrays::RayHit &rayHit,
			bool &continueToTrace, float &rayHitT,
			luxrays::Point &p, luxrays::Normal &n) const;

	static ExtTriangleMeshUPtr Load(const std::string &fileName);

	static ExtTriangleMeshUPtr Merge(
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

	static ExtTriangleMeshUPtr LoadPly(const std::string &fileName);
	static ExtTriangleMeshUPtr LoadSerialized(const std::string &fileName);

	// Used by serialization
	ExtTriangleMesh() {
	}

	void Init(
		NormalBuffer&& meshNormals,
		std::optional<ExtMeshProp<UV>> meshUVs,
		std::optional<ExtMeshProp<Spectrum>> meshCols,
		std::optional<ExtMeshProp<float>> meshAlphas
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
		auto vertCount = vertices.Count();
		auto triCount = tris.Count();
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
		auto vertCount = vertices.Count();
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

	BevelCylinder *bevelCylinders;
	BevelBoundingCylinder *bevelBoundingCylinders;
	std::unique_ptr<luxrays::ocl::IndexBVHArrayNode[]> bevelBVHArrayNodes;
};

class ExtInstanceTriangleMesh : public InstanceTriangleMesh, public ExtMesh {
public:
	ExtInstanceTriangleMesh(ExtTriangleMesh& m, const Transform &t) : 
		InstanceTriangleMesh(m, t) { }
	~ExtInstanceTriangleMesh() { };
	virtual void Delete() {	}

	virtual MeshType GetType() const { return TYPE_EXT_TRIANGLE_INSTANCE; }
	
	virtual float GetBevelRadius() const { return static_cast<const ExtTriangleMesh&>(*mesh).GetBevelRadius(); }

	virtual bool HasNormals() const { return static_cast<const ExtTriangleMesh&>(*mesh).HasNormals(); }
	virtual bool HasUVs(const u_int layerIndex) const { return static_cast<const ExtTriangleMesh&>(*mesh).HasUVs(layerIndex); }
	virtual bool HasColors(const u_int layerIndex) const { return static_cast<const ExtTriangleMesh&>(*mesh).HasColors(layerIndex); }
	virtual bool HasAlphas(const u_int layerIndex) const { return static_cast<const ExtTriangleMesh&>(*mesh).HasAlphas(layerIndex); }
	
	virtual bool HasVertexAOV(const u_int layerIndex) const { return static_cast<const ExtTriangleMesh&>(*mesh).HasVertexAOV(layerIndex); }
	virtual bool HasTriAOV(const u_int layerIndex) const { return static_cast<const ExtTriangleMesh&>(*mesh).HasTriAOV(layerIndex); }

	virtual Normal GetGeometryNormal(const luxrays::Transform &local2World, const u_int triIndex) const {
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetGeometryNormal(Transform::TRANS_IDENTITY, triIndex));
	}
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int triIndex, const u_int vertIndex) const {
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetShadeNormal(Transform::TRANS_IDENTITY, triIndex, vertIndex));
	}
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int vertIndex) const {
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetShadeNormal(Transform::TRANS_IDENTITY, vertIndex));
	}
	virtual UV GetUV(const unsigned vertIndex, const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetUV(vertIndex, layerIndex);
	}
	virtual Spectrum GetColor(const unsigned vertIndex, const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetColor(vertIndex, layerIndex);
	}
	virtual float GetAlpha(const unsigned vertIndex, const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetAlpha(vertIndex, layerIndex);
	}

	virtual float GetVertexAOV(const unsigned vertIndex, const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetVertexAOV(vertIndex, layerIndex);
	}
	virtual float GetTriAOV(const unsigned triIndex, const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetTriAOV(triIndex, layerIndex);
	}

	virtual bool GetTriBaryCoords(const luxrays::Transform &local2World, const u_int triIndex,
			const Point &hitPoint, float *b1, float *b2) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::GetBaryCoords(
				GetVertex(local2World, tri.v[0]),
				GetVertex(local2World, tri.v[1]),
				GetVertex(local2World, tri.v[2]),
				hitPoint, b1, b2);
	}

	virtual Normal InterpolateTriNormal(const luxrays::Transform &local2World,
			const u_int triIndex, const float b1, const float b2) const {
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(trans * static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriNormal(
				Transform::TRANS_IDENTITY, triIndex, b1, b2));
	}

	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriUV(triIndex,
				b1, b2, layerIndex);
	}

	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriColor(triIndex,
				b1, b2, layerIndex);
	}
	
	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriAlpha(triIndex,
				b1, b2, layerIndex);
	}
	
	virtual float InterpolateTriVertexAOV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriVertexAOV(triIndex,
				b1, b2, layerIndex);
	}

	virtual bool IntersectBevel(const luxrays::Ray &ray, const luxrays::RayHit &rayHit,
			bool &continueToTrace, float &rayHitT,
			luxrays::Point &p, luxrays::Normal &n) const;

	virtual void Save(const std::string &fileName) const { static_cast<const ExtTriangleMesh&>(*mesh).Save(fileName); }

	const Transform &GetTransformation() const { return trans; }
	const ExtTriangleMesh& GetExtTriangleMesh() const { return static_cast<const ExtTriangleMesh&>(*mesh); };
	
	void UpdateMeshReferences(const ExtTriangleMesh& oldMesh, ExtTriangleMesh& newMesh);

	friend class boost::serialization::access;

private:
	// Used by serialization
	ExtInstanceTriangleMesh() {
	}

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
		MotionTriangleMesh(m, ms) { }
	~ExtMotionTriangleMesh() { }
	virtual void Delete() {	}

	virtual MeshType GetType() const { return TYPE_EXT_TRIANGLE_MOTION; }

	virtual float GetBevelRadius() const { return static_cast<const ExtTriangleMesh&>(*mesh).GetBevelRadius(); }

	virtual bool HasNormals() const { return static_cast<const ExtTriangleMesh&>(*mesh).HasNormals(); }
	virtual bool HasUVs(const u_int layerIndex) const { return static_cast<const ExtTriangleMesh&>(*mesh).HasUVs(layerIndex); }
	virtual bool HasColors(const u_int layerIndex) const { return static_cast<const ExtTriangleMesh&>(*mesh).HasColors(layerIndex); }
	virtual bool HasAlphas(const u_int layerIndex) const { return static_cast<const ExtTriangleMesh&>(*mesh).HasAlphas(layerIndex); }

	virtual bool HasVertexAOV(const u_int layerIndex) const { return static_cast<const ExtTriangleMesh&>(*mesh).HasVertexAOV(layerIndex); }
	virtual bool HasTriAOV(const u_int layerIndex) const { return static_cast<const ExtTriangleMesh&>(*mesh).HasTriAOV(layerIndex); }

	virtual Normal GetGeometryNormal(const luxrays::Transform &local2World, const u_int triIndex) const {
		const bool transSwapsHandedness = local2World.SwapsHandedness();
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetGeometryNormal(local2World, triIndex));
	}
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int triIndex, const u_int vertIndex) const {
		const bool transSwapsHandedness = local2World.SwapsHandedness();
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetShadeNormal(local2World, triIndex, vertIndex));
	}
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int vertIndex) const {
		const bool transSwapsHandedness = local2World.SwapsHandedness();
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).GetShadeNormal(local2World, vertIndex));
	}
	virtual UV GetUV(const unsigned vertIndex, const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetUV(vertIndex, layerIndex);
	}
	virtual Spectrum GetColor(const unsigned vertIndex, const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetColor(vertIndex, layerIndex);
	}
	virtual float GetAlpha(const unsigned vertIndex, const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetAlpha(vertIndex, layerIndex);
	}

	virtual float GetVertexAOV(const unsigned vertIndex, const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetVertexAOV(vertIndex, layerIndex);
	}
	virtual float GetTriAOV(const unsigned triIndex, const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).GetTriAOV(triIndex, layerIndex);
	}

	virtual bool GetTriBaryCoords(const luxrays::Transform &local2World, const u_int triIndex,
			const Point &hitPoint, float *b1, float *b2) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::GetBaryCoords(GetVertex(local2World, tri.v[0]),
				GetVertex(local2World, tri.v[1]), GetVertex(local2World, tri.v[2]),
				hitPoint, b1, b2);
	}

	virtual Normal InterpolateTriNormal(const luxrays::Transform &local2World,
			const u_int triIndex, const float b1, const float b2) const {
		const bool transSwapsHandedness = local2World.SwapsHandedness();
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriNormal(
				local2World, triIndex, b1, b2));
	}

	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriUV(triIndex,
				b1, b2, layerIndex);
	}
	
	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriColor(triIndex,
				b1, b2, layerIndex);
	}
	
	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriAlpha(triIndex,
				b1, b2, layerIndex);
	}

	virtual float InterpolateTriVertexAOV(const u_int triIndex, const float b1, const float b2,
			const u_int layerIndex) const {
		return static_cast<const ExtTriangleMesh&>(*mesh).InterpolateTriVertexAOV(triIndex,
				b1, b2, layerIndex);
	}

	virtual void Save(const std::string &fileName) const { static_cast<const ExtTriangleMesh&>(*mesh).Save(fileName); }

	const MotionSystem &GetMotionSystem() const { return motionSystem; }
	const ExtTriangleMesh& GetExtTriangleMesh() const { return static_cast<const ExtTriangleMesh&>(*mesh); };

	void UpdateMeshReferences(const ExtTriangleMesh& oldMesh, ExtTriangleMesh& newMesh);

	friend class boost::serialization::access;

private:
	// Used by serialization
	ExtMotionTriangleMesh() {
	}

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

#endif	/* _LUXRAYS_EXTTRIANGLEMESH_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
