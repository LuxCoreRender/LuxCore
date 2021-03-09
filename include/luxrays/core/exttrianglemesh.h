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

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

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
	ExtMesh() : bevelRadius(0.f) { }
	virtual ~ExtMesh() { }

	virtual float GetBevelRadius() const { return bevelRadius; }
	virtual bool IntersectBevel(const luxrays::Ray &ray, const luxrays::RayHit &rayHit,
			bool &continueToTrace, float &rayHitT,
			luxrays::Point &p, luxrays::Normal &n) const {
		continueToTrace = false;
		return false;
	}

	virtual bool HasNormals() const = 0;
	virtual bool HasUVs(const u_int dataIndex) const = 0;
	virtual bool HasColors(const u_int dataIndex) const = 0;
	virtual bool HasAlphas(const u_int dataIndex) const = 0;
	
	virtual bool HasVertexAOV(const u_int dataIndex) const = 0;
	virtual bool HasTriAOV(const u_int dataIndex) const = 0;
	
	virtual Normal GetGeometryNormal(const luxrays::Transform &local2World, const u_int triIndex) const = 0;
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int triIndex, const u_int vertIndex) const = 0;
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int vertIndex) const = 0;

	virtual UV GetUV(const u_int vertIndex, const u_int dataIndex) const = 0;
	virtual Spectrum GetColor(const u_int vertIndex, const u_int dataIndex) const = 0;
	virtual float GetAlpha(const u_int vertIndex, const u_int dataIndex) const = 0;

	virtual float GetVertexAOV(const u_int vertIndex, const u_int dataIndex) const = 0;
	virtual float GetTriAOV(const u_int triIndex, const u_int dataIndex) const = 0;
	
	virtual bool GetTriBaryCoords(const luxrays::Transform &local2World, const u_int triIndex, const Point &hitPoint, float *b1, float *b2) const = 0;
    virtual void GetDifferentials(const luxrays::Transform &local2World,
			const u_int triIndex, const Normal &shadeNormal, const u_int dataIndex,
			Vector *dpdu, Vector *dpdv,
			Normal *dndu, Normal *dndv) const;

	virtual Normal InterpolateTriNormal(const luxrays::Transform &local2World,
			const u_int triIndex, const float b1, const float b2) const = 0;
	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const = 0;
	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const = 0;
	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const = 0;

	virtual float InterpolateTriVertexAOV(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const = 0;

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
	ExtTriangleMesh(const u_int meshVertCount, const u_int meshTriCount,
			Point *meshVertices, Triangle *meshTris, Normal *meshNormals = nullptr,
			UV *meshUVs = nullptr, Spectrum *meshCols = nullptr, float *meshAlphas = nullptr,
			const float bRadius = 0.f);
	ExtTriangleMesh(const u_int meshVertCount, const u_int meshTriCount,
			Point *meshVertices, Triangle *meshTris, Normal *meshNormals,
			std::array<UV *, EXTMESH_MAX_DATA_COUNT> *meshUVs,
			std::array<Spectrum *, EXTMESH_MAX_DATA_COUNT> *meshCols,
			std::array<float *, EXTMESH_MAX_DATA_COUNT> *meshAlphas,
			const float bRadius = 0.f);
	~ExtTriangleMesh() { };
	virtual void Delete();

	void SetVertexAOV(const u_int dataIndex, float *values) {
		vertAOV[dataIndex] = values;
	}
	void DeleteVertexAOV(const u_int dataIndex) {
		delete[] vertAOV[dataIndex];
		vertAOV[dataIndex] = nullptr;
	}
	void SetTriAOV(const u_int dataIndex, float *values) {
		triAOV[dataIndex] = values;
	}
	void DeleteTriAOV(const u_int dataIndex) {
		delete[] triAOV[dataIndex];
		triAOV[dataIndex] = nullptr;
	}

	Normal *GetNormals() const { return normals; }
	Normal *GetTriNormals() const { return triNormals; }

	UV *GetUVs(const u_int dataIndex) const { return uvs[dataIndex]; }
	Spectrum *GetColors(const u_int dataIndex) const { return cols[dataIndex]; }
	float *GetAlphas(const u_int dataIndex) const { return alphas[dataIndex]; }
	float *GetVertexAOVs(const u_int dataIndex) const { return vertAOV[dataIndex]; }
	float *GetTriAOVs(const u_int dataIndex) const { return triAOV[dataIndex]; }

	const std::array<UV *, EXTMESH_MAX_DATA_COUNT> &GetAllUVs() const { return uvs; }
	const std::array<Spectrum *, EXTMESH_MAX_DATA_COUNT> &GetAllColors() const { return cols; }
	const std::array<float *, EXTMESH_MAX_DATA_COUNT> &GetAllAlphas() const { return alphas; }

	Normal *ComputeNormals();

	virtual MeshType GetType() const { return TYPE_EXT_TRIANGLE; }

	virtual bool HasNormals() const { return normals != nullptr; }
	virtual bool HasUVs(const u_int dataIndex) const { return uvs[dataIndex] != nullptr; }
	virtual bool HasColors(const u_int dataIndex) const { return cols[dataIndex] != nullptr; }
	virtual bool HasAlphas(const u_int dataIndex) const { return alphas[dataIndex] != nullptr; }

	virtual bool HasVertexAOV(const u_int dataIndex) const { return vertAOV[dataIndex] != nullptr; }
	virtual bool HasTriAOV(const u_int dataIndex) const { return triAOV[dataIndex] != nullptr; }

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

	virtual UV GetUV(const u_int vertIndex, const u_int dataIndex) const { return uvs[dataIndex][vertIndex]; }
	virtual Spectrum GetColor(const u_int vertIndex, const u_int dataIndex) const { return cols[dataIndex][vertIndex]; }
	virtual float GetAlpha(const u_int vertIndex, const u_int dataIndex) const { return alphas[dataIndex][vertIndex]; }
	
	virtual float GetVertexAOV(const u_int vertIndex, const u_int dataIndex) const {
		if (HasTriAOV(dataIndex))
			return vertAOV[dataIndex][vertIndex];
		else
			return 0.f;
	}
	virtual float GetTriAOV(const u_int triIndex, const u_int dataIndex) const {
		if (HasTriAOV(dataIndex))
			return triAOV[dataIndex][triIndex];
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
		if (!normals)
			return GetGeometryNormal(local2World, triIndex);
		const Triangle &tri = tris[triIndex];
		const float b0 = 1.f - b1 - b2;
		return (appliedTransSwapsHandedness ? -1.f : 1.f) * Normalize(b0 * normals[tri.v[0]] + b1 * normals[tri.v[1]] + b2 * normals[tri.v[2]]);
	}

	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		if (HasUVs(dataIndex)) {
			const Triangle &tri = tris[triIndex];
			const float b0 = 1.f - b1 - b2;
			return b0 * uvs[dataIndex][tri.v[0]] + b1 * uvs[dataIndex][tri.v[1]] + b2 * uvs[dataIndex][tri.v[2]];
		} else
			return UV(0.f, 0.f);
	}

	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		if (HasColors(dataIndex)) {
			const Triangle &tri = tris[triIndex];
			const float b0 = 1.f - b1 - b2;
			return b0 * cols[dataIndex][tri.v[0]] + b1 * cols[dataIndex][tri.v[1]] + b2 * cols[dataIndex][tri.v[2]];
		} else
			return Spectrum(1.f);
	}

	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		if (HasAlphas(dataIndex)) {
			const Triangle &tri = tris[triIndex];
			const float b0 = 1.f - b1 - b2;
			return b0 * alphas[dataIndex][tri.v[0]] + b1 * alphas[dataIndex][tri.v[1]] + b2 * alphas[dataIndex][tri.v[2]];
		} else
			return 1.f;
	}
	
	virtual float InterpolateTriVertexAOV(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		if (HasVertexAOV(dataIndex)) {
			const Triangle &tri = tris[triIndex];
			const float b0 = 1.f - b1 - b2;
			return b0 * vertAOV[dataIndex][tri.v[0]] + b1 * vertAOV[dataIndex][tri.v[1]] + b2 * vertAOV[dataIndex][tri.v[2]];
		} else
			return 0.f;
	}

	virtual void Save(const std::string &fileName) const;

	void CopyAOV(ExtMesh *destMesh) const;
	ExtTriangleMesh *CopyExt(Point *meshVertices, Triangle *meshTris, Normal *meshNormals,
			std::array<UV *, EXTMESH_MAX_DATA_COUNT> *meshUVs,
			std::array<Spectrum *, EXTMESH_MAX_DATA_COUNT> *meshCols,
			std::array<float *, EXTMESH_MAX_DATA_COUNT> *meshAlphas,
			const float bRadius = 0.f) const;
	ExtTriangleMesh *Copy(Point *meshVertices, Triangle *meshTris, Normal *meshNormals,
			UV *meshUVs, Spectrum *meshCols, float *meshAlphas,
			const float bRadius = 0.f) const;
	ExtTriangleMesh *Copy(const float bRadius = 0.f) const {
		return CopyExt(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, bRadius);
	}

	virtual bool IntersectBevel(const luxrays::Ray &ray, const luxrays::RayHit &rayHit,
			bool &continueToTrace, float &rayHitT,
			luxrays::Point &p, luxrays::Normal &n) const;
	
	static ExtTriangleMesh *Load(const std::string &fileName);
	static ExtTriangleMesh *Merge(const std::vector<const ExtTriangleMesh *> &meshes,
			const std::vector<luxrays::Transform> *trans = nullptr);

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
		const float r, const u_int index) {
			v0 = cv0;
			v1 = cv1;
			radius = r;
			bevelCylinderIndex = index;
		}

		luxrays::BBox GetBBox() const;
		bool IsInside(const luxrays::Point &p) const;
		
		luxrays::Point v0, v1;
		float radius;
		u_int bevelCylinderIndex;
	};

	static ExtTriangleMesh *LoadPly(const std::string &fileName);
	static ExtTriangleMesh *LoadSerialized(const std::string &fileName);

	// Used by serialization
	ExtTriangleMesh() {
	}

	void Init(Normal *meshNormals,
			std::array<UV *, EXTMESH_MAX_DATA_COUNT> *meshUVs,
			std::array<Spectrum *, EXTMESH_MAX_DATA_COUNT> *meshCols,
			std::array<float *, EXTMESH_MAX_DATA_COUNT> *meshAlphas);

	void Preprocess();
	void PreprocessBevel();
	
	virtual void SavePly(const std::string &fileName) const;
	virtual void SaveSerialized(const std::string &fileName) const;

	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(TriangleMesh);
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ExtMesh);

		const bool hasNormals = HasNormals();
		ar & hasNormals;
		if (HasNormals())
			for (u_int i = 0; i < vertCount; ++i)
				ar & normals[i];

		for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
			const bool hasUVs = HasUVs(i);
			ar & hasUVs;
			if (hasUVs)
				ar & boost::serialization::make_array<UV>(uvs[i], vertCount);

			const bool hasColors = HasColors(i);
			ar & hasColors;
			if (hasColors)
				ar & boost::serialization::make_array<Spectrum>(cols[i], vertCount);

			const bool hasAlphas = HasAlphas(i);
			ar & hasAlphas;
			if (hasAlphas)
				ar & boost::serialization::make_array<float>(alphas[i], vertCount);
			
			const bool hasVertexAOV = HasVertexAOV(i);
			ar & hasVertexAOV;
			if (hasVertexAOV)
				ar & boost::serialization::make_array<float>(vertAOV[i], vertCount);
			
			const bool hasTriangleAOV = HasTriAOV(i);
			ar & hasTriangleAOV;
			if (hasTriangleAOV)
				ar & boost::serialization::make_array<float>(triAOV[i], triCount);
		}
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(TriangleMesh);
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ExtMesh);

		bool hasNormals;
		ar & hasNormals;
		if (hasNormals) {
			normals = new Normal[vertCount];
			for (u_int i = 0; i < vertCount; ++i)
				ar & normals[i];
		} else
			normals = nullptr;
		triNormals = new Normal[triCount];

		for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
			bool hasUVs;
			ar & hasUVs;
			if (hasUVs) {
				uvs[i] = new UV[vertCount];
				ar & boost::serialization::make_array<UV>(uvs[i], vertCount);
			} else
				uvs[i] = nullptr;

			bool hasColors;
			ar & hasColors;
			if (hasColors) {
				cols[i] = new Spectrum[vertCount];
				ar & boost::serialization::make_array<Spectrum>(cols[i], vertCount);
			} else
				cols[i] = nullptr;

			bool hasAlphas;
			ar & hasAlphas;
			if (hasAlphas) {
				alphas[i] = new float[vertCount];
				ar & boost::serialization::make_array<float>(alphas[i], vertCount);
			} else
				alphas[i] = nullptr;

			bool hasVertexAOV;
			ar & hasVertexAOV;
			if (hasVertexAOV) {
				vertAOV[i] = new float[vertCount];
				ar & boost::serialization::make_array<float>(vertAOV[i], vertCount);
			} else
				vertAOV[i] = nullptr;

			bool hasTriangleAOV;
			ar & hasTriangleAOV;
			if (hasTriangleAOV) {
				triAOV[i] = new float[triCount];
				ar & boost::serialization::make_array<float>(triAOV[i], triCount);
			} else
				triAOV[i] = nullptr;
		}

		bevelCylinders = nullptr;
		bevelBoundingCylinders = nullptr;
		bevelBVHArrayNodes = nullptr;
		
		Preprocess();
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	Normal *normals; // Vertices normals
	Normal *triNormals; // Triangle normals

	std::array<UV *, EXTMESH_MAX_DATA_COUNT> uvs; // Vertex uvs
	std::array<Spectrum *, EXTMESH_MAX_DATA_COUNT> cols; // Vertex colors
	std::array<float *, EXTMESH_MAX_DATA_COUNT> alphas; // Vertex alphas

	std::array<float *, EXTMESH_MAX_DATA_COUNT> vertAOV; // Vertex AOV
	std::array<float *, EXTMESH_MAX_DATA_COUNT> triAOV; // Triangle AOV

	BevelCylinder *bevelCylinders;
	BevelBoundingCylinder *bevelBoundingCylinders;
	luxrays::ocl::IndexBVHArrayNode *bevelBVHArrayNodes;
};

class ExtInstanceTriangleMesh : public InstanceTriangleMesh, public ExtMesh {
public:
	ExtInstanceTriangleMesh(ExtTriangleMesh *m, const Transform &t) : 
		InstanceTriangleMesh(m, t) { }
	~ExtInstanceTriangleMesh() { };
	virtual void Delete() {	}

	virtual MeshType GetType() const { return TYPE_EXT_TRIANGLE_INSTANCE; }
	
	virtual float GetBevelRadius() const { return static_cast<ExtTriangleMesh *>(mesh)->GetBevelRadius(); }

	virtual bool HasNormals() const { return static_cast<ExtTriangleMesh *>(mesh)->HasNormals(); }
	virtual bool HasUVs(const u_int dataIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->HasUVs(dataIndex); }
	virtual bool HasColors(const u_int dataIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->HasColors(dataIndex); }
	virtual bool HasAlphas(const u_int dataIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->HasAlphas(dataIndex); }
	
	virtual bool HasVertexAOV(const u_int dataIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->HasVertexAOV(dataIndex); }
	virtual bool HasTriAOV(const u_int dataIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->HasTriAOV(dataIndex); }

	virtual Normal GetGeometryNormal(const luxrays::Transform &local2World, const u_int triIndex) const {
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<ExtTriangleMesh *>(mesh)->GetGeometryNormal(Transform::TRANS_IDENTITY, triIndex));
	}
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int triIndex, const u_int vertIndex) const {
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<ExtTriangleMesh *>(mesh)->GetShadeNormal(Transform::TRANS_IDENTITY, triIndex, vertIndex));
	}
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int vertIndex) const {
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<ExtTriangleMesh *>(mesh)->GetShadeNormal(Transform::TRANS_IDENTITY, vertIndex));
	}
	virtual UV GetUV(const unsigned vertIndex, const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->GetUV(vertIndex, dataIndex);
	}
	virtual Spectrum GetColor(const unsigned vertIndex, const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->GetColor(vertIndex, dataIndex);
	}
	virtual float GetAlpha(const unsigned vertIndex, const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->GetAlpha(vertIndex, dataIndex);
	}

	virtual float GetVertexAOV(const unsigned vertIndex, const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->GetVertexAOV(vertIndex, dataIndex);
	}
	virtual float GetTriAOV(const unsigned triIndex, const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->GetTriAOV(triIndex, dataIndex);
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
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(trans * static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriNormal(
				Transform::TRANS_IDENTITY, triIndex, b1, b2));
	}

	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriUV(triIndex,
				b1, b2, dataIndex);
	}

	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriColor(triIndex,
				b1, b2, dataIndex);
	}
	
	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriAlpha(triIndex,
				b1, b2, dataIndex);
	}
	
	virtual float InterpolateTriVertexAOV(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriVertexAOV(triIndex,
				b1, b2, dataIndex);
	}

	virtual void Save(const std::string &fileName) const { static_cast<ExtTriangleMesh *>(mesh)->Save(fileName); }

	const Transform &GetTransformation() const { return trans; }
	ExtTriangleMesh *GetExtTriangleMesh() const { return (ExtTriangleMesh *)mesh; };
	
	void UpdateMeshReferences(ExtTriangleMesh *oldMesh, ExtTriangleMesh *newMesh);

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
	ExtMotionTriangleMesh(ExtTriangleMesh *m, const MotionSystem &ms) :
		MotionTriangleMesh(m, ms) { }
	~ExtMotionTriangleMesh() { }
	virtual void Delete() {	}

	virtual MeshType GetType() const { return TYPE_EXT_TRIANGLE_MOTION; }

	virtual float GetBevelRadius() const { return static_cast<ExtTriangleMesh *>(mesh)->GetBevelRadius(); }

	virtual bool HasNormals() const { return static_cast<ExtTriangleMesh *>(mesh)->HasNormals(); }
	virtual bool HasUVs(const u_int dataIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->HasUVs(dataIndex); }
	virtual bool HasColors(const u_int dataIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->HasColors(dataIndex); }
	virtual bool HasAlphas(const u_int dataIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->HasAlphas(dataIndex); }

	virtual bool HasVertexAOV(const u_int dataIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->HasVertexAOV(dataIndex); }
	virtual bool HasTriAOV(const u_int dataIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->HasTriAOV(dataIndex); }

	virtual Normal GetGeometryNormal(const luxrays::Transform &local2World, const u_int triIndex) const {
		const bool transSwapsHandedness = local2World.SwapsHandedness();
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<ExtTriangleMesh *>(mesh)->GetGeometryNormal(local2World, triIndex));
	}
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int triIndex, const u_int vertIndex) const {
		const bool transSwapsHandedness = local2World.SwapsHandedness();
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<ExtTriangleMesh *>(mesh)->GetShadeNormal(local2World, triIndex, vertIndex));
	}
	virtual Normal GetShadeNormal(const luxrays::Transform &local2World, const u_int vertIndex) const {
		const bool transSwapsHandedness = local2World.SwapsHandedness();
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<ExtTriangleMesh *>(mesh)->GetShadeNormal(local2World, vertIndex));
	}
	virtual UV GetUV(const unsigned vertIndex, const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->GetUV(vertIndex, dataIndex);
	}
	virtual Spectrum GetColor(const unsigned vertIndex, const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->GetColor(vertIndex, dataIndex);
	}
	virtual float GetAlpha(const unsigned vertIndex, const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->GetAlpha(vertIndex, dataIndex);
	}

	virtual float GetVertexAOV(const unsigned vertIndex, const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->GetVertexAOV(vertIndex, dataIndex);
	}
	virtual float GetTriAOV(const unsigned triIndex, const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->GetTriAOV(triIndex, dataIndex);
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
		return (transSwapsHandedness ? -1.f : 1.f) * Normalize(local2World * static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriNormal(
				local2World, triIndex, b1, b2));
	}

	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriUV(triIndex,
				b1, b2, dataIndex);
	}
	
	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriColor(triIndex,
				b1, b2, dataIndex);
	}
	
	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriAlpha(triIndex,
				b1, b2, dataIndex);
	}

	virtual float InterpolateTriVertexAOV(const u_int triIndex, const float b1, const float b2,
			const u_int dataIndex) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriVertexAOV(triIndex,
				b1, b2, dataIndex);
	}

	virtual void Save(const std::string &fileName) const { static_cast<ExtTriangleMesh *>(mesh)->Save(fileName); }

	const MotionSystem &GetMotionSystem() const { return motionSystem; }
	ExtTriangleMesh *GetExtTriangleMesh() const { return (ExtTriangleMesh *)mesh; };

	void UpdateMeshReferences(ExtTriangleMesh *oldMesh, ExtTriangleMesh *newMesh);

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
