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

#include "slg/shapes/tessellate_core.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <stdexcept>

#include <oneapi/tbb.h>

#include "luxrays/core/exttrianglemesh.h"
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
	std::map<std::pair<u_int, u_int>, u_int> edgeIndex;  // sorted vertex pair -> edge

	EdgeTable(u_int numVertices, std::span<const Triangle> faces) {
		vertexFirstFace.assign(numVertices, NO_FACE);
		for (u_int f = 0; f < faces.size(); ++f) {
			for (u_int c = 0; c < 3; ++c) {
				const u_int a = faces[f].v[c];
				const u_int b = faces[f].v[(c + 1) % 3];
				if (vertexFirstFace[a] == NO_FACE)
					vertexFirstFace[a] = f;
				const auto key = std::minmax(a, b);
				const auto it = edgeIndex.find(key);
				if (it != edgeIndex.end())
					continue;
				edgeIndex[key] = numEdges++;
				edgeV0.push_back(a);
				edgeV1.push_back(b);
				edgeFirstFace.push_back(f);
			}
		}
	}

	u_int FindEdge(u_int v0, u_int v1) const {
		return edgeIndex.at(std::minmax(v0, v1));
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
		for (u_int f = r.begin(); f < r.end(); ++f) {  // for each face
			const Triangle &ft = faces[f];
			const u_int faceVertices[3] = { ft.v[0], ft.v[1], ft.v[2] };

			// If (i,j) are local coordinates, and edge length is N,
			// we have in the triangle: i >= 0, j >= 0, i + j <= N
			// We then consider k so that i + j + k = N.
			// If we are on an edge, i * j * k == 0.
			// If we are on a vertex, i == N or j == N or k == N.
			// In the face (triangle), after tessellation, we'll have N² points.
			// Some of them will be shared with other faces, we'll have to deal
			// with that.

			std::vector<u_int> pointMap;  // Map points between local numeration and global one

			for (int j = 0; j <= (int)N ; ++j) {
				for (int i = 0; i <= (int)N - j; ++i) {
					int k = (int)N - i - j;
					u_int vertex;  // Output
					// Find point type: vertex, edge point, interior point
					if (i == (int)N || j == (int)N || k == (int)N) {  // Vertex
						// Vertex point in initial triangle
						vertex = faceVertices[(i + 2 * j) / (int)N];
					} else if (i == 0 || j == 0 || k == 0) {  // Edge
						// Find edge vertices
						u_int v0, v1;
						int pos;
						// Find edge
						if (i == 0) {
							v0 = faceVertices[0];
							v1 = faceVertices[2];
							pos = j;
						} else if (j == 0) {
							v0 = faceVertices[0];
							v1 = faceVertices[1];
							pos = i;
						} else if (k == 0) {
							v0 = faceVertices[1];
							v1 = faceVertices[2];
							pos = j;
						}
						u_int edge = edges.FindEdge(v0, v1);
						if (v0 != edges.edgeV0[edge]) {
							pos = (int)N - pos;
							std::swap(v0, v1);
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
// barycentrically interpolated positions
luxrays::ExtTriangleMeshUPtr TessellateTopology(
	const ExtTriangleMesh &srcMesh,
	const size_t N
) {
	const auto faces = srcMesh.GetTriangles();

	CoordVector coords;
	TriangleBuffer tris;
	u_int numPoints, numTriangles;
	std::tie(coords, tris, numPoints, numTriangles) = TessellateTopologyData(
		CheckedCount(srcMesh.GetTotalVertexCount()), faces,
		N, nullptr);

	// 3D positions: barycentric interpolation inside the base triangles
	// (local coords convention: face vertex 0 -> (0,0), 1 -> (1,0), 2 -> (0,1))
	const Points verts = srcMesh.GetVertices();
	VertexBuffer points(numPoints);
	tbb::parallel_for(tbb::blocked_range<u_int>(0, numPoints, 4096),
		[&](const tbb::blocked_range<u_int> &r) {
		for (u_int i = r.begin(); i < r.end(); ++i) {
			const auto &c = coords[i];
			const Triangle &ft = faces[c.face];
			const Point &p0 = verts[ft.v[0]];
			const Point &p1 = verts[ft.v[1]];
			const Point &p2 = verts[ft.v[2]];
			const float b0 = 1.f - c.x - c.y;
			points[i] = b0 * p0 + c.x * p1 + c.y * p2;
		}
	});

	return std::make_unique<ExtTriangleMesh>(
		std::move(points), std::move(tris), NormalBuffer()
	);
}

}  // ~namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
