/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _LUXRAYS_EXTTRIANGLEMESH_H
#define	_LUXRAYS_EXTTRIANGLEMESH_H

#include <cassert>
#include <cstdlib>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/triangle.h"
#include "luxrays/core/trianglemesh.h"

namespace luxrays {

class ExtMesh : public Mesh {
public:
	ExtMesh() { }
	virtual ~ExtMesh() { }

	virtual bool HasNormals() const = 0;
	virtual bool HasColors() const = 0;
	virtual bool HasUVs() const = 0;

	virtual Normal GetNormal(const unsigned int triIndex, const unsigned int vertIndex) const = 0;
	virtual Normal GetNormal(const unsigned int vertIndex) const = 0;
	virtual Spectrum GetColor(const unsigned int vertIndex) const = 0;
	virtual UV GetUV(const unsigned int vertIndex) const = 0;

	virtual Normal InterpolateTriNormal(const unsigned int index, const float b1, const float b2) const = 0;
	virtual Spectrum InterpolateTriColor(const unsigned int index, const float b0, const float b1, const float b2) const = 0;
	virtual Spectrum InterpolateTriColor(const unsigned int index, const float b1, const float b2) const = 0;
	virtual UV InterpolateTriUV(const unsigned int index, const float b1, const float b2) const = 0;

	virtual void Sample(const unsigned int index, const float u0, const float u1, Point *p, float *b0, float *b1, float *b2) const = 0;

	virtual void Delete() = 0;
};

class ExtTriangleMesh : public ExtMesh {
public:
	ExtTriangleMesh(ExtTriangleMesh *mesh) {
		assert (mesh != NULL);

		vertCount = mesh->vertCount;
		triCount = mesh->triCount;
		vertices = mesh->vertices;
		tris = mesh->tris;

		normals = mesh->normals;
		colors = mesh->colors;
		uvs = mesh->uvs;
	}
	ExtTriangleMesh(const unsigned int meshVertCount, const unsigned int meshTriCount,
			Point *meshVertices, Triangle *meshTris, Normal *meshNormals = NULL, Spectrum *meshColors = NULL, UV *meshUV = NULL) {
		assert (meshVertCount > 0);
		assert (meshTriCount > 0);
		assert (meshVertices != NULL);
		assert (meshTris != NULL);

		vertCount = meshVertCount;
		triCount = meshTriCount;
		vertices = meshVertices;
		tris = meshTris;

		normals = meshNormals;
		colors = meshColors;
		uvs = meshUV;
	}
	~ExtTriangleMesh() { };
	void Delete() {
		delete[] vertices;
		delete[] tris;
		delete[] normals;
		delete[] colors;
		delete[] uvs;
	}

	MeshType GetType() const { return TYPE_EXT_TRIANGLE; }
	unsigned int GetTotalVertexCount() const { return vertCount; }
	unsigned int GetTotalTriangleCount() const { return triCount; }
	BBox GetBBox() const;

	bool HasNormals() const { return normals != NULL; }
	bool HasColors() const { return colors != NULL; }
	bool HasUVs() const { return uvs != NULL; }

	Point GetVertex(const unsigned int vertIndex) const { return vertices[vertIndex]; }
	float GetTriangleArea(const unsigned int triIndex) const { return tris[triIndex].Area(vertices); }
	Normal GetNormal(const unsigned int triIndex, const unsigned int vertIndex) const { return normals[tris[triIndex].v[vertIndex]]; }
	Normal GetNormal(const unsigned int vertIndex) const { return normals[vertIndex]; }
	Spectrum GetColor(const unsigned int vertIndex) const { return colors[vertIndex]; }
	UV GetUV(const unsigned int vertIndex) const { return uvs[vertIndex]; }

	Normal InterpolateTriNormal(const unsigned int index, const float b1, const float b2) const {
		const Triangle &tri = tris[index];
		const float b0 = 1.f - b1 - b2;
		return Normalize(b0 * normals[tri.v[0]] + b1 * normals[tri.v[1]] + b2 * normals[tri.v[2]]);
	}

	Spectrum InterpolateTriColor(const unsigned int index, const float b0, const float b1, const float b2) const {
		const Triangle &tri = tris[index];
		return b0 * colors[tri.v[0]] + b1 * colors[tri.v[1]] + b2 * colors[tri.v[2]];
	}

	Spectrum InterpolateTriColor(const unsigned int index, const float b1, const float b2) const {
		const Triangle &tri = tris[index];
		const float b0 = 1.f - b1 - b2;
		return b0 * colors[tri.v[0]] + b1 * colors[tri.v[1]] + b2 * colors[tri.v[2]];
	}

	UV InterpolateTriUV(const unsigned int index, const float b1, const float b2) const {
		const Triangle &tri = tris[index];
		const float b0 = 1.f - b1 - b2;
		return b0 * uvs[tri.v[0]] + b1 * uvs[tri.v[1]] + b2 * uvs[tri.v[2]];
	}

	void Sample(const unsigned int index, const float u0, const float u1, Point *p, float *b0, float *b1, float *b2) const  {
		const Triangle &tri = tris[index];
		tri.Sample(vertices, u0, u1, p, b0, b1, b2);
	}

	Point *GetVertices() const { return vertices; }
	Triangle *GetTriangles() const { return tris; }

	virtual void ApplyTransform(const Transform &trans);

	static ExtTriangleMesh *LoadExtTriangleMesh(const std::string &fileName, const bool usePlyNormals = false);

private:
	unsigned int vertCount;
	unsigned int triCount;
	Point *vertices;
	Triangle *tris;
	Normal *normals;
	Spectrum *colors;
	UV *uvs;
};

class ExtInstanceTriangleMesh : public ExtMesh {
public:
	ExtInstanceTriangleMesh(ExtTriangleMesh *m, const Transform &t) {
		assert (m != NULL);

		trans = t;
		invTrans = t.GetInverse();
		mesh = m;
	}
	~ExtInstanceTriangleMesh() { };
	void Delete() {	}

	virtual MeshType GetType() const { return TYPE_EXT_TRIANGLE_INSTANCE; }

	Point GetVertex(const unsigned index) const { return trans(mesh->GetVertex(index)); }
	float GetTriangleArea(const unsigned int triIndex) const {
		const Triangle &tri = mesh->GetTriangles()[triIndex];

		return Triangle::Area(GetVertex(tri.v[0]), GetVertex(tri.v[1]), GetVertex(tri.v[2]));
	}
	unsigned int GetTotalVertexCount() const { return mesh->GetTotalVertexCount(); }
	unsigned int GetTotalTriangleCount() const { return mesh->GetTotalTriangleCount(); }

	BBox GetBBox() const {
		return trans(mesh->GetBBox());
	}

	bool HasNormals() const { return mesh->HasNormals(); }
	bool HasColors() const { return mesh->HasColors(); }
	bool HasUVs() const { return mesh->HasUVs(); }

	Normal GetNormal(const unsigned index) const { return Normalize(trans(mesh->GetNormal(index))); }
	Normal GetNormal(const unsigned int triIndex, const unsigned int vertIndex) const { return Normalize(trans(mesh->GetNormal(triIndex, vertIndex))); }
	Spectrum GetColor(const unsigned index) const { return mesh->GetColor(index); }
	UV GetUV(const unsigned index) const { return mesh->GetUV(index); }

	Normal InterpolateTriNormal(const unsigned int index, const float b1, const float b2) const {
		return Normalize(trans(mesh->InterpolateTriNormal(index, b1, b2)));
	}

	Spectrum InterpolateTriColor(const unsigned int index, const float b0, const float b1, const float b2) const {
		return mesh->InterpolateTriColor(index, b0, b1, b2);
	}

	Spectrum InterpolateTriColor(const unsigned int index, const float b1, const float b2) const {
		return mesh->InterpolateTriColor(index, b1, b2);
	}

	UV InterpolateTriUV(const unsigned int index, const float b1, const float b2) const {
		return mesh->InterpolateTriUV(index, b1, b2);
	}

	void Sample(const unsigned int index, const float u0, const float u1, Point *p, float *b0, float *b1, float *b2) const  {
		mesh->Sample(index, u0, u1, p , b0, b1, b2);
		*p = trans(*p);
	}

	virtual void ApplyTransform(const Transform &t) { trans = trans * t; }

	const Transform &GetTransformation() const { return trans; }
	const Transform &GetInvTransformation() const { return invTrans; }
	void SetTransformation(const Transform &t) {
		trans = t;
		invTrans = t.GetInverse();
	}
	Point *GetVertices() const { return mesh->GetVertices(); }
	Triangle *GetTriangles() const { return mesh->GetTriangles(); }
	ExtTriangleMesh *GetExtTriangleMesh() const { return mesh; };

private:
	Transform trans, invTrans;
	ExtTriangleMesh *mesh;
};

}

#endif	/* _LUXRAYS_EXTTRIANGLEMESH_H */
