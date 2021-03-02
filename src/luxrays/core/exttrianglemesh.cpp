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

#include <iostream>
#include <fstream>
#include <cstring>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/utils/ply/rply.h"
#include "luxrays/utils/serializationutils.h"
#include "luxrays/utils/strutils.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// ExtMesh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::ExtMesh)

void ExtMesh::GetDifferentials(const Transform &local2World,
		const u_int triIndex, const Normal &shadeNormal, const u_int dataIndex,
        Vector *dpdu, Vector *dpdv,
        Normal *dndu, Normal *dndv) const {
    // Compute triangle partial derivatives
    const Triangle &tri = GetTriangles()[triIndex];
	const u_int v0Index = tri.v[0];
	const u_int v1Index = tri.v[1];
	const u_int v2Index = tri.v[2];

    UV uv0, uv1, uv2;
    if (HasUVs(dataIndex)) {
        uv0 = GetUV(v0Index, dataIndex);
        uv1 = GetUV(v1Index, dataIndex);
        uv2 = GetUV(v2Index, dataIndex);
    } else {
		uv0 = UV(.5f, .5f);
		uv1 = UV(.5f, .5f);
		uv2 = UV(.5f, .5f);
	}

    // Compute deltas for triangle partial derivatives
	const float du1 = uv0.u - uv2.u;
	const float du2 = uv1.u - uv2.u;
	const float dv1 = uv0.v - uv2.v;
	const float dv2 = uv1.v - uv2.v;
	const float determinant = du1 * dv2 - dv1 * du2;

	if (determinant == 0.f) {
		// Handle 0 determinant for triangle partial derivative matrix
		CoordinateSystem(Vector(shadeNormal), dpdu, dpdv);
		*dndu = Normal();
		*dndv = Normal();
	} else {
		const float invdet = 1.f / determinant;

		// Using localToWorld in order to do all computation relative to
		// the global coordinate system
		const Point p0 = GetVertex(local2World, v0Index);
		const Point p1 = GetVertex(local2World, v1Index);
		const Point p2 = GetVertex(local2World, v2Index);

		const Vector dp1 = p0 - p2;
		const Vector dp2 = p1 - p2;

		const Vector geometryDpDu = ( dv2 * dp1 - dv1 * dp2) * invdet;
		const Vector geometryDpDv = (-du2 * dp1 + du1 * dp2) * invdet;

		*dpdu = Cross(shadeNormal, Cross(geometryDpDu, shadeNormal));
		*dpdv = Cross(shadeNormal, Cross(geometryDpDv, shadeNormal));

		if (HasNormals()) {
			// Using localToWorld in order to do all computation relative to
			// the global coordinate system
			const Normal n0 = Normalize(GetShadeNormal(local2World, v0Index));
			const Normal n1 = Normalize(GetShadeNormal(local2World, v1Index));
			const Normal n2 = Normalize(GetShadeNormal(local2World, v2Index));

			const Normal dn1 = n0 - n2;
			const Normal dn2 = n1 - n2;
			*dndu = ( dv2 * dn1 - dv1 * dn2) * invdet;
			*dndv = (-du2 * dn1 + du1 * dn2) * invdet;
		} else {
			*dndu = Normal();
			*dndv = Normal();
		}
	}
}

//------------------------------------------------------------------------------
// ExtTriangleMesh
//------------------------------------------------------------------------------

// This is a workaround to a GCC bug described here:
//  https://svn.boost.org/trac10/ticket/3730
//  https://marc.info/?l=boost&m=126496738227673&w=2
namespace boost{
template<>
struct is_virtual_base_of<luxrays::TriangleMesh, luxrays::ExtTriangleMesh>: public mpl::true_ {};
}

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::ExtTriangleMesh)

ExtTriangleMesh::ExtTriangleMesh(const u_int meshVertCount, const u_int meshTriCount,
		Point *meshVertices, Triangle *meshTris, Normal *meshNormals,
		UV *mUVs, Spectrum *mCols, float *mAlphas, const float bRadius) :
		TriangleMesh(meshVertCount, meshTriCount, meshVertices, meshTris) {
	bevelCylinders = nullptr;
	triBevelCylinders = nullptr;
	bevelRadius = bRadius;

	fill(uvs.begin(), uvs.end(), nullptr);
	fill(cols.begin(), cols.end(), nullptr);
	fill(alphas.begin(), alphas.end(), nullptr);
	fill(vertAOV.begin(), vertAOV.end(), nullptr);
	fill(triAOV.begin(), triAOV.end(), nullptr);

	array<UV *, EXTMESH_MAX_DATA_COUNT> meshUVs;
	fill(meshUVs.begin(), meshUVs.end(), nullptr);
	if (mUVs)
		meshUVs[0] = mUVs;

	array<Spectrum *, EXTMESH_MAX_DATA_COUNT> meshCols;
	fill(meshCols.begin(), meshCols.end(), nullptr);
	if (mCols)
		meshCols[0] = mCols;

	array<float *, EXTMESH_MAX_DATA_COUNT> meshAlphas;
	fill(meshAlphas.begin(), meshAlphas.end(), nullptr);
	if (mAlphas)
		meshAlphas[0] = mAlphas;

	Init(meshNormals, &meshUVs, &meshCols, &meshAlphas);	
}

ExtTriangleMesh::ExtTriangleMesh(const u_int meshVertCount, const u_int meshTriCount,
		Point *meshVertices, Triangle *meshTris, Normal *meshNormals,
		array<UV *, EXTMESH_MAX_DATA_COUNT> *meshUVs,
		array<Spectrum *, EXTMESH_MAX_DATA_COUNT> *meshCols,
		array<float *, EXTMESH_MAX_DATA_COUNT> *meshAlphas,
		const float bRadius) :
		TriangleMesh(meshVertCount, meshTriCount, meshVertices, meshTris) {
	bevelCylinders = nullptr;
	triBevelCylinders = nullptr;
	bevelRadius = bRadius;

	fill(uvs.begin(), uvs.end(), nullptr);
	fill(cols.begin(), cols.end(), nullptr);
	fill(alphas.begin(), alphas.end(), nullptr);
	fill(vertAOV.begin(), vertAOV.end(), nullptr);
	fill(triAOV.begin(), triAOV.end(), nullptr);

	Init(meshNormals, meshUVs, meshCols, meshAlphas);
}

void ExtTriangleMesh::Init(Normal *meshNormals,
		array<UV *, EXTMESH_MAX_DATA_COUNT> *meshUVs,
		array<Spectrum *, EXTMESH_MAX_DATA_COUNT> *meshCols,
		array<float *, EXTMESH_MAX_DATA_COUNT> *meshAlphas) {
	if (meshUVs && (meshUVs->size() > EXTMESH_MAX_DATA_COUNT)) {
		throw runtime_error("Error in ExtTriangleMesh::ExtTriangleMesh(): trying to define more (" +
				ToString(meshUVs->size()) + ") UV sets than EXTMESH_MAX_DATA_COUNT");
	}
	if (meshCols && (meshCols->size() > EXTMESH_MAX_DATA_COUNT)) {
		throw runtime_error("Error in ExtTriangleMesh::ExtTriangleMesh(): trying to define more (" +
				ToString(meshCols->size()) + ") Color sets than EXTMESH_MAX_DATA_COUNT");
	}
	if (meshAlphas && (meshAlphas->size() > EXTMESH_MAX_DATA_COUNT)) {
		throw runtime_error("Error in ExtTriangleMesh::ExtTriangleMesh(): trying to define more (" +
				ToString(meshAlphas->size()) + ") Alpha sets than EXTMESH_MAX_DATA_COUNT");
	}

	normals = meshNormals;
	triNormals = new Normal[triCount];

	if (meshUVs)
		uvs = *meshUVs;
	if (meshCols)
		cols = *meshCols;
	if (meshAlphas)
		alphas = *meshAlphas;

	Preprocess();
}

void ExtTriangleMesh::Preprocess() {
	// Compute all triangle normals
	for (u_int i = 0; i < triCount; ++i)
		triNormals[i] = tris[i].GetGeometryNormal(vertices);
	
	PreprocessBevel();
}

void ExtTriangleMesh::PreprocessBevel() {
	const double start = WallClockTime();

	const float bevelRadius = GetBevelRadius();
	if (bevelRadius > 0.f) {
		//----------------------------------------------------------------------
		// Edge class
		//----------------------------------------------------------------------

		class Edge {
		public:
			Edge(const u_int triIndex, const u_int e, const u_int va, const u_int vb) : tri(triIndex),
					edge(e), v0(va), v1(vb), alreadyFound(false) { }
			~Edge() { }

			const u_int tri, edge;
			const u_int v0, v1;

			bool alreadyFound;
		};

		//----------------------------------------------------------------------
		// Build the edge information
		//----------------------------------------------------------------------

		vector<Edge> edges;
		for (u_int i = 0; i < triCount; ++i) {
			edges.push_back(Edge(i, 0, tris[i].v[0], tris[i].v[1]));
			edges.push_back(Edge(i, 1, tris[i].v[1], tris[i].v[2]));
			edges.push_back(Edge(i, 2, tris[i].v[2], tris[i].v[0]));
		}

		cout << "ExtTriangleMesh " << this << " edges count: " << edges.size() << endl;

		//----------------------------------------------------------------------
		// Initialize bevelCylinders and triBevelCylinders
		//----------------------------------------------------------------------

		vector<BevelCylinder> bevelCyls;
		vector<TriangleBevelCylinders> triBevelCyls(triCount);

		//----------------------------------------------------------------------
		// Look for bevel edges
		//----------------------------------------------------------------------

		auto IsSameVertex = [&](const u_int v0, const u_int v1) {
			return DistanceSquared(vertices[v0], vertices[v1]) < DEFAULT_EPSILON_STATIC;
		};

		auto IsSameEdge = [&](const u_int edge0Index, const u_int edge1Index) {
			const u_int e0v0 = edges[edge0Index].v0;
			const u_int e0v1 = edges[edge0Index].v1;
			const u_int e1v0 = edges[edge1Index].v0;
			const u_int e1v1 = edges[edge1Index].v1;

			return
				// Check if the vertices are near enough
				(IsSameVertex(e0v0, e1v0) && IsSameVertex(e0v1, e1v1)) ||
					(IsSameVertex(e0v0, e1v1) && IsSameVertex(e0v1, e1v0));
		};

		for (u_int edge0Index = 0; edge0Index < edges.size(); ++edge0Index) {
			Edge &e0 = edges[edge0Index];
			e0.alreadyFound = true;

			for (u_int edge1Index = edge0Index + 1; edge1Index < edges.size(); ++edge1Index) {
				Edge &e1 = edges[edge1Index];

				if (!e1.alreadyFound && IsSameEdge(edge0Index, edge1Index)) {
					// It is a candidate. Check if it a convex edge.
					
					// Pick the normal of the first triangles
					const Normal &tri0Normal = triNormals[e0.tri];

					// Pick the vertex, not part of the edge, of the first triangle
					const Point &tri0Vertex = vertices[(e0.edge + 2) % 3];

					// Pick the vertex, not part of the edge, of the second triangle
					const Point &tri1Vertex = vertices[(e1.edge + 2) % 3];
					
					// Compare the vector between the vertices not part of the shared edge and
					// the triangle 0 normal
					const float angle = Dot(tri0Normal, Normalize(tri1Vertex - tri0Vertex));
					
					if (angle <= DEFAULT_EPSILON_STATIC) {
						// Ok, it is a convex edge. It is an edge to bevel.
						e1.alreadyFound = true;
						
						const Normal &tri1Normal = triNormals[e1.tri];

						// /Normals half vector direction
						const Vector h(-Normalize(tri0Normal + tri1Normal));
						const float cosHAngle = AbsDot(h, tri0Normal);

						// Compute the bevel cylinder vertices

						const float sinAlpha = sinf(acosf(cosHAngle));
						const float distance = bevelRadius / sinAlpha;
						const Vector vertexOffset(distance * h);
						Vector cv0(vertices[e0.v0] + vertexOffset);
						Vector cv1(vertices[e0.v1] + vertexOffset);
						
						// Add a new BevelCylinder
						const u_int bevelCylinderIndex = bevelCyls.size();
						bevelCyls.push_back(BevelCylinder(cv0, cv1));

						// Add the BevelCylinder to the 2 triangles sharing the edge
						if (!triBevelCyls[e0.tri].IsFull())
							triBevelCyls[e0.tri].AddBevelCylinderIndex(bevelCylinderIndex);
						if (!triBevelCyls[e1.tri].IsFull())
							triBevelCyls[e1.tri].AddBevelCylinderIndex(bevelCylinderIndex);
					}
				}
			}			
		}

		cout << "ExtTriangleMesh " << this << " bevel cylinders count: " << bevelCyls.size() << endl;

		delete[] bevelCylinders;
		bevelCylinders = new BevelCylinder[bevelCyls.size()];
		copy(bevelCyls.begin(), bevelCyls.end(), bevelCylinders);

		delete[] triBevelCylinders;
		triBevelCylinders = new TriangleBevelCylinders[triCount];
		copy(triBevelCyls.begin(), triBevelCyls.end(), triBevelCylinders);
	} else {
		delete[] bevelCylinders;
		bevelCylinders = nullptr;
		delete[] triBevelCylinders;
		triBevelCylinders = nullptr;
	}
	
	const double endTotal = WallClockTime();
	cout << "ExtTriangleMesh " << this << " bevel preprocessing time: " << (endTotal - start) << "secs" << endl;
}

void ExtTriangleMesh::Delete() {
	delete[] vertices;
	delete[] tris;

	delete[] normals;
	delete[] triNormals;

	for (UV *uv : uvs)
		delete[] uv;
	for (Spectrum *c : cols)
		delete[] c;
	for (float *a : alphas)
		delete[] a;
	for (float *v : vertAOV)
		delete[] v;
	for (float *t : triAOV)
		delete[] t;

	delete[] bevelCylinders;
	delete[] triBevelCylinders;
}

Normal *ExtTriangleMesh::ComputeNormals() {
	bool allocated;
	if (!normals) {
		allocated = true;
		normals = new Normal[vertCount];
	} else
		allocated = false;

	for (u_int i = 0; i < vertCount; ++i)
		normals[i] = Normal(0.f, 0.f, 0.f);
	for (u_int i = 0; i < triCount; ++i) {
		const Vector e1 = vertices[tris[i].v[1]] - vertices[tris[i].v[0]];
		const Vector e2 = vertices[tris[i].v[2]] - vertices[tris[i].v[0]];
		const Normal N = Normal(Normalize(Cross(e1, e2)));
		normals[tris[i].v[0]] += N;
		normals[tris[i].v[1]] += N;
		normals[tris[i].v[2]] += N;
	}
	//int printedWarning = 0;
	for (u_int i = 0; i < vertCount; ++i) {
		normals[i] = Normalize(normals[i]);
		// Check for degenerate triangles/normals, they can freeze the GPU
		if (isnan(normals[i].x) || isnan(normals[i].y) || isnan(normals[i].z)) {
			/*if (printedWarning < 15) {
				SDL_LOG("The model contains a degenerate normal (index " << i << ")");
				++printedWarning;
			} else if (printedWarning == 15) {
				SDL_LOG("The model contains more degenerate normals");
				++printedWarning;
			}*/
			normals[i] = Normal(0.f, 0.f, 1.f);
		}
	}

	return allocated ? normals : NULL;
}

void ExtTriangleMesh::ApplyTransform(const Transform &trans) {
	TriangleMesh::ApplyTransform(trans);

	if (normals) {
		for (u_int i = 0; i < vertCount; ++i) {
			normals[i] *= trans;
			normals[i] = Normalize(normals[i]);
		}
	}

	Preprocess();
}

void ExtTriangleMesh::CopyAOV(ExtMesh *destMesh) const {
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		if (HasVertexAOV(i)) {
			float *v = new float[vertCount];
			copy(&vertAOV[i][0], &vertAOV[i][0] + vertCount, v);
		}

		if (HasTriAOV(i)) {
			float *t = new float[triCount];
			copy(&triAOV[i][0], &triAOV[i][0] + triCount, t);
		}
	}
}

ExtTriangleMesh *ExtTriangleMesh::CopyExt(Point *meshVertices, Triangle *meshTris, Normal *meshNormals,
		array<UV *, EXTMESH_MAX_DATA_COUNT> *meshUVs,
		array<Spectrum *, EXTMESH_MAX_DATA_COUNT> *meshCols,
		array<float *, EXTMESH_MAX_DATA_COUNT> *meshAlphas,
		const float bRadius) const {
	Point *vs = meshVertices;
	if (!vs) {
		vs = AllocVerticesBuffer(vertCount);
		copy(vertices, vertices + vertCount, vs);
	}

	Triangle *ts = meshTris;
	if (!ts) {
		ts = AllocTrianglesBuffer(triCount);
		copy(tris, tris + triCount, ts);
	}

	Normal *ns = meshNormals;
	if (!ns && HasNormals()) {
		ns = new Normal[vertCount];
		copy(normals, normals + vertCount, ns);
	}

	array<UV *, EXTMESH_MAX_DATA_COUNT> us;
	array<Spectrum *, EXTMESH_MAX_DATA_COUNT> cs;
	array<float *, EXTMESH_MAX_DATA_COUNT> as;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		if (HasUVs(i) && (!meshUVs || !(*meshUVs)[i])) {
			us[i] = new UV[vertCount];
			copy(uvs[i], uvs[i] + vertCount, us[i]);
		} else
			us[i] = meshUVs ? (*meshUVs)[i] : nullptr;

		if (HasColors(i) && (!meshCols || !(*meshCols)[i])) {
			cs[i] = new Spectrum[vertCount];
			copy(cols[i], cols[i] + vertCount, cs[i]);
		} else
			cs[i] = meshCols ? (*meshCols)[i] : nullptr;

		if (HasAlphas(i) && (!meshAlphas || !(*meshAlphas)[i])) {
			as[i] = new float[vertCount];
			copy(alphas[i], alphas[i] + vertCount, as[i]);
		} else
			as[i] = meshAlphas ? (*meshAlphas)[i] : nullptr;
	}

	ExtTriangleMesh *m = new ExtTriangleMesh(vertCount, triCount,
			vs, ts, ns, &us, &cs, &as, bRadius);
	m->SetLocal2World(appliedTrans);
	
	// Copy AOV too
	CopyAOV(m);

	return m;
}

ExtTriangleMesh *ExtTriangleMesh::Copy(Point *meshVertices, Triangle *meshTris, Normal *meshNormals,
		UV *mUVs, Spectrum *mCols, float *mAlphas, const float bRadius) const {
	array<UV *, EXTMESH_MAX_DATA_COUNT> meshUVs;
	fill(meshUVs.begin(), meshUVs.end(), nullptr);
	if (mUVs)
		meshUVs[0] = mUVs;

	array<Spectrum *, EXTMESH_MAX_DATA_COUNT> meshCols;
	fill(meshCols.begin(), meshCols.end(), nullptr);
	if (mCols)
		meshCols[0] = mCols;

	array<float *, EXTMESH_MAX_DATA_COUNT> meshAlphas;
	fill(meshAlphas.begin(), meshAlphas.end(), nullptr);
	if (mAlphas)
		meshAlphas[0] = mAlphas;

	return CopyExt(meshVertices, meshTris, meshNormals, &meshUVs, &meshCols, &meshAlphas, bRadius);
}

ExtTriangleMesh *ExtTriangleMesh::Merge(const vector<const ExtTriangleMesh *> &meshes,
		const vector<Transform> *trans) {
	u_int totalVertexCount = 0;
	u_int totalTriangleCount = 0;

	for (auto mesh : meshes) {
		totalVertexCount += mesh->GetTotalVertexCount();
		totalTriangleCount += mesh->GetTotalTriangleCount();
	}

	assert (totalVertexCount > 0);
	assert (totalTriangleCount > 0);
	assert (meshes.size() > 0);

	Point *meshVertices = AllocVerticesBuffer(totalVertexCount);
	Triangle *meshTris = AllocTrianglesBuffer(totalTriangleCount);

	Normal *meshNormals = nullptr;
	array<UV *, EXTMESH_MAX_DATA_COUNT> meshUVs;
	array<Spectrum *, EXTMESH_MAX_DATA_COUNT> meshCols;
	array<float *, EXTMESH_MAX_DATA_COUNT> meshAlphas;
	array<float *, EXTMESH_MAX_DATA_COUNT> meshVertAOV;
	array<float *, EXTMESH_MAX_DATA_COUNT> meshTriAOV;

	if (meshes[0]->HasNormals())
		meshNormals = new Normal[totalVertexCount];

	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (meshes[0]->HasUVs(i))
			meshUVs[i] = new UV[totalVertexCount];
		else
			meshUVs[i] = nullptr;

		if (meshes[0]->HasColors(i))
			meshCols[i] = new Spectrum[totalVertexCount];
		else
			meshCols[i] = nullptr;

		if (meshes[0]->HasAlphas(i))
			meshAlphas[i] = new float[totalVertexCount];
		else
			meshAlphas[i] = nullptr;

		if (meshes[0]->HasVertexAOV(i))
			meshVertAOV[i] = new float[totalVertexCount];
		else
			meshVertAOV[i] = nullptr;

		if (meshes[0]->HasTriAOV(i))
			meshTriAOV[i] = new float[totalTriangleCount];
		else
			meshTriAOV[i] = nullptr;
	}
	
	u_int vIndex = 0;
	u_int iIndex = 0;
	for (u_int meshIndex = 0; meshIndex < meshes.size(); ++meshIndex) {
		const ExtTriangleMesh *mesh = meshes[meshIndex];
		const Transform *transformation = trans ? &((*trans)[meshIndex]) : nullptr;

		// It is a ExtTriangleMesh so I can use Transform::TRANS_IDENTITY everywhere
		// in the following code instead of local2World

		// Copy the mesh vertices
		if (transformation) {
			for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
				meshVertices[i + vIndex] = (*transformation) * mesh->GetVertex(Transform::TRANS_IDENTITY, i);
		} else {
			for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
				meshVertices[i + vIndex] = mesh->GetVertex(Transform::TRANS_IDENTITY, i);			
		}

		// Copy the mesh normals
		if (meshes[0]->HasNormals() != mesh->HasNormals())
			throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of normal definitions");
		if (meshes[0]->HasNormals()) {
			if (transformation) {
				for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
					meshNormals[i + vIndex] = Normalize((*transformation) * mesh->GetShadeNormal(Transform::TRANS_IDENTITY, i));
			} else {
				for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
					meshNormals[i + vIndex] = mesh->GetShadeNormal(Transform::TRANS_IDENTITY, i);
			}
		}

		for (u_int dataIndex = 0; dataIndex < EXTMESH_MAX_DATA_COUNT; dataIndex++) {
			// Copy the mesh uvs
			if (meshes[0]->HasUVs(dataIndex) != mesh->HasUVs(dataIndex))
				throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of UV definitions");
			if (meshes[0]->HasUVs(dataIndex)) {
				for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
					meshUVs[dataIndex][i + vIndex] = mesh->GetUV(i, dataIndex);
			}

			// Copy the mesh colors
			if (meshes[0]->HasColors(dataIndex) != mesh->HasColors(dataIndex))
				throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of color definitions");
			if (meshes[0]->HasColors(dataIndex)) {
				for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
					meshCols[dataIndex][i + vIndex] = mesh->GetColor(i, dataIndex);
			}

			// Copy the mesh alphas
			if (meshes[0]->HasAlphas(dataIndex) != mesh->HasAlphas(dataIndex))
				throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of alpha definitions");
			if (meshes[0]->HasAlphas(dataIndex)) {
				for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
					meshAlphas[dataIndex][i + vIndex] = mesh->GetAlpha(i, dataIndex);
			}

			// Copy the mesh vertex AOV
			if (meshes[0]->HasVertexAOV(dataIndex) != mesh->HasVertexAOV(dataIndex))
				throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of vertex AOV definitions");
			if (meshes[0]->HasVertexAOV(dataIndex)) {
				for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
					meshVertAOV[dataIndex][i + vIndex] = mesh->GetVertexAOV(i, dataIndex);
			}

			// Copy the mesh triangle AOV
			if (meshes[0]->HasTriAOV(dataIndex) != mesh->HasTriAOV(dataIndex))
				throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of triangle AOV definitions");
			if (meshes[0]->HasTriAOV(dataIndex)) {
				for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i)
					meshTriAOV[dataIndex][i + vIndex] = mesh->GetTriAOV(i, dataIndex);
			}
		}

		// Translate mesh indices
		const Triangle *tris = mesh->GetTriangles();
		for (u_int j = 0; j < mesh->GetTotalTriangleCount(); j++) {
			meshTris[iIndex].v[0] = tris[j].v[0] + vIndex;
			meshTris[iIndex].v[1] = tris[j].v[1] + vIndex;
			meshTris[iIndex].v[2] = tris[j].v[2] + vIndex;

			++iIndex;
		}

		
		vIndex += mesh->GetTotalVertexCount();
	}

	ExtTriangleMesh *newMesh = new ExtTriangleMesh(totalVertexCount, totalTriangleCount,
			meshVertices, meshTris, meshNormals, &meshUVs, &meshCols, &meshAlphas);
	for (u_int dataIndex = 0; dataIndex < EXTMESH_MAX_DATA_COUNT; dataIndex++) {
		newMesh->SetVertexAOV(dataIndex, meshVertAOV[dataIndex]);
		newMesh->SetTriAOV(dataIndex, meshTriAOV[dataIndex]);
	}

	return newMesh;
}

// For some reason, LoadSerialized() and SaveSerialized() must be in the same
// file of BOOST_CLASS_EXPORT_IMPLEMENT()

ExtTriangleMesh *ExtTriangleMesh::LoadSerialized(const string &fileName) {
	SerializationInputFile sif(fileName);

	ExtTriangleMesh *mesh;
	sif.GetArchive() >> mesh;

	if (!sif.IsGood())
		throw runtime_error("Error while loading serialized scene: " + fileName);

	return mesh;
}

void ExtTriangleMesh::SaveSerialized(const string &fileName) const {
	SerializationOutputFile sof(fileName);

	const ExtTriangleMesh *mesh = this;
	sof.GetArchive() << mesh;

	if (!sof.IsGood())
		throw runtime_error("Error while saving serialized mesh: " + fileName);

	sof.Flush();
}

//------------------------------------------------------------------------------
// ExtInstanceTriangleMesh
//------------------------------------------------------------------------------

// This is a workaround to a GCC bug described here:
//  https://svn.boost.org/trac10/ticket/3730
//  https://marc.info/?l=boost&m=126496738227673&w=2
namespace boost{
template<>
struct is_virtual_base_of<luxrays::InstanceTriangleMesh, luxrays::ExtInstanceTriangleMesh>: public mpl::true_ {};
}

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::ExtInstanceTriangleMesh)

void ExtInstanceTriangleMesh::UpdateMeshReferences(ExtTriangleMesh *oldMesh, ExtTriangleMesh *newMesh) {
	if (static_cast<ExtTriangleMesh *>(mesh) == oldMesh) {
		mesh = newMesh;
		cachedArea = false;
	}
}

//------------------------------------------------------------------------------
// ExtMotionTriangleMesh
//------------------------------------------------------------------------------

// This is a workaround to a GCC bug described here:
//  https://svn.boost.org/trac10/ticket/3730
//  https://marc.info/?l=boost&m=126496738227673&w=2
namespace boost{
template<>
struct is_virtual_base_of<luxrays::MotionTriangleMesh, luxrays::ExtMotionTriangleMesh>: public mpl::true_ {};
}

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::ExtMotionTriangleMesh)

void ExtMotionTriangleMesh::UpdateMeshReferences(ExtTriangleMesh *oldMesh, ExtTriangleMesh *newMesh) {
	if (static_cast<ExtTriangleMesh *>(mesh) == oldMesh) {
		mesh = oldMesh;
		cachedArea = false;
	}
}
