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
#include <execution>
#include <oneapi/tbb.h>

#include <boost/format.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <filesystem>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/trianglemesh.h"
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

ExtTriangleMesh::ExtTriangleMesh(
	VertexBuffer&& meshVertices,
	TriangleBuffer&& meshTris,
	NormalBuffer&& meshNormals,
	std::shared_ptr<UV[]> mUVs,
	std::shared_ptr<Spectrum[]> mCols,
	std::shared_ptr<float[]> mAlphas,
	const float bRadius
) :
	TriangleMesh(std::move(meshVertices), std::move(meshTris)),
	ExtMesh(bRadius),
	bevelCylinders(nullptr),
	bevelBoundingCylinders(nullptr),
	bevelBVHArrayNodes(nullptr)

{
	auto meshUVs = ExtMeshProp(mUVs, vertices.Count());
	auto meshCols = ExtMeshProp(mCols, vertices.Count());
	auto meshAlphas = ExtMeshProp(mAlphas, vertices.Count());

	Init(std::move(meshNormals), meshUVs, meshCols, meshAlphas);
}

ExtTriangleMesh::ExtTriangleMesh(
	VertexBuffer&& meshVertices,
	TriangleBuffer&& meshTris,
	NormalBuffer&& meshNormals,
	std::optional<std::span<UV>> mUVs,
	std::optional<std::span<Spectrum>> mCols,
	std::optional<std::span<float>> mAlphas,
	const float bRadius
) :
	TriangleMesh(std::move(meshVertices), std::move(meshTris)),
	ExtMesh(bRadius),
	bevelCylinders(nullptr),
	bevelBoundingCylinders(nullptr),
	bevelBVHArrayNodes(nullptr)

{
	auto applySpan = []<typename T>(
		std::optional<std::span<T>> optionalSpan
	) {
		auto meshProp = ExtMeshProp<T>();
		if (optionalSpan) {
			meshProp.SetLayer(0, *optionalSpan);
		}
		return meshProp;
	};

	auto meshUVs = applySpan(mUVs);
	auto meshCols = applySpan(mCols);
	auto meshAlphas = applySpan(mAlphas);

	Init(std::move(meshNormals), meshUVs, meshCols, meshAlphas);
}

ExtTriangleMesh::ExtTriangleMesh(
	VertexBuffer&& meshVertices,
	TriangleBuffer&& meshTris,
	NormalBuffer&& meshNormals,
	std::optional<ExtMeshProp<UV>> meshUVs,
	std::optional<ExtMeshProp<Spectrum>> meshCols,
	std::optional<ExtMeshProp<float>> meshAlphas,
	const float bRadius
) :
	TriangleMesh(std::move(meshVertices), std::move(meshTris)),
	ExtMesh(bRadius),
	bevelCylinders(nullptr),
	bevelBoundingCylinders(nullptr),
	bevelBVHArrayNodes(nullptr)
{
	Init(std::move(meshNormals), meshUVs, meshCols, meshAlphas);
}

void ExtTriangleMesh::Init(
	NormalBuffer&& meshNormals,
	std::optional<ExtMeshProp<UV>> meshUVs,
	std::optional<ExtMeshProp<Spectrum>> meshCols,
	std::optional<ExtMeshProp<float>> meshAlphas
) {
	if (meshUVs && (meshUVs->GetMaxLayerNumber() > EXTMESH_MAX_DATA_COUNT)) {
		throw runtime_error(
			"Error in ExtTriangleMesh::ExtTriangleMesh(): trying to define more ("
			+ ToString(meshUVs->GetMaxLayerNumber())
			+ ") UV sets than EXTMESH_MAX_DATA_COUNT"
		);
	}
	if (meshCols && (meshCols->GetMaxLayerNumber() > EXTMESH_MAX_DATA_COUNT)) {
		throw runtime_error(
			"Error in ExtTriangleMesh::ExtTriangleMesh(): trying to define more ("
			+ ToString(meshCols->GetMaxLayerNumber())
			+ ") Color sets than EXTMESH_MAX_DATA_COUNT"
		);
	}
	if (meshAlphas && (meshAlphas->GetMaxLayerNumber() > EXTMESH_MAX_DATA_COUNT)) {
		throw runtime_error(
			"Error in ExtTriangleMesh::ExtTriangleMesh(): trying to define more ("
			+ ToString(meshAlphas->GetMaxLayerNumber())
			+ ") Alpha sets than EXTMESH_MAX_DATA_COUNT"
		);
	}

	normals = std::move(meshNormals);
	triNormals.Allocate(tris.Count());

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
	#pragma omp parallel for
	for (long long i = 0; i < tris.Count(); ++i)
		triNormals[i] = tris[i].GetGeometryNormal(vertices);

	PreprocessBevel();
}

void ExtTriangleMesh::Delete() {

	uvs.DeleteAll();
	cols.DeleteAll();
	alphas.DeleteAll();
	vertAOV.DeleteAll();
	triAOV.DeleteAll();

	delete[] bevelCylinders;
	delete[] bevelBoundingCylinders;
}

void ExtTriangleMesh::ComputeNormals() {
	if (not bool(normals))
		normals = NormalBuffer(vertices.Count());

	const u_int vertCount = (u_int)vertices.Count();
	const u_int triCount = (u_int)tris.Count();

	// The minimum number of triangles for a chunk to be worth the temporary
	// memory it requires
	constexpr u_int MIN_TRIS_PER_CHUNK = 32768;

	// One chunk per worker thread, without however splitting small meshes more
	// than necessary and without exceeding a reasonable amount of temporary
	// memory (chunkCount * vertCount * sizeof(Normal) bytes)
	const u_int maxChunksByMem = Max(2u,
			(u_int)(size_t(96) * 1024 * 1024 / (size_t(vertCount) * sizeof(Normal) + 1)));
	const u_int chunkCount = Max(1u, Min(triCount / MIN_TRIS_PER_CHUNK,
			Min((u_int)tbb::this_task_arena::max_concurrency(), maxChunksByMem)));
	const u_int trisPerChunk = (triCount + chunkCount - 1) / chunkCount;

	// Each chunk records, in its own private array, the sum of the (not
	// normalized) normals of the triangles referencing a vertex. As every write
	// targets a chunk-private array, no lock or atomic operation is required,
	// and the result is deterministic: the chunks are combined below, in chunk
	// order, for every vertex.
	//
	// Note: this changes the order of the floating point additions, compared
	// to a serial pass over the triangles, so the computed normals can differ
	// by an ULP from the ones computed before.
	vector<Normal> chunkSums(size_t(chunkCount) * vertCount, Normal(0.f, 0.f, 0.f));
	tbb::parallel_for(0u, chunkCount, [&](u_int chunkIndex) {
		Normal *sums = &chunkSums[size_t(chunkIndex) * vertCount];

		const u_int startTri = chunkIndex * trisPerChunk;
		const u_int endTri = Min(startTri + trisPerChunk, triCount);
		for (u_int triIndex = startTri; triIndex < endTri; ++triIndex) {
			const Vector e1 = vertices[tris[triIndex].v[1]] - vertices[tris[triIndex].v[0]];
			const Vector e2 = vertices[tris[triIndex].v[2]] - vertices[tris[triIndex].v[0]];

			// The normal is normalized with a single division and 3
			// multiplications, instead of the 3 divisions of Normalize: this
			// changes the result by an ULP, like the chunked accumulation below
			const Vector c = Cross(e1, e2);
			const float invLen = 1.f / sqrtf(c.x * c.x + c.y * c.y + c.z * c.z);
			const Normal N(c.x * invLen, c.y * invLen, c.z * invLen);

			sums[tris[triIndex].v[0]] += N;
			sums[tris[triIndex].v[1]] += N;
			sums[tris[triIndex].v[2]] += N;
		}
	});

	// Combine the chunk results and normalize
	tbb::parallel_for(0u, vertCount, [&](u_int vertIndex) {
		Normal N(0.f, 0.f, 0.f);
		for (u_int chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
			N += chunkSums[size_t(chunkIndex) * vertCount + vertIndex];

		N = Normalize(N);
		// Check for degenerate triangles/normals, they can freeze the GPU
		if (isnan(N.x) || isnan(N.y) || isnan(N.z))
			N = Normal(0.f, 0.f, 1.f);
		normals[vertIndex] = N;
	});
}

void ExtTriangleMesh::ApplyTransform(const Transform &trans) {
	TriangleMesh::ApplyTransform(trans);

	if (normals) {
		for (u_int i = 0; i < vertices.Count(); ++i) {
			normals[i] *= trans;
			normals[i] = Normalize(normals[i]);
		}
	}

	Preprocess();
}

void ExtTriangleMesh::CopyAOV(ExtTriangleMeshRef destMesh) const {
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		if (HasVertexAOV(i)) {
			const auto [layer, size] = vertAOV.CopyLayer(i);
			destMesh.SetVertexAOV(i, layer, size);
		}

		if (HasTriAOV(i)) {
			auto [layer, size] = triAOV.CopyLayer(i);
			destMesh.SetTriAOV(i, layer, size);
		}
	}
}

ExtTriangleMeshUPtr ExtTriangleMesh::CopyExt(
	std::optional<VertexBuffer> meshVertices,
	std::optional<TriangleBuffer> meshTris,
	std::optional<NormalBuffer> meshNormals,
	std::optional<ExtMeshProp<UV>> meshUVs,
	std::optional<ExtMeshProp<Spectrum>> meshCols,
	std::optional<ExtMeshProp<float>> meshAlphas,
	const float bRadius) const
{
	VertexBuffer vs;
	if (meshVertices.has_value()) {
		vs = std::move(meshVertices.value());
	} else {
		vs.Allocate(vertices.Count());
		vs.Set(vertices);
	}

	TriangleBuffer ts;
	if (meshTris.has_value()) {
		ts = std::move(meshTris.value());
	} else {
		// Share the triangles instead of copying them: the triangles of a
		// copied mesh are never modified in place (only the vertices are)
		ts = TriangleBuffer::Share(tris);
	}

	NormalBuffer ns;
	if (meshNormals.has_value()) {
		ns = std::move(meshNormals.value());
	} else {
		ns.Allocate(normals.Count());
		ns.Set(normals);
	}


	// Optionals: UV, colors, alphas. Initialize from source
	ExtMeshProp<UV> us = uvs;
	ExtMeshProp<Spectrum> cs = cols;
	ExtMeshProp<float> as = alphas;


	// Copy overwriting values
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		if (meshUVs) {
			us.SetLayer(i, uvs.CopyLayer(i, meshUVs));
		}
		if (meshCols) {
			cs.SetLayer(i, cols.CopyLayer(i, meshCols));
		}
		if (meshAlphas) {
			as.SetLayer(i, alphas.CopyLayer(i, meshAlphas));
		}
	}

	auto m =  std::make_unique<ExtTriangleMesh>(
		std::move(vs), std::move(ts), std::move(ns), us, cs, as, bRadius);

	m->SetLocal2World(appliedTrans);

	// Copy AOV too
	CopyAOV(*m);

	return m;
}

ExtTriangleMeshUPtr ExtTriangleMesh::Copy(
	std::optional<VertexBuffer> meshVertices,
	std::optional<TriangleBuffer> meshTris,
	std::optional<NormalBuffer> meshNormals,
	std::optional<std::span<UV>> mUVs,
	std::optional<std::span<Spectrum>> mCols,
	std::optional<std::span<float>> mAlphas,
	const float bRadius
) const {
	ExtMeshProp<UV> meshUVs;
	if (mUVs)
		meshUVs.SetLayer(0, *mUVs);

	ExtMeshProp<Spectrum> meshCols;
	if (mCols)
		meshCols.SetLayer(0, *mCols);

	ExtMeshProp<float> meshAlphas;
	if (mAlphas)
		meshAlphas.SetLayer(0, *mAlphas);

	return CopyExt(
		std::move(meshVertices),
		std::move(meshTris),
		std::move(meshNormals),
		mUVs ? std::optional(meshUVs) : std::nullopt,
		mCols ? std::optional(meshCols) : std::nullopt,
		mAlphas ? std::optional(meshAlphas) : std::nullopt,
		bRadius
	);
}

ExtTriangleMeshUPtr ExtTriangleMesh::Merge(
	std::vector<std::reference_wrapper<const ExtTriangleMesh>> meshes,
	std::optional<std::vector<Transform>> trans
) {
	u_int totalVertexCount = 0;
	u_int totalTriangleCount = 0;

	for (MeshConstRef mesh : meshes) {
		totalVertexCount += mesh.GetTotalVertexCount();
		totalTriangleCount += mesh.GetTotalTriangleCount();
	}

	assert (totalVertexCount > 0);
	assert (totalTriangleCount > 0);
	assert (meshes.size() > 0);

	VertexBuffer meshVertices(totalVertexCount);
	TriangleBuffer meshTris(totalTriangleCount);

	NormalBuffer meshNormals;
	ExtMeshProp<UV> meshUVs;
	ExtMeshProp<Spectrum> meshCols;
	ExtMeshProp<float> meshAlphas;
	ExtMeshProp<float> meshVertAOV;
	ExtMeshProp<float> meshTriAOV;

	ExtTriangleMeshConstRef mesh0 = meshes[0];
	if (mesh0.HasNormals())
		meshNormals.Allocate(totalVertexCount);

	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (mesh0.HasUVs(i))
			meshUVs.AllocateLayer(i, totalVertexCount);

		if (mesh0.HasColors(i))
			meshCols.AllocateLayer(i, totalVertexCount);

		if (mesh0.HasAlphas(i))
			meshAlphas.AllocateLayer(i, totalVertexCount);

		if (mesh0.HasVertexAOV(i))
			meshVertAOV.AllocateLayer(i, totalVertexCount);

		if (mesh0.HasTriAOV(i))
			meshTriAOV.AllocateLayer(i, totalTriangleCount);
	}

	u_int vIndex = 0;
	u_int iIndex = 0;
	for (u_int meshIndex = 0; meshIndex < meshes.size(); ++meshIndex) {
		ExtTriangleMeshConstRef mesh = meshes[meshIndex];
		const Transform *transformation = trans ? &((*trans)[meshIndex]) : nullptr;

		// It is a ExtTriangleMesh so I can use Transform::TRANS_IDENTITY everywhere
		// in the following code instead of local2World

		// Copy the mesh vertices
		if (transformation) {
			for (u_int i = 0; i < mesh.GetTotalVertexCount(); ++i)
				meshVertices[i + vIndex] = (*transformation) * mesh.GetVertex(Transform::TRANS_IDENTITY, i);
		} else {
			for (u_int i = 0; i < mesh.GetTotalVertexCount(); ++i)
				meshVertices[i + vIndex] = mesh.GetVertex(Transform::TRANS_IDENTITY, i);			
		}

		// Copy the mesh normals
		if (mesh0.HasNormals() != mesh.HasNormals())
			throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of normal definitions");
		if (mesh0.HasNormals()) {
			if (transformation) {
				for (u_int i = 0; i < mesh.GetTotalVertexCount(); ++i)
					meshNormals[i + vIndex] = Normalize((*transformation) * mesh.GetShadeNormal(Transform::TRANS_IDENTITY, i));
			} else {
				for (u_int i = 0; i < mesh.GetTotalVertexCount(); ++i)
					meshNormals[i + vIndex] = mesh.GetShadeNormal(Transform::TRANS_IDENTITY, i);
			}
		}

		for (u_int dataIndex = 0; dataIndex < EXTMESH_MAX_DATA_COUNT; dataIndex++) {
			// Copy the mesh uvs
			if (mesh0.HasUVs(dataIndex) != mesh.HasUVs(dataIndex))
				throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of UV definitions");
			if (mesh0.HasUVs(dataIndex)) {
				for (u_int i = 0; i < mesh.GetTotalVertexCount(); ++i)
					meshUVs[dataIndex][i + vIndex] = mesh.GetUV(i, dataIndex);
			}

			// Copy the mesh colors
			if (mesh0.HasColors(dataIndex) != mesh.HasColors(dataIndex))
				throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of color definitions");
			if (mesh0.HasColors(dataIndex)) {
				for (u_int i = 0; i < mesh.GetTotalVertexCount(); ++i)
					meshCols[dataIndex][i + vIndex] = mesh.GetColor(i, dataIndex);
			}

			// Copy the mesh alphas
			if (mesh0.HasAlphas(dataIndex) != mesh.HasAlphas(dataIndex))
				throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of alpha definitions");
			if (mesh0.HasAlphas(dataIndex)) {
				for (u_int i = 0; i < mesh.GetTotalVertexCount(); ++i)
					meshAlphas[dataIndex][i + vIndex] = mesh.GetAlpha(i, dataIndex);
			}

			// Copy the mesh vertex AOV
			if (mesh0.HasVertexAOV(dataIndex) != mesh.HasVertexAOV(dataIndex))
				throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of vertex AOV definitions");
			if (mesh0.HasVertexAOV(dataIndex)) {
				for (u_int i = 0; i < mesh.GetTotalVertexCount(); ++i)
					meshVertAOV[dataIndex][i + vIndex] = mesh.GetVertexAOV(i, dataIndex);
			}

			// Copy the mesh triangle AOV
			if (mesh0.HasTriAOV(dataIndex) != mesh.HasTriAOV(dataIndex))
				throw runtime_error("Error in ExtTriangleMesh::Merge(): trying to merge meshes with different type of triangle AOV definitions");
			if (mesh0.HasTriAOV(dataIndex)) {
				for (u_int i = 0; i < mesh.GetTotalTriangleCount(); ++i)
					meshTriAOV[dataIndex][i + vIndex] = mesh.GetTriAOV(i, dataIndex);
			}
		}

		// Translate mesh indices
		const auto tris(mesh.GetTriangles());
		for (u_int j = 0; j < mesh.GetTotalTriangleCount(); j++) {
			meshTris[iIndex].v[0] = tris[j].v[0] + vIndex;
			meshTris[iIndex].v[1] = tris[j].v[1] + vIndex;
			meshTris[iIndex].v[2] = tris[j].v[2] + vIndex;

			++iIndex;
		}


		vIndex += mesh.GetTotalVertexCount();
	}

	auto newMesh = std::make_unique<ExtTriangleMesh>(
		std::move(meshVertices),
		std::move(meshTris),
		std::move(meshNormals),
		meshUVs,
		meshCols,
		meshAlphas
	);

	for (u_int dataIndex = 0; dataIndex < EXTMESH_MAX_DATA_COUNT; dataIndex++) {
		newMesh->SetVertexAOV(dataIndex, meshVertAOV.GetLayer(dataIndex), meshVertAOV.GetLayerSize());
		newMesh->SetTriAOV(dataIndex, meshTriAOV.GetLayer(dataIndex), meshTriAOV.GetLayerSize());
	}

	return newMesh;
}

// For some reason, LoadSerialized() and SaveSerialized() must be in the same
// file of BOOST_CLASS_EXPORT_IMPLEMENT()

ExtTriangleMeshUPtr ExtTriangleMesh::LoadSerialized(const string &fileName) {
	SerializationInputFile sif(fileName);

	ExtTriangleMesh * mesh;
	sif.GetArchive() >> mesh;

	if (!sif.IsGood())
		throw runtime_error("Error while loading serialized scene: " + fileName);

	ExtTriangleMeshUPtr res(mesh);
	return res;
}

void ExtTriangleMesh::SaveSerialized(const string &fileName) const {
	SerializationOutputFile sof(fileName);

	//const ExtTriangleMesh *mesh = this;
	sof.GetArchive() << this;

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

void ExtInstanceTriangleMesh::UpdateMeshReferences(const ExtTriangleMesh& oldMesh, ExtTriangleMesh& newMesh) {
	if (&static_cast<ExtTriangleMesh&>(*mesh) == &oldMesh) {
		mesh = &newMesh;
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

void ExtMotionTriangleMesh::UpdateMeshReferences(const ExtTriangleMesh& oldMesh, ExtTriangleMesh& newMesh) {
	if (&static_cast<ExtTriangleMesh&>(*mesh) == &oldMesh) {
		mesh = &newMesh;
		cachedArea = false;
	}
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
