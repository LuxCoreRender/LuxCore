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

#ifndef _LUXRAYS_EXTTRIANGLEMESH_H
#define	_LUXRAYS_EXTTRIANGLEMESH_H

#include <cassert>
#include <cstdlib>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include "luxrays/luxrays.h"
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

/*
 * The inheritance scheme used here:
 * 
 *         | => TriangleMesh => |
 * Mesh => |                    |=> ExtTriangleMesh
 *         | =>      ExtMesh => |
 * 
 *         | => InstanceTriangleMesh => |
 * Mesh => |                            |=> ExtInstanceTriangleMesh
 *         | =>              ExtMesh => |
 * 
 *         | => MotionTriangleMesh => |
 * Mesh => |                          |=> ExtMotionTriangleMesh
 *         | =>            ExtMesh => |
 */
	
class ExtMesh : virtual public Mesh, public NamedObject {
public:
	ExtMesh() { }
	virtual ~ExtMesh() { }

	virtual bool HasNormals() const = 0;
	virtual bool HasUVs() const = 0;
	virtual bool HasColors() const = 0;
	virtual bool HasAlphas() const = 0;

	virtual Normal GetGeometryNormal(const float time, const u_int triIndex) const = 0;
	virtual Normal GetShadeNormal(const float time, const u_int triIndex, const u_int vertIndex) const = 0;
	virtual Normal GetShadeNormal(const float time, const u_int vertIndex) const = 0;
	virtual UV GetUV(const u_int vertIndex) const = 0;
	virtual Spectrum GetColor(const u_int vertIndex) const = 0;
	virtual float GetAlpha(const u_int vertIndex) const = 0;

	virtual bool GetTriBaryCoords(const float time, const u_int triIndex, const Point &hitPoint, float *b1, float *b2) const = 0;
    void GetDifferentials(const float time, const u_int triIndex, const Normal &shadeNormal,
        Vector *dpdu, Vector *dpdv,
        Normal *dndu, Normal *dndv) const;
	// Note: GetLocal2World can return NULL
	virtual void GetLocal2World(const float time, luxrays::Transform &t) const = 0;

	virtual Normal InterpolateTriNormal(const float time, const u_int triIndex, const float b1, const float b2) const = 0;
	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2) const = 0;
	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2) const = 0;
	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2) const = 0;

	// This can be a very expansive function to run
	virtual float GetMeshArea(const float time) const = 0;
	virtual float GetTriangleArea(const float time, const unsigned int triIndex) const = 0;
	virtual void Sample(const float time, const u_int triIndex, const float u0, const float u1,
		Point *p, float *b0, float *b1, float *b2) const = 0;

	virtual void Delete() = 0;
	virtual void Save(const std::string &fileName) const = 0;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Mesh);
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(NamedObject);
	}
};

class ExtTriangleMesh : public TriangleMesh, public ExtMesh {
public:
	ExtTriangleMesh(const u_int meshVertCount, const u_int meshTriCount,
			Point *meshVertices, Triangle *meshTris, Normal *meshNormals = NULL, UV *meshUV = NULL,
			Spectrum *meshCols = NULL, float *meshAlpha = NULL);
	~ExtTriangleMesh() { };
	virtual void Delete() {
		delete[] vertices;
		delete[] tris;
		delete[] normals;
		delete[] triNormals;
		delete[] uvs;
		delete[] cols;
		delete[] alphas;
	}

	Normal *ComputeNormals();

	virtual MeshType GetType() const { return TYPE_EXT_TRIANGLE; }

	virtual bool HasNormals() const { return normals != NULL; }
	virtual bool HasUVs() const { return uvs != NULL; }
	virtual bool HasColors() const { return cols != NULL; }
	virtual bool HasAlphas() const { return alphas != NULL; }

	virtual Normal GetGeometryNormal(const float time, const u_int triIndex) const {
		return triNormals[triIndex];
	}
	virtual Normal GetShadeNormal(const float time, const u_int triIndex, const u_int vertIndex) const { return normals[tris[triIndex].v[vertIndex]]; }
	virtual Normal GetShadeNormal(const float time, const u_int vertIndex) const { return normals[vertIndex]; }
	virtual UV GetUV(const u_int vertIndex) const { return uvs[vertIndex]; }
	virtual Spectrum GetColor(const u_int vertIndex) const { return cols[vertIndex]; }
	virtual float GetAlpha(const u_int vertIndex) const { return alphas[vertIndex]; }

	virtual bool GetTriBaryCoords(const float time, const u_int triIndex, const Point &hitPoint, float *b1, float *b2) const {
		const Triangle &tri = tris[triIndex];
		return tri.GetBaryCoords(vertices, hitPoint, b1, b2);
	}

	virtual void GetLocal2World(const float time, luxrays::Transform &t) const { }

	virtual void ApplyTransform(const Transform &trans);

	virtual Normal InterpolateTriNormal(const float time, const u_int triIndex, const float b1, const float b2) const {
		if (!normals)
			return GetGeometryNormal(time, triIndex);
		const Triangle &tri = tris[triIndex];
		const float b0 = 1.f - b1 - b2;
		return Normalize(b0 * normals[tri.v[0]] + b1 * normals[tri.v[1]] + b2 * normals[tri.v[2]]);
	}

	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2) const {
		if (uvs) {
			const Triangle &tri = tris[triIndex];
			const float b0 = 1.f - b1 - b2;
			return b0 * uvs[tri.v[0]] + b1 * uvs[tri.v[1]] + b2 * uvs[tri.v[2]];
		} else
			return UV(0.f, 0.f);
	}

	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2) const {
		if (cols) {
			const Triangle &tri = tris[triIndex];
			const float b0 = 1.f - b1 - b2;
			return b0 * cols[tri.v[0]] + b1 * cols[tri.v[1]] + b2 * cols[tri.v[2]];
		} else
			return Spectrum(1.f);
	}

	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2) const {
		if (alphas) {
			const Triangle &tri = tris[triIndex];
			const float b0 = 1.f - b1 - b2;
			return b0 * alphas[tri.v[0]] + b1 * alphas[tri.v[1]] + b2 * alphas[tri.v[2]];
		} else
			return 1.f;
	}

	virtual float GetMeshArea(const float time) const {
		return area;
	}
	
	virtual float GetTriangleArea(const float time, const unsigned int triIndex) const {
		return tris[triIndex].Area(vertices);
	}
	virtual void Sample(const float time, const u_int triIndex, const float u0, const float u1,
			Point *p, float *b0, float *b1, float *b2) const  {
		const Triangle &tri = tris[triIndex];
		tri.Sample(vertices, u0, u1, p, b0, b1, b2);
	}

	virtual void Save(const std::string &fileName) const;

	ExtTriangleMesh *Copy(Point *meshVertices, Triangle *meshTris, Normal *meshNormals, UV *meshUV,
			Spectrum *meshCols, float *meshAlpha) const;
	ExtTriangleMesh *Copy() const {
		return Copy(NULL, NULL, NULL, NULL, NULL, NULL);
	}

	static ExtTriangleMesh *Load(const std::string &fileName);

	friend class boost::serialization::access;

private:
	static ExtTriangleMesh *LoadPly(const std::string &fileName);
	static ExtTriangleMesh *LoadSerialized(const std::string &fileName);

	// Used by serialization
	ExtTriangleMesh() {
	}

	void Preprocess();

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

		const bool hasUVs = HasUVs();
		ar & hasUVs;
		if (HasUVs())
			for (u_int i = 0; i < vertCount; ++i)
				ar & uvs[i];

		const bool hasColors = HasColors();
		ar & hasColors;
		if (HasColors())
			for (u_int i = 0; i < vertCount; ++i)
				ar & cols[i];

		const bool hasAlphas = HasAlphas();
		ar & hasAlphas;
		if (HasAlphas())
			for (u_int i = 0; i < vertCount; ++i)
				ar & alphas[i];
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
			normals = NULL;

		bool hasUVs;
		ar & hasUVs;
		if (hasUVs) {
			uvs = new UV[vertCount];
			for (u_int i = 0; i < vertCount; ++i)
				ar & uvs[i];
		} else
			uvs = NULL;

		bool hasColors;
		ar & hasColors;
		if (hasColors) {
			cols = new Spectrum[vertCount];
			for (u_int i = 0; i < vertCount; ++i)
				ar & cols[i];
		} else
			cols = NULL;

		bool hasAlphas;
		ar & hasAlphas;
		if (hasAlphas) {
			alphas = new float[vertCount];
			for (u_int i = 0; i < vertCount; ++i)
				ar & alphas[i];
		} else
			alphas = NULL;

		triNormals = new Normal[triCount];
		Preprocess();
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	Normal *normals; // Vertices normals
	Normal *triNormals; // Triangle normals
	UV *uvs; // Vertex uvs
	Spectrum *cols; // Vertex color
	float *alphas; // Vertex alpha
	float area;
};

class ExtInstanceTriangleMesh : public InstanceTriangleMesh, public ExtMesh {
public:
	ExtInstanceTriangleMesh(ExtTriangleMesh *m, const Transform &t) :  InstanceTriangleMesh(m, t) {
		// The mesh area is compute on demand and cached
		cachedArea = -1.f;
	}
	~ExtInstanceTriangleMesh() { };
	virtual void Delete() {	}

	virtual MeshType GetType() const { return TYPE_EXT_TRIANGLE_INSTANCE; }

	virtual bool HasNormals() const { return static_cast<ExtTriangleMesh *>(mesh)->HasNormals(); }
	virtual bool HasUVs() const { return static_cast<ExtTriangleMesh *>(mesh)->HasUVs(); }
	virtual bool HasColors() const { return static_cast<ExtTriangleMesh *>(mesh)->HasColors(); }
	virtual bool HasAlphas() const { return static_cast<ExtTriangleMesh *>(mesh)->HasAlphas(); }

	virtual Normal GetGeometryNormal(const float time, const u_int triIndex) const {
		return Normalize(trans * static_cast<ExtTriangleMesh *>(mesh)->GetGeometryNormal(time, triIndex));
	}
	virtual Normal GetShadeNormal(const float time, const unsigned vertIndex) const {
		return Normalize(trans * static_cast<ExtTriangleMesh *>(mesh)->GetShadeNormal(time, vertIndex));
	}
	virtual Normal GetShadeNormal(const float time, const u_int triIndex, const u_int vertIndex) const {
		return Normalize(trans * static_cast<ExtTriangleMesh *>(mesh)->GetShadeNormal(time, triIndex, vertIndex));
	}
	virtual UV GetUV(const unsigned vertIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->GetUV(vertIndex); }
	virtual Spectrum GetColor(const unsigned vertIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->GetColor(vertIndex); }
	virtual float GetAlpha(const unsigned vertIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->GetAlpha(vertIndex); }

	virtual bool GetTriBaryCoords(const float time, const u_int triIndex, const Point &hitPoint, float *b1, float *b2) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::GetBaryCoords(GetVertex(time, tri.v[0]),
				GetVertex(time, tri.v[1]), GetVertex(time, tri.v[2]), hitPoint, b1, b2);
	}
	
	virtual void GetLocal2World(const float time, luxrays::Transform &t) const {
		t = trans;
	}

	virtual Normal InterpolateTriNormal(const float time, const u_int triIndex, const float b1, const float b2) const {
		return Normalize(trans * static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriNormal(time, triIndex, b1, b2));
	}

	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriUV(triIndex, b1, b2);
	}

	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriColor(triIndex, b1, b2);
	}
	
	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriAlpha(triIndex, b1, b2);
	}

	virtual float GetMeshArea(const float time) const {
		if (cachedArea < 0.f) {
			float area = 0.f;
			for (u_int i = 0; i < GetTotalTriangleCount(); ++i)
				area += GetTriangleArea(0.f, i);

			// Cache the result
			cachedArea = area;
		}

		return cachedArea;
	}

	virtual float GetTriangleArea(const float time, const u_int triIndex) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::Area(GetVertex(time, tri.v[0]), GetVertex(time, tri.v[1]), GetVertex(time, tri.v[2]));
	}
	virtual void Sample(const float time, const u_int triIndex, const float u0, const float u1, Point *p, float *b0, float *b1, float *b2) const  {
		static_cast<ExtTriangleMesh *>(mesh)->Sample(time, triIndex, u0, u1, p , b0, b1, b2);
		*p *= trans;
	}

	virtual void Save(const std::string &fileName) const { static_cast<ExtTriangleMesh *>(mesh)->Save(fileName); }

	virtual void ApplyTransform(const Transform &t) {
		InstanceTriangleMesh::ApplyTransform(t);

		// Invalidate the cached result
		cachedArea = -1.f;
	}

	const Transform &GetTransformation() const { return trans; }
	void SetTransformation(const Transform &t) {
		InstanceTriangleMesh::SetTransformation(t);

		// Invalidate the cached result
		cachedArea = -1.f;
	}
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
		cachedArea = -1.f;
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	mutable float cachedArea;
};

class ExtMotionTriangleMesh : public MotionTriangleMesh, public ExtMesh {
public:
	ExtMotionTriangleMesh(ExtTriangleMesh *m, const MotionSystem &ms) :
		MotionTriangleMesh(m, ms) {
		// The mesh area is compute on demand and cached
		cachedArea = -1.f;
	}
	~ExtMotionTriangleMesh() { }
	virtual void Delete() {	}

	virtual MeshType GetType() const { return TYPE_EXT_TRIANGLE_MOTION; }

	virtual bool HasNormals() const { return static_cast<ExtTriangleMesh *>(mesh)->HasNormals(); }
	virtual bool HasUVs() const { return static_cast<ExtTriangleMesh *>(mesh)->HasUVs(); }
	virtual bool HasColors() const { return static_cast<ExtTriangleMesh *>(mesh)->HasColors(); }
	virtual bool HasAlphas() const { return static_cast<ExtTriangleMesh *>(mesh)->HasAlphas(); }

	virtual Normal GetGeometryNormal(const float time, const u_int triIndex) const {
		const Matrix4x4 m = motionSystem.Sample(time);
		const Transform t = Inverse(Transform(m));
		return Normalize(t * static_cast<ExtTriangleMesh *>(mesh)->GetGeometryNormal(time, triIndex));
	}
	virtual Normal GetShadeNormal(const float time, const unsigned vertIndex) const {
		const Matrix4x4 m = motionSystem.Sample(time);
		const Transform t = Inverse(Transform(m));
		return Normalize(t * static_cast<ExtTriangleMesh *>(mesh)->GetShadeNormal(time, vertIndex));
	}
	virtual Normal GetShadeNormal(const float time, const u_int triIndex, const u_int vertIndex) const {
		const Matrix4x4 m = motionSystem.Sample(time);
		const Transform t = Inverse(Transform(m));
		return Normalize(t * static_cast<ExtTriangleMesh *>(mesh)->GetShadeNormal(time, triIndex, vertIndex));
	}
	virtual UV GetUV(const unsigned vertIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->GetUV(vertIndex); }
	virtual Spectrum GetColor(const unsigned vertIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->GetColor(vertIndex); }
	virtual float GetAlpha(const unsigned vertIndex) const { return static_cast<ExtTriangleMesh *>(mesh)->GetAlpha(vertIndex); }

	virtual bool GetTriBaryCoords(const float time, const u_int triIndex, const Point &hitPoint, float *b1, float *b2) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::GetBaryCoords(GetVertex(time, tri.v[0]),
				GetVertex(time, tri.v[1]), GetVertex(time, tri.v[2]), hitPoint, b1, b2);
	}

	virtual void GetLocal2World(const float time, luxrays::Transform &t) const {
		const Matrix4x4 m = motionSystem.Sample(time);
		t = Inverse(Transform(m));
	}

	virtual Normal InterpolateTriNormal(const float time, const u_int triIndex, const float b1, const float b2) const {
		const Matrix4x4 m = motionSystem.Sample(time);
		const Transform t = Inverse(Transform(m));
		return Normalize(t * static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriNormal(time, triIndex, b1, b2));
	}

	virtual UV InterpolateTriUV(const u_int triIndex, const float b1, const float b2) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriUV(triIndex, b1, b2);
	}
	
	virtual Spectrum InterpolateTriColor(const u_int triIndex, const float b1, const float b2) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriColor(triIndex, b1, b2);
	}
	
	virtual float InterpolateTriAlpha(const u_int triIndex, const float b1, const float b2) const {
		return static_cast<ExtTriangleMesh *>(mesh)->InterpolateTriAlpha(triIndex, b1, b2);
	}

	virtual float GetMeshArea(const float time) const {
		if (cachedArea < 0.f) {
			float area = 0.f;
			for (u_int i = 0; i < GetTotalTriangleCount(); ++i)
				area += GetTriangleArea(0.f, i);

			// Cache the result
			cachedArea = area;
		}

		return cachedArea;
	}

	virtual float GetTriangleArea(const float time, const u_int triIndex) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::Area(GetVertex(time, tri.v[0]), GetVertex(time, tri.v[1]), GetVertex(time, tri.v[2]));
	}
	virtual void Sample(const float time, const u_int triIndex, const float u0, const float u1, Point *p, float *b0, float *b1, float *b2) const  {
		static_cast<ExtTriangleMesh *>(mesh)->Sample(time, triIndex, u0, u1, p , b0, b1, b2);
		*p *= motionSystem.Sample(time);
	}

	virtual void Save(const std::string &fileName) const { static_cast<ExtTriangleMesh *>(mesh)->Save(fileName); }

	virtual void ApplyTransform(const Transform &t) {
		MotionTriangleMesh::ApplyTransform(t);

		// Invalidate the cached result
		cachedArea = -1.f;
	}

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
		cachedArea = -1.f;
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	mutable float cachedArea;
};

}

BOOST_SERIALIZATION_ASSUME_ABSTRACT(luxrays::ExtMesh)

BOOST_CLASS_VERSION(luxrays::ExtTriangleMesh, 1)
BOOST_CLASS_VERSION(luxrays::ExtInstanceTriangleMesh, 1)
BOOST_CLASS_VERSION(luxrays::ExtMotionTriangleMesh, 1)

BOOST_CLASS_EXPORT_KEY(luxrays::ExtTriangleMesh)
BOOST_CLASS_EXPORT_KEY(luxrays::ExtInstanceTriangleMesh)
BOOST_CLASS_EXPORT_KEY(luxrays::ExtMotionTriangleMesh)

#endif	/* _LUXRAYS_EXTTRIANGLEMESH_H */
