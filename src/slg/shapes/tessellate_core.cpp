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

#include "slg/shapes/tessellate_core.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <stdexcept>

#include <oneapi/tbb.h>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/shape.h"
#include "luxrays/utils/strutils.h"
#include "slg/core/sdl.h"

using namespace luxrays;

namespace slg {

namespace {

// Sentinel for "no face" in LocalCoords/vertexFirstFace
constexpr u_int NO_FACE = std::numeric_limits<u_int>::max();

// Checked narrowing of mesh counts (size_t, from Buffer::Count()) to the
// u_int index type: throws on overflow instead of silently truncating
u_int CheckedCount(size_t count) {
	if (count > (size_t)std::numeric_limits<u_int>::max())
		throw std::runtime_error(
			"mesh count exceeds u_int limit: " + ToString(count));
	return (u_int)count;
}

// Edge table of a triangle topology, built from the faces: edges are
// deduplicated, edge orientation is the one of the first incident face.
// All indices use the LuxCore u_int type.
struct EdgeTable {
	u_int numEdges = 0;
	std::vector<u_int> edgeV0, edgeV1;   // per edge
	std::vector<u_int> edgeFirstFace;     // per edge: first incident face
	std::vector<u_int> vertexFirstFace;   // per vertex: first incident face (NO_FACE if none)
	// Per face, the edge ids of its 3 sides (side c = (v[c], v[c+1])): a
	// contiguous array, so the parallel tessellation loops never touch the
	// node-based map below (random reads)
	std::vector<u_int> faceEdges;         // 3 entries per face
	std::map<std::pair<u_int, u_int>, u_int> edgeIndex;  // sorted vertex pair -> edge (construction-time dedup only)

	EdgeTable(u_int numVertices, std::span<const Triangle> faces) {
		vertexFirstFace.assign(numVertices, NO_FACE);
		faceEdges.reserve(faces.size() * 3);
		for (u_int f = 0; f < faces.size(); ++f) {
			for (u_int c = 0; c < 3; ++c) {
				const u_int a = faces[f].v[c];
				const u_int b = faces[f].v[(c + 1) % 3];
				if (vertexFirstFace[a] == NO_FACE)
					vertexFirstFace[a] = f;
				const auto key = std::minmax(a, b);
				const auto it = edgeIndex.find(key);
				if (it != edgeIndex.end()) {
					faceEdges.push_back(it->second);
					continue;
				}
				edgeIndex[key] = numEdges;
				faceEdges.push_back(numEdges);
				++numEdges;
				edgeV0.push_back(a);
				edgeV1.push_back(b);
				edgeFirstFace.push_back(f);
			}
		}
	}
};

// Count helpers on plain counts (the public ones take a mesh)
u_int PosCount(u_int numVertices, u_int numEdges, u_int numFaces, int N) {
	const u_int numEdgeInteriorPoints = N - 1;
	const u_int numTriangleInteriorPoints = (N - 1) * (N - 2) / 2;
	return numVertices
		+ numEdgeInteriorPoints * numEdges
		+ numTriangleInteriorPoints * numFaces;
}

u_int TriCount(u_int numFaces, int N) {
	return numFaces * (u_int)N * (u_int)N;
}

///
//	/// Tessellate input mesh
//	///
//	/// Apply a triforce tessellation to input mesh.
//	///
//	/// This tessellation splits edges in N segments,
//	/// and generates N² resulting triangles by input triangle.
//	/// It's been reimplemented here in order to use multithreading (osd
//	/// implementation is monothreaded...).
//	///
//	/// Input:
//	///           V2
//	///          / \
//	///         /  \
//	///        /   \
//	///      V0----V1
//	///
//	///
//	///	Output (N=4):
//	///
//	///          V2
//	///         / \
//	///        E7-E8
//	///       / \/ \
//	///      E5-I3-E6
//	///     / \/ \/ \
//	///    E3-I0-I1-E4
//	///   / \/ \/ \/ \
//	///	V0-E0-E1-E2-V1
//	///
//	///	Nota:
//	///	Vx - initial vertex (to be kept)
//	///	Ex - edge vertex (to be created)
//	///	Ix - internal vertex (to be created)
//	///
TessellationData
TessellateTopologyData(
	u_int numVertices,
	std::span<const Triangle> faces,
	const size_t N,
	std::vector<LocalCoords> *cornerCoords
) {
	// This is triforce tessellation.

	const EdgeTable edges(numVertices, faces);
	const u_int numEdges = edges.numEdges;
	const u_int numFaces = (u_int)faces.size();

	// Allocate outputs
	CoordVector tessCoords;
	u_int numTriangles = TriCount(numFaces, N);
	u_int numPoints = PosCount(numVertices, numEdges, numFaces, N);
	TriangleBuffer tessTris(numTriangles);
	if (cornerCoords)
		cornerCoords->resize(3 * (size_t)numTriangles);

	// Check
	SDL_LOG(
		"Tessellating with factor N = "
		<< N
	);
	if ((size_t)numFaces * N * N > (size_t)std::numeric_limits<u_int>::max()) {
		throw std::runtime_error(
			"FACE_COUNT * N * N > numeric limit ("
			+ ToString(std::numeric_limits<u_int>::max())
			+ ")"
		);
	}

	// Useful variables and constants
	size_t offset;

	// Number of coarse vertices
	const u_int numCoarseVertices = numVertices;

	// Number of interior points in an edge
	const int numEdgeInteriorPoints = N - 1;

	// Number of interior points in a triangle
	const int numTriangleInteriorPoints = (N - 1) * (N - 2) / 2;

	// Number of coords to be computed
	const u_int numCoords = numPoints;

	// Size tessCoords
	tessCoords.resize(numCoords);

	// Get vertex local coordinates in given face
	auto getVertexLocalCoords = [&faces](u_int face, u_int vertex) {
		const Triangle &ft = faces[face];
		if (vertex == ft.v[0]) {
			return LocalCoords(face, 0.f, 0.f);
		} else if (vertex == ft.v[1]) {
			return LocalCoords(face, 1.f, 0.f);
		} else if (vertex == ft.v[2]) {
			return LocalCoords(face, 0.f, 1.f);
		}

		throw std::runtime_error("Error in getVertexLocalCoords");
	};

	// Step #1 - Initialize with initial (coarse) vertices
	tbb::parallel_for(tbb::blocked_range<u_int>(0, numCoarseVertices, 4096),
		[&](const tbb::blocked_range<u_int> &r) {
		for (u_int vertex = r.begin(); vertex < r.end(); ++vertex) {
			const u_int face = edges.vertexFirstFace[vertex];
			if (face != NO_FACE) {
				// Choose first face (arbitrary)
				tessCoords[vertex] = getVertexLocalCoords(face, vertex);
			} else {
				// Isolated vertex (not referenced by any face) — assign
				// a default coordinate; it won't appear in any triangle
				tessCoords[vertex] = LocalCoords((u_int)0, 0.f, 0.f);
			}
		}
	});
	offset = numCoarseVertices;

	// Step #2 - Add edge vertices
	tbb::parallel_for(tbb::blocked_range<u_int>(0, numEdges, 1024),
		[&](const tbb::blocked_range<u_int> &r) {
		for (u_int edge = r.begin(); edge < r.end(); ++edge) {
			// Find face
			u_int face = edges.edgeFirstFace[edge];
			// Find edge vertices.
			u_int v0 = edges.edgeV0[edge];
			u_int v1 = edges.edgeV1[edge];

			LocalCoords c0 = getVertexLocalCoords(face, v0);
			LocalCoords c1 = getVertexLocalCoords(face, v1);

			for (int i = 1; i < (int)N; ++i) {  // Only edge interior vertices, not extremities
				LocalCoords coords(c0, c1, i, N);  // Interpolate from c0 and c1
				tessCoords[offset + edge * numEdgeInteriorPoints + i - 1 ] = coords;
			}
		}
	});
	offset = numCoarseVertices + (size_t)numEdgeInteriorPoints * numEdges;

	// Step #3 - Add interior points
	tbb::parallel_for(tbb::blocked_range<u_int>(0, numFaces, 256),
		[&](const tbb::blocked_range<u_int> &r) {
		// One point map per worker chunk, reused across faces (it is rebuilt
		// for every face): avoids a heap allocation per face
		std::vector<u_int> pointMap;  // Map points between local numeration and global one
		pointMap.reserve((N + 1) * (N + 2) / 2);
		for (u_int f = r.begin(); f < r.end(); ++f) {  // for each face
			const Triangle &ft = faces[f];
			const u_int faceVertices[3] = { ft.v[0], ft.v[1], ft.v[2] };

			// Side edge ids and orientation flips, read from the contiguous
			// faceEdges/edgeV0 arrays (no map lookup in this loop).
			// Side 0 = (v0,v1), 1 = (v1,v2), 2 = (v2,v0); pos is measured
			// from v0 on sides 0 and 2, from v1 on side 1.
			const u_int sideEdge[3] = {
				edges.faceEdges[3 * f + 0],
				edges.faceEdges[3 * f + 1],
				edges.faceEdges[3 * f + 2]
			};
			const bool sideFlip[3] = {
				edges.edgeV0[sideEdge[0]] != faceVertices[0],
				edges.edgeV0[sideEdge[1]] != faceVertices[1],
				edges.edgeV0[sideEdge[2]] != faceVertices[0]
			};

			// If (i,j) are local coordinates, and edge length is N,
			// we have in the triangle: i >= 0, j >= 0, i + j <= N
			// We then consider k so that i + j + k = N.
			// If we are on an edge, i * j * k == 0.
			// If we are on a vertex, i == N or j == N or k == N.
			// In the face (triangle), after tessellation, we'll have N² points.
			// Some of them will be shared with other faces, we'll have to deal
			// with that.

			pointMap.clear();

			for (int j = 0; j <= (int)N ; ++j) {
				for (int i = 0; i <= (int)N - j; ++i) {
					int k = (int)N - i - j;
					u_int vertex;  // Output
					// Find point type: vertex, edge point, interior point
					if (i == (int)N || j == (int)N || k == (int)N) {  // Vertex
						// Vertex point in initial triangle
						vertex = faceVertices[(i + 2 * j) / (int)N];
					} else if (i == 0 || j == 0 || k == 0) {  // Edge
						u_int edge;
						int pos;
						if (i == 0) {          // side 2, pos from v0
							edge = sideEdge[2];
							pos = j;
							if (sideFlip[2])
								pos = (int)N - pos;
						} else if (j == 0) {   // side 0, pos from v0
							edge = sideEdge[0];
							pos = i;
							if (sideFlip[0])
								pos = (int)N - pos;
						} else {               // k == 0: side 1, pos from v1
							edge = sideEdge[1];
							pos = j;
							if (sideFlip[1])
								pos = (int)N - pos;
						}
						vertex = numCoarseVertices + edge * numEdgeInteriorPoints + (pos - 1);

					} else { // Interior point
						size_t idx = offset
							+ (size_t)numTriangleInteriorPoints * f
							+ numTriangleInteriorPoints - ((size_t)((int)N - j) * ((int)N - j - 1) / 2)
							+ i - 1;
						// Create position
						tessCoords[idx] = LocalCoords(f, i, j, N);
						// Record point in map
						vertex = (u_int)idx;
					}

					pointMap.push_back(vertex);
				}  // ~for j
			}  // ~for i

			// Create triangles

			// Lambda to get a point in the pointMap, given (i, j)
			auto pnt = [N, &pointMap](int i, int j) {
				size_t idx = ((2 * (int)N + 3 - j) * j / 2) + i;
				return pointMap[idx];
			};

			size_t idxTri = (size_t)f * N * N;

			// Up triangles
			for (int j = 0; j < (int)N; ++j) {
				for (int i = 0; i < (int)N - j; ++i, ++idxTri) {
					tessTris[idxTri] = Triangle(pnt(i, j), pnt(i + 1, j), pnt(i, j + 1));
					if (cornerCoords) {
						(*cornerCoords)[3 * idxTri + 0] = LocalCoords(f, i, j, N);
						(*cornerCoords)[3 * idxTri + 1] = LocalCoords(f, i + 1, j, N);
						(*cornerCoords)[3 * idxTri + 2] = LocalCoords(f, i, j + 1, N);
					}
				}
			}
			// Down triangles
			for (int j = 0; j < (int)N; ++j) {
				for (int i = 0; i < (int)N - j - 1; ++i, ++idxTri) {
					tessTris[idxTri] = Triangle(pnt(i, j + 1), pnt(i + 1, j), pnt(i + 1, j + 1));
					if (cornerCoords) {
						(*cornerCoords)[3 * idxTri + 0] = LocalCoords(f, i, j + 1, N);
						(*cornerCoords)[3 * idxTri + 1] = LocalCoords(f, i + 1, j, N);
						(*cornerCoords)[3 * idxTri + 2] = LocalCoords(f, i + 1, j + 1, N);
					}
				}
			}

	}  // ~for face
	});

	return std::make_tuple(std::move(tessCoords), std::move(tessTris), numPoints, numTriangles);

}

// Collect the present layers of a mesh prop of the source mesh: allocates
// the destination layer and records the (src, dst) pointer pair used by
// the interpolation loops. `allocated` keeps the new layers alive until
// they are attached to the output mesh.
template <typename T>
void CollectLayers(
	const ExtMeshProp<T> &srcProp,
	const size_t count,
	std::vector<std::pair<const T *, T *>> &pairs,
	std::vector<std::pair<u_int, std::shared_ptr<T[]>>> &allocated
) {
	for (u_int l = 0; l < EXTMESH_MAX_DATA_COUNT; ++l) {
		if (!srcProp.LayerHasValues(l))
			continue;
		auto dst = std::make_shared<T[]>(count);
		pairs.emplace_back(srcProp[l].get(), dst.get());
		allocated.emplace_back(l, std::move(dst));
	}
}

}  // ~namespace (local)

// Get number of positions after N-tessellation
u_int GetTessellationPosCount(const ExtTriangleMesh &srcMesh, int N) {
	const EdgeTable edges(srcMesh.GetTotalVertexCount(), srcMesh.GetTriangles());
	return PosCount(CheckedCount(srcMesh.GetTotalVertexCount()), edges.numEdges,
		CheckedCount(srcMesh.GetTotalTriangleCount()), N);
}

// Get number of triangles after N-tessellation
u_int GetTessellationTriangleCount(const ExtTriangleMesh &srcMesh, int N) {
	return TriCount(CheckedCount(srcMesh.GetTotalTriangleCount()), N);
}

// Subdivision API flavor: raw tessellation data (coords + triangles); the
// subdivision shape evaluates limit-surface positions from tessCoords
TessellationData TessellateTopology(
	const ExtTriangleMesh &srcMesh,
	const size_t N,
	std::vector<LocalCoords> *cornerCoords
) {
	return TessellateTopologyData(
		CheckedCount(srcMesh.GetTotalVertexCount()),
		srcMesh.GetTriangles(),
		N,
		cornerCoords
	);
}

// ApplyTessellate (flat) API flavor: mesh in, tessellated mesh out, with
// barycentrically interpolated positions and mesh data
luxrays::ShapeUPtr TessellateTopology(
	const ExtTriangleMesh &srcMesh,
	const size_t N,
	const bool interpolatedNormals
) {
	const auto faces = srcMesh.GetTriangles();

	CoordVector coords;
	TriangleBuffer tris;
	u_int numPoints, numTriangles;
	std::tie(coords, tris, numPoints, numTriangles) = TessellateTopologyData(
		CheckedCount(srcMesh.GetTotalVertexCount()), faces,
		N, nullptr);

	// Output vertex data: positions and all per-vertex props (normals, UVs,
	// colors, alphas, vertex AOVs) are interpolated barycentrically inside
	// the base triangles (local coords convention: face vertex 0 -> (0,0),
	// 1 -> (1,0), 2 -> (0,1)). Normals are normalized after interpolation,
	// unless interpolatedNormals is false: then the output mesh has no
	// vertex normals and shades flat via the triangle normals.
	const Points verts = srcMesh.GetVertices();
	VertexBuffer points(numPoints);
	NormalBuffer normals;
	if (interpolatedNormals && srcMesh.HasNormals())
		normals.Allocate(numPoints);
	const NormalBuffer &srcNormals = srcMesh.GetNormals();

	std::vector<std::pair<const UV *, UV *>> uvPairs;
	std::vector<std::pair<u_int, std::shared_ptr<UV[]>>> uvLayers;
	CollectLayers(srcMesh.GetAllUVs(), numPoints, uvPairs, uvLayers);
	std::vector<std::pair<const Spectrum *, Spectrum *>> colPairs;
	std::vector<std::pair<u_int, std::shared_ptr<Spectrum[]>>> colLayers;
	CollectLayers(srcMesh.GetAllColors(), numPoints, colPairs, colLayers);
	std::vector<std::pair<const float *, float *>> alphaPairs;
	std::vector<std::pair<u_int, std::shared_ptr<float[]>>> alphaLayers;
	CollectLayers(srcMesh.GetAllAlphas(), numPoints, alphaPairs, alphaLayers);
	std::vector<std::pair<const float *, float *>> vertAOVPairs;
	std::vector<std::pair<u_int, std::shared_ptr<float[]>>> vertAOVLayers;
	CollectLayers(srcMesh.vertAOV, numPoints, vertAOVPairs, vertAOVLayers);

	tbb::parallel_for(tbb::blocked_range<u_int>(0, numPoints, 4096),
		[&](const tbb::blocked_range<u_int> &r) {
		for (u_int i = r.begin(); i < r.end(); ++i) {
			const LocalCoords &c = coords[i];
			const Triangle &ft = faces[c.face];
			const u_int v0 = ft.v[0], v1 = ft.v[1], v2 = ft.v[2];
			const float b0 = 1.f - c.x - c.y;
			points[i] = b0 * verts[v0] + c.x * verts[v1] + c.y * verts[v2];
			if (normals) {
				normals[i] = Normalize(
					srcNormals[v0] * b0 + srcNormals[v1] * c.x + srcNormals[v2] * c.y);
			}
			for (const auto &p : uvPairs)
				p.second[i] = p.first[v0] * b0 + p.first[v1] * c.x + p.first[v2] * c.y;
			for (const auto &p : colPairs)
				p.second[i] = p.first[v0] * b0 + p.first[v1] * c.x + p.first[v2] * c.y;
			for (const auto &p : alphaPairs)
				p.second[i] = p.first[v0] * b0 + p.first[v1] * c.x + p.first[v2] * c.y;
			for (const auto &p : vertAOVPairs)
				p.second[i] = p.first[v0] * b0 + p.first[v1] * c.x + p.first[v2] * c.y;
		}
	});

	// Per-triangle data: each tessellated triangle lies inside one source
	// face (source face = t / (N*N)), so triangle AOV layers are broadcast
	// unchanged. Triangle normals need no work here: Preprocess() recomputes
	// them from the tessellated (flat) geometry.
	std::vector<std::pair<const float *, float *>> triAOVPairs;
	std::vector<std::pair<u_int, std::shared_ptr<float[]>>> triAOVLayers;
	CollectLayers(srcMesh.triAOV, numTriangles, triAOVPairs, triAOVLayers);
	if (!triAOVPairs.empty()) {
		const size_t NN = N * N;
		tbb::parallel_for(tbb::blocked_range<u_int>(0, numTriangles, 4096),
			[&](const tbb::blocked_range<u_int> &r) {
			for (u_int t = r.begin(); t < r.end(); ++t) {
				const size_t srcFace = t / NN;
				for (const auto &p : triAOVPairs)
					p.second[t] = p.first[srcFace];
			}
		});
	}

	auto shape = std::make_unique<luxrays::Shape>(srcMesh.GetName(), std::move(points));
	auto mesh = std::make_unique<ExtTriangleMesh>(
		*shape, std::move(tris), std::move(normals),
		nullptr, nullptr, nullptr, srcMesh.GetBevelRadius()
	);
	for (const auto &p : uvLayers)
		mesh->SetUVs(p.first, p.second, numPoints);
	for (const auto &p : colLayers)
		mesh->SetColors(p.first, p.second, numPoints);
	for (const auto &p : alphaLayers)
		mesh->SetAlphas(p.first, p.second, numPoints);
	for (const auto &p : vertAOVLayers)
		mesh->SetVertexAOV(p.first, p.second, numPoints);
	for (const auto &p : triAOVLayers)
		mesh->SetTriAOV(p.first, p.second, numTriangles);
	shape->AddSubmesh(std::move(mesh));
	return shape;
}

}  // ~namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
