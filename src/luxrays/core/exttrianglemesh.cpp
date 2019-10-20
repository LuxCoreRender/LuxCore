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

#include <iostream>
#include <fstream>
#include <cstring>

#include <boost/format.hpp>
#include <boost/filesystem.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/utils/ply/rply.h"
#include "luxrays/utils/serializationutils.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// ExtMesh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::ExtMesh)

// The optimized version for ExtTriangleMesh where I can ignore local2World
// because it is an identity
void ExtMesh::GetDifferentials(const Transform &local2World,
		const u_int triIndex, const Normal &shadeNormal,
        Vector *dpdu, Vector *dpdv,
        Normal *dndu, Normal *dndv) const {
    // Compute triangle partial derivatives
    const Triangle &tri = GetTriangles()[triIndex];
	const u_int v0Index = tri.v[0];
	const u_int v1Index = tri.v[1];
	const u_int v2Index = tri.v[2];
	
    UV uv0, uv1, uv2;
    if (HasUVs()) {
        uv0 = GetUV(v0Index);
        uv1 = GetUV(v1Index);
        uv2 = GetUV(v2Index);
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
		Point *meshVertices, Triangle *meshTris, Normal *meshNormals, UV *meshUV,
			Spectrum *meshCols, float *meshAlpha) :
		TriangleMesh(meshVertCount, meshTriCount, meshVertices, meshTris) {
	normals = meshNormals;
	uvs = meshUV;
	cols = meshCols;
	alphas = meshAlpha;

	triNormals = new Normal[triCount];
	Preprocess();
}

void ExtTriangleMesh::Preprocess() {
	// Compute all triangle normals
	for (u_int i = 0; i < triCount; ++i)
		triNormals[i] = tris[i].GetGeometryNormal(vertices);
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

	appliedTrans = appliedTrans * trans;

	if (normals) {
		for (u_int i = 0; i < vertCount; ++i) {
			normals[i] *= trans;
			normals[i] = Normalize(normals[i]);
		}
	}

	Preprocess();
}

ExtTriangleMesh *ExtTriangleMesh::Copy(Point *meshVertices, Triangle *meshTris, Normal *meshNormals, UV *meshUV,
			Spectrum *meshCols, float *meshAlpha) const {
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

	UV *us = meshUV;
	if (!us && HasUVs()) {
		us = new UV[vertCount];
		copy(uvs, uvs + vertCount, us);
	}

	Spectrum *cs = meshCols;
	if (!cs && HasColors()) {
		cs = new Spectrum[vertCount];
		copy(cols, cols + vertCount, cs);
	}
	
	float *as = meshAlpha;
	if (!as && HasAlphas()) {
		as = new float[vertCount];
		copy(alphas, alphas + vertCount, as);
	}

	ExtTriangleMesh *m = new ExtTriangleMesh(vertCount, triCount, vs, ts, ns, us, cs, as);
	m->appliedTrans = appliedTrans;
	return m;
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
	UV *meshUV = nullptr;
	Spectrum *meshCols = nullptr;
	float *meshAlpha = nullptr;

	const bool hasNormals = meshes[0]->HasNormals();
	const bool hasUVs = meshes[0]->HasUVs();
	const bool hasColors = meshes[0]->HasColors();
	const bool hasAlphas = meshes[0]->HasAlphas();

	if (hasNormals)
		meshNormals = new Normal[totalVertexCount];
	if (hasUVs)
		meshUV = new UV[totalVertexCount];
	if (hasColors)
		meshCols = new Spectrum[totalVertexCount];
	if (hasAlphas)
		meshAlpha = new float[totalVertexCount];
	
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
		if (hasNormals != mesh->HasNormals())
			throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of normal definitions");
		if (hasNormals) {
			if (transformation) {
				for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
					meshNormals[i + vIndex] = Normalize((*transformation) * mesh->GetShadeNormal(Transform::TRANS_IDENTITY, i));
			} else {
				for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
					meshNormals[i + vIndex] = mesh->GetShadeNormal(Transform::TRANS_IDENTITY, i);
			}
		}

		// Copy the mesh uvs
		if (hasUVs != mesh->HasUVs())
			throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of UV definitions");
		if (hasUVs) {
			for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
				meshUV[i + vIndex] = mesh->GetUV(i);
		}

		// Copy the mesh colors
		if (hasColors != mesh->HasColors())
			throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of color definitions");
		if (hasColors) {
			for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
				meshCols[i + vIndex] = mesh->GetColor(i);
		}

		// Copy the mesh alphas
		if (hasAlphas != mesh->HasAlphas())
			throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of alpha definitions");
		if (hasAlphas) {
			for (u_int i = 0; i < mesh->GetTotalVertexCount(); ++i)
				meshAlpha[i + vIndex] = mesh->GetAlpha(i);
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

	return new ExtTriangleMesh(totalVertexCount, totalTriangleCount,
			meshVertices, meshTris, meshNormals, meshUV, meshCols, meshAlpha);
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
