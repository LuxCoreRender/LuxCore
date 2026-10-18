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

#include <boost/format.hpp>
#include <oneapi/tbb.h>

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/displacement.h"
#include "luxrays/core/trianglemesh.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;


namespace {

// The minimum number of triangles for a chunk to be worth the temporary memory
// it requires
constexpr u_int MIN_TRIS_PER_CHUNK = 1024;

// The triangle index of the vertices not referenced by any triangle
constexpr u_int NO_TRIANGLE = 0xffffffffu;

// Returns, for each vertex, the index of the lowest-indexed triangle
// referencing it, or 0 if the vertex is not referenced by any triangle.
//
// The triangle array is partitioned in chunks and every chunk records, in its
// own private array, the minimum index of the triangles referencing a vertex.
// The chunk results are then combined with a per-vertex minimum. As every
// write targets a chunk-private array, no lock or atomic operation is
// required, and the result is deterministic: the minimum always wins, exactly
// like a serial first-wins pass over the triangles.
vector<u_int> ComputeVertexTriangleIndex(const ExtTriangleMesh &mesh) {
	const u_int vertCount = mesh.GetTotalVertexCount();
	const u_int triCount = mesh.GetTotalTriangleCount();
	const auto tris = mesh.GetTriangles();

	// One chunk per worker thread, without however splitting small meshes more
	// than necessary and without exceeding a small amount of temporary memory
	// (chunkCount * vertCount * sizeof(u_int) bytes): the chunk arrays must
	// stay cache friendly or the scatter pass below becomes memory bound and
	// slower than with less chunks
	const u_int maxChunksByMem = Max(2u,
			(u_int)(size_t(16) * 1024 * 1024 / (size_t(vertCount) * sizeof(u_int) + 1)));
	const u_int chunkCount = Max(1u, Min(triCount / MIN_TRIS_PER_CHUNK,
			Min((u_int)tbb::this_task_arena::max_concurrency(), maxChunksByMem)));
	const u_int trisPerChunk = (triCount + chunkCount - 1) / chunkCount;

	// The minimum triangle index referencing each vertex, per chunk. The chunk
	// arrays are disjoint so the writes are race free
	vector<u_int> chunkMins(size_t(chunkCount) * vertCount, NO_TRIANGLE);
	tbb::parallel_for(0u, chunkCount, [&](u_int chunkIndex) {
		u_int *mins = &chunkMins[size_t(chunkIndex) * vertCount];

		const u_int startTri = chunkIndex * trisPerChunk;
		const u_int endTri = Min(startTri + trisPerChunk, triCount);
		for (u_int triIndex = startTri; triIndex < endTri; ++triIndex) {
			const Triangle &tri = tris[triIndex];
			for (u_int j = 0; j < 3; ++j)
				mins[tri.v[j]] = Min(mins[tri.v[j]], triIndex);
		}
	});

	// Combine the chunk results: the lowest-indexed triangle referencing each
	// vertex wins
	vector<u_int> triangleIndex(vertCount, 0);
	tbb::parallel_for(0u, vertCount, [&](u_int vertIndex) {
		u_int triIndex = NO_TRIANGLE;
		for (u_int chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
			triIndex = Min(triIndex,
					chunkMins[size_t(chunkIndex) * vertCount + vertIndex]);
		triangleIndex[vertIndex] = (triIndex == NO_TRIANGLE) ? 0u : triIndex;
	});

	return triangleIndex;
}

// Computes the geometry differentials of each vertex from the triangle
// referencing it with the lowest index (see ComputeVertexTriangleIndex). The
// vertices not referenced by any triangle get zeroed differentials.
//
// The differential buffers are passed uninitialized: this function writes all
// their entries.
void ComputeVertexDifferentials(const ExtTriangleMesh &mesh,
		const vector<u_int> &triangleIndex,
		std::span<Vector> dpdu, std::span<Vector> dpdv,
		std::span<Normal> dndu, std::span<Normal> dndv) {
	if (mesh.GetTotalTriangleCount() == 0)
		return;

	const u_int vertCount = (u_int)triangleIndex.size();
	const auto tris = mesh.GetTriangles();

	tbb::parallel_for(0u, vertCount, [&](u_int vertIndex) {
		const u_int triIndex = triangleIndex[vertIndex];
		const Triangle &tri = tris[triIndex];

		// A vertex is always a corner of its associated triangle. If it is
		// not, the vertex is not referenced by any triangle (index 0): it gets
		// zeroed differentials
		if (tri.v[0] != vertIndex && tri.v[1] != vertIndex && tri.v[2] != vertIndex) {
			dpdu[vertIndex] = Vector(0.f, 0.f, 0.f);
			dpdv[vertIndex] = Vector(0.f, 0.f, 0.f);
			dndu[vertIndex] = Normal(0.f, 0.f, 0.f);
			dndv[vertIndex] = Normal(0.f, 0.f, 0.f);
			return;
		}

		const Normal shadeN = mesh.GetShadeNormal(Transform::TRANS_IDENTITY, vertIndex);
		mesh.GetDifferentials(Transform::TRANS_IDENTITY, triIndex, shadeN, 0,
				&dpdu[vertIndex], &dpdv[vertIndex],
				&dndu[vertIndex], &dndv[vertIndex]);
	});
}

} // namespace


DisplacementShape::DisplacementShape(luxrays::ExtTriangleMeshRef srcMesh, const Texture &dispMap,
		const Params &params) {
	SDL_LOG("Displacement shape " << srcMesh.GetName() << " with texture " << dispMap.GetName());

	// I need vertex normals
	if (!srcMesh.HasNormals())
		srcMesh.ComputeNormals();

	// I need vertex UVs for vector displacement
	if ((params.mapType == VECTOR_DISPLACEMENT) && !srcMesh.HasUVs(params.uvIndex))
		throw runtime_error("Displacement shape for vector displacement can be used only with mesh having UVs defined");

	const double startTime = WallClockTime();

	const u_int vertCount = srcMesh.GetTotalVertexCount();
	const auto vertices = srcMesh.GetVertices();
	VertexBuffer newVertices(vertCount);

	// I need to build the dpdu, dpdv, dndu, dndv for each vertex. They are mostly
	// used for vector displacement but they may be used by the texture too.
	// They are allocated uninitialized: ComputeVertexDifferentials writes all
	// the entries, the ones of the vertices not referenced by any triangle
	// included
	const std::unique_ptr<Vector[]> dpduStorage(new Vector[vertCount]);
	const std::unique_ptr<Vector[]> dpdvStorage(new Vector[vertCount]);
	const std::unique_ptr<Normal[]> dnduStorage(new Normal[vertCount]);
	const std::unique_ptr<Normal[]> dndvStorage(new Normal[vertCount]);
	const std::span<Vector> dpdu(dpduStorage.get(), vertCount);
	const std::span<Vector> dpdv(dpdvStorage.get(), vertCount);
	const std::span<Normal> dndu(dnduStorage.get(), vertCount);
	const std::span<Normal> dndv(dndvStorage.get(), vertCount);

	// The lowest-indexed triangle referencing each vertex. Unreferenced
	// vertices get index 0.
	const vector<u_int> triangleIndex = ComputeVertexTriangleIndex(srcMesh);

	// Compute the geometry differentials of the vertices
	ComputeVertexDifferentials(srcMesh, triangleIndex, dpdu, dpdv, dndu, dndv);

	const auto tris(srcMesh.GetTriangles());

	const u_int binormalIndex = params.mapChannels[0];
	const u_int tangentIndex = params.mapChannels[1];
	const u_int normalIndex = params.mapChannels[2];

	tbb::parallel_for(0u, vertCount, [&](u_int i) {
		HitPoint hitPoint;
		
		hitPoint.fixedDir = Vector(0.f, 0.f, 1.f);
		hitPoint.p = srcMesh.GetVertex(Transform::TRANS_IDENTITY, i);

		hitPoint.geometryN = srcMesh.GetShadeNormal(Transform::TRANS_IDENTITY, i);
		hitPoint.interpolatedN = hitPoint.geometryN;
		hitPoint.shadeN = hitPoint.interpolatedN;

		hitPoint.defaultUV = srcMesh.HasUVs(params.uvIndex) ? srcMesh.GetUV(i, params.uvIndex) : UV(0.f, 0.f);
		hitPoint.mesh = &srcMesh;
		hitPoint.triangleIndex = triangleIndex[i];
		if (i == tris[hitPoint.triangleIndex].v[0]) {
			// First vertex of the triangle
			hitPoint.triangleBariCoord1 = 0.f;
			hitPoint.triangleBariCoord2 = 0.f;
		} else if (i == tris[hitPoint.triangleIndex].v[1]) {
			// Second vertex of the triangle
			hitPoint.triangleBariCoord1 = 1.f;
			hitPoint.triangleBariCoord2 = 0.f;
		} else {
			// Last vertex of the triangle
			hitPoint.triangleBariCoord1 = 0.f;
			hitPoint.triangleBariCoord2 = 1.f;
		}

		hitPoint.dpdu = dpdu[i];
		hitPoint.dpdv = dpdv[i];
		hitPoint.dndu = dndu[i];
		hitPoint.dndv = dndv[i];

		hitPoint.passThroughEvent = 0.f;
		srcMesh.GetLocal2World(0.f, hitPoint.localToWorld);
		hitPoint.interiorVolume = nullptr;
		hitPoint.exteriorVolume = nullptr;
		hitPoint.objectID = 0;
		hitPoint.fromLight = false;
		hitPoint.intoObject = true;
		hitPoint.throughShadowTransparency = false;

		Vector disp;
		if (params.mapType == HIGHT_DISPLACEMENT)
			disp = (dispMap.GetFloatValue(hitPoint) * params.scale + params.offset) *
					Vector(hitPoint.shadeN);
		else {
			// Not using dispOffset parameter because it doesn't make very much sense
			// for vector displacement
			const Spectrum dispValue = dispMap.GetSpectrumValue(hitPoint) * params.scale;

			// Build the local reference system, uses shadeN, dpdu and dpdv
			const Frame frame = hitPoint.GetFrame();

			disp = frame.ToWorld(Vector(dispValue.c[binormalIndex], dispValue.c[tangentIndex], dispValue.c[normalIndex]));


			// I work on tangent space and in this case: R is an offset along
			// the tangent, G along the normal and B along the bitangent.
			// This is the Blender standard.
			//disp = frame.ToWorld(Vector(dispValue.c[2], dispValue.c[0], dispValue.c[1]));

			// This is the Mudbox standard.
			//disp = frame.ToWorld(Vector(dispValue.c[0], dispValue.c[2], dispValue.c[1]));
		}

		newVertices[i] = vertices[i] + disp;
	});

	// Make a copy of the original mesh and overwrite vertex information
	mesh = srcMesh.Copy(
		std::move(newVertices),
		std::nullopt,
		std::nullopt,
		std::nullopt,
		std::nullopt,
		std::nullopt
	);
	if (params.normalSmooth)
		mesh->ComputeNormals();

	// For some debugging
	//mesh->Save("debug.ply");

	const double endTime = WallClockTime();
	SDL_LOG("Displacement time: " << (boost::format("%.3f") % (endTime - startTime)) << "secs");
}

DisplacementShape::~DisplacementShape() {
}

ExtTriangleMeshUPtr DisplacementShape::RefineImpl(SceneConstRef scene) {
	return std::move(mesh);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
