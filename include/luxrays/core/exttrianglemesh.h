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
#include "luxrays/core/trianglemesh.h"
#include "luxrays/utils/properties.h"

namespace luxrays {

class ExtMeshCache;

class ExtMesh : public Mesh {
public:
	ExtMesh() { }
	virtual ~ExtMesh() { }

	std::string GetName() const { return "extmesh-" + boost::lexical_cast<std::string>(this); }

	virtual bool HasNormals() const = 0;
	virtual bool HasUVs() const = 0;
	virtual bool HasColors() const = 0;
	virtual bool HasAlphas() const = 0;

	virtual Normal GetGeometryNormal(const unsigned int triIndex) const = 0;
	virtual Normal GetShadeNormal(const unsigned int triIndex, const unsigned int vertIndex) const = 0;
	virtual Normal GetShadeNormal(const unsigned int vertIndex) const = 0;
	virtual UV GetUV(const unsigned int vertIndex) const = 0;
	virtual Spectrum GetColor(const unsigned int vertIndex) const = 0;
	virtual float GetAlpha(const unsigned int vertIndex) const = 0;

	virtual bool GetTriBaryCoords(const unsigned int index, const Point &hitPoint, float *b1, float *b2) const = 0;

	virtual Normal InterpolateTriNormal(const unsigned int index, const float b1, const float b2) const = 0;
	virtual UV InterpolateTriUV(const unsigned int index, const float b1, const float b2) const = 0;
	virtual Spectrum InterpolateTriColor(const unsigned int index, const float b1, const float b2) const = 0;
	virtual float InterpolateTriAlpha(const unsigned int index, const float b1, const float b2) const = 0;

	virtual void Sample(const unsigned int index, const float u0, const float u1, Point *p, float *b0, float *b1, float *b2) const = 0;

	virtual void Delete() = 0;
	virtual void WritePly(const std::string &fileName) const = 0;
};

class ExtTriangleMesh : public ExtMesh {
public:
	ExtTriangleMesh(ExtTriangleMesh *mesh);
	ExtTriangleMesh(const unsigned int meshVertCount, const unsigned int meshTriCount,
			Point *meshVertices, Triangle *meshTris, Normal *meshNormals = NULL, UV *meshUV = NULL,
			Spectrum *meshCols = NULL, float *meshAlpha = NULL);
	~ExtTriangleMesh() { };
	void Delete() {
		delete[] vertices;
		delete[] tris;
		delete[] normals;
		delete[] triNormals;
		delete[] uvs;
		delete[] cols;
		delete[] alphas;
	}

	Normal *ComputeNormals();

	MeshType GetType() const { return TYPE_EXT_TRIANGLE; }
	unsigned int GetTotalVertexCount() const { return vertCount; }
	unsigned int GetTotalTriangleCount() const { return triCount; }
	BBox GetBBox() const;

	bool HasNormals() const { return normals != NULL; }
	bool HasUVs() const { return uvs != NULL; }
	bool HasColors() const { return cols != NULL; }
	bool HasAlphas() const { return alphas != NULL; }

	Point GetVertex(const unsigned int vertIndex) const { return vertices[vertIndex]; }
	float GetTriangleArea(const unsigned int triIndex) const { return tris[triIndex].Area(vertices); }
	Normal GetGeometryNormal(const unsigned int triIndex) const {
		return triNormals[triIndex];
	}
	Normal GetShadeNormal(const unsigned int triIndex, const unsigned int vertIndex) const { return normals[tris[triIndex].v[vertIndex]]; }
	Normal GetShadeNormal(const unsigned int vertIndex) const { return normals[vertIndex]; }
	UV GetUV(const unsigned int vertIndex) const { return uvs[vertIndex]; }
	Spectrum GetColor(const unsigned int vertIndex) const { return cols[vertIndex]; }
	float GetAlpha(const unsigned int vertIndex) const { return alphas[vertIndex]; }

	bool GetTriBaryCoords(const unsigned int index, const Point &hitPoint, float *b1, float *b2) const {
		const Triangle &tri = tris[index];
		return tri.GetBaryCoords(vertices, hitPoint, b1, b2);
	}
	
	Normal InterpolateTriNormal(const unsigned int index, const float b1, const float b2) const {
		if (!normals)
			return GetGeometryNormal(index);
		const Triangle &tri = tris[index];
		const float b0 = 1.f - b1 - b2;
		return Normalize(b0 * normals[tri.v[0]] + b1 * normals[tri.v[1]] + b2 * normals[tri.v[2]]);
	}

	UV InterpolateTriUV(const unsigned int index, const float b1, const float b2) const {
		if (uvs) {
			const Triangle &tri = tris[index];
			const float b0 = 1.f - b1 - b2;
			return b0 * uvs[tri.v[0]] + b1 * uvs[tri.v[1]] + b2 * uvs[tri.v[2]];
		} else
			return UV(0.f, 0.f);
	}

	Spectrum InterpolateTriColor(const unsigned int index, const float b1, const float b2) const {
		if (cols) {
			const Triangle &tri = tris[index];
			const float b0 = 1.f - b1 - b2;
			return b0 * cols[tri.v[0]] + b1 * cols[tri.v[1]] + b2 * cols[tri.v[2]];
		} else
			return Spectrum(1.f);
	}

	float InterpolateTriAlpha(const unsigned int index, const float b1, const float b2) const {
		if (alphas) {
			const Triangle &tri = tris[index];
			const float b0 = 1.f - b1 - b2;
			return b0 * alphas[tri.v[0]] + b1 * alphas[tri.v[1]] + b2 * alphas[tri.v[2]];
		} else
			return 1.f;
	}

	void Sample(const unsigned int index, const float u0, const float u1, Point *p, float *b0, float *b1, float *b2) const  {
		const Triangle &tri = tris[index];
		tri.Sample(vertices, u0, u1, p, b0, b1, b2);
	}

	Point *GetVertices() const { return vertices; }
	Triangle *GetTriangles() const { return tris; }

	virtual void ApplyTransform(const Transform &trans);

	virtual void WritePly(const std::string &fileName) const;

	static ExtTriangleMesh *LoadExtTriangleMesh(const std::string &fileName);
	static ExtTriangleMesh *CreateExtTriangleMesh(
		const long plyNbVerts, const long plyNbTris,
		Point *p, Triangle *vi, Normal *n, UV *uv, Spectrum *cols, float *alphas);

private:
	unsigned int vertCount;
	unsigned int triCount;
	Point *vertices;
	Triangle *tris;
	Normal *normals; // Vertices normals
	Normal *triNormals; // Triangle normals
	UV *uvs; // Vertex uvs
	Spectrum *cols; // Vertex color
	float *alphas; // Vertex alpha
};

class ExtInstanceTriangleMesh : public ExtMesh {
public:
	ExtInstanceTriangleMesh(ExtTriangleMesh *m, const Transform &t) {
		assert (m != NULL);

		trans = t;
		mesh = m;
	}
	~ExtInstanceTriangleMesh() { };
	void Delete() {	}

	virtual MeshType GetType() const { return TYPE_EXT_TRIANGLE_INSTANCE; }

	Point GetVertex(const unsigned index) const {
		return trans * mesh->GetVertex(index);
	}
	float GetTriangleArea(const unsigned int triIndex) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::Area(GetVertex(tri.v[0]), GetVertex(tri.v[1]), GetVertex(tri.v[2]));
	}
	unsigned int GetTotalVertexCount() const { return mesh->GetTotalVertexCount(); }
	unsigned int GetTotalTriangleCount() const { return mesh->GetTotalTriangleCount(); }

	BBox GetBBox() const {
		return trans * mesh->GetBBox();
	}

	bool HasNormals() const { return mesh->HasNormals(); }
	bool HasUVs() const { return mesh->HasUVs(); }
	bool HasColors() const { return mesh->HasColors(); }
	bool HasAlphas() const { return mesh->HasAlphas(); }

	Normal GetGeometryNormal(const unsigned int triIndex) const {
		return Normalize(trans * mesh->GetGeometryNormal(triIndex));
	}
	Normal GetShadeNormal(const unsigned index) const {
		return Normalize(trans * mesh->GetShadeNormal(index));
	}
	Normal GetShadeNormal(const unsigned int triIndex, const unsigned int vertIndex) const {
		return Normalize(trans * mesh->GetShadeNormal(triIndex, vertIndex));
	}
	UV GetUV(const unsigned index) const { return mesh->GetUV(index); }
	Spectrum GetColor(const unsigned index) const { return mesh->GetColor(index); }
	float GetAlpha(const unsigned index) const { return mesh->GetAlpha(index); }

	bool GetTriBaryCoords(const unsigned int index, const Point &hitPoint, float *b1, float *b2) const {
		const Triangle &tri = mesh->GetTriangles()[index];

		return Triangle::GetBaryCoords(GetVertex(tri.v[0]), GetVertex(tri.v[1]), GetVertex(tri.v[2]), hitPoint, b1, b2);
	}

	Normal InterpolateTriNormal(const unsigned int index, const float b1, const float b2) const {
		return Normalize(trans * mesh->InterpolateTriNormal(index, b1, b2));
	}

	UV InterpolateTriUV(const unsigned int index, const float b1, const float b2) const {
		return mesh->InterpolateTriUV(index, b1, b2);
	}
	
	Spectrum InterpolateTriColor(const unsigned int index, const float b1, const float b2) const {
		return mesh->InterpolateTriColor(index, b1, b2);
	}
	
	float InterpolateTriAlpha(const unsigned int index, const float b1, const float b2) const {
		return mesh->InterpolateTriAlpha(index, b1, b2);
	}

	void Sample(const unsigned int index, const float u0, const float u1, Point *p, float *b0, float *b1, float *b2) const  {
		mesh->Sample(index, u0, u1, p , b0, b1, b2);
		*p *= trans;
	}

	virtual void ApplyTransform(const Transform &t) { trans = trans * t; }

	const Transform &GetTransformation() const { return trans; }
	void SetTransformation(const Transform &t) {
		trans = t;
	}
	Point *GetVertices() const { return mesh->GetVertices(); }
	Triangle *GetTriangles() const { return mesh->GetTriangles(); }
	ExtTriangleMesh *GetExtTriangleMesh() const { return mesh; };

	virtual void WritePly(const std::string &fileName) const { mesh->WritePly(fileName); }

private:
	Transform trans;
	ExtTriangleMesh *mesh;
};

}

#endif	/* _LUXRAYS_EXTTRIANGLEMESH_H */
