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

#include "slg/shapes/tessellate.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>

#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/topologyRefinerFactory.h>
#include <opensubdiv/sdc/options.h>

#include <omp.h>

#include "luxrays/core/exttrianglemesh.h"
#include "slg/scene/scene.h"
#include "slg/shapes/subdiv.h"

using namespace luxrays;
using namespace OpenSubdiv;

namespace slg {

// Free-function API (used directly by the subdivision shape)

// Get number of positions after N-tessellation
int GetTessellationPosCount(const Far::TopologyLevel &topology, int N) {

	// Number of interior points in an edge
	const int numEdgeInteriorPoints = N - 1;

	// Number of interior points in a triangle
	const int numTriangleInteriorPoints = (N - 1) * (N - 2) / 2;

	// Number of coords to be computed
	const int numCoords =
		topology.GetNumVertices()
		+ numEdgeInteriorPoints * topology.GetNumEdges()
		+ numTriangleInteriorPoints * topology.GetNumFaces();

	return numCoords;
}

// Get number of triangles after N-tessellation
int GetTessellationTriangleCount(const Far::TopologyLevel &topology, int N) {
	return topology.GetNumFaces() * N * N;
}

///

	/// Tessellate input mesh
	///
	/// Apply a triforce tessellation to input mesh.
	///
	/// This tessellation splits edges in N segments,
	/// and generates N² resulting triangles by input triangle.
	/// It's been reimplemented here in order to use multithreading (osd
	/// implementation is monothreaded...).
	///
	/// Input:
	///           V2
	///          / \
	///         /  \
	///        /   \
	///      V0----V1
	///
	///
	///	Output (N=4):
	///
	///          V2
	///         / \
	///        E7-E8
	///       / \/ \
	///      E5-I3-E6
	///     / \/ \/ \
	///    E3-I0-I1-E4
	///   / \/ \/ \/ \
	///	V0-E0-E1-E2-V1
	///
	///	Nota:
	///	Vx - initial vertex (to be kept)
	///	Ex - edge vertex (to be created)
	///	Ix - internal vertex (to be created)
	///
	/// @param N Tessellation rate: each edge will be subdivided into N
	/// sub-edges
	///
std::tuple<CoordVector, luxrays::TriangleBuffer, int, int>
TessellateTopology(const Far::TopologyRefiner &refiner, const size_t N) {
	// This is triforce tessellation.

	// Allocate outputs
	const auto& topology = refiner.GetLevel(0);
	CoordVector tessCoords;
	int numTriangles = GetTessellationTriangleCount(topology, N);
	int numPoints = GetTessellationPosCount(topology, N);
	TriangleBuffer tessTris(numTriangles);

	// Some constants
	const size_t FACE_COUNT = topology.GetNumFaces();
	const int FACE_SIZE = 3;  // Of course (triangles)...

	// Check
	SDL_LOG(
		"Subdivision (enhanced) - Tessellating with factor N = "
		<< N
	);
	if (FACE_COUNT * N * N >= std::numeric_limits<int>::max()) {
		throw std::runtime_error(
			"FACE_COUNT * N * N > numeric limit ("
			+ ToString(std::numeric_limits<int>::max())
			+ ")"
		);
	}

	// Useful variables and constants
	int offset;

	// Number of coarse vertices
	const int numCoarseVertices = topology.GetNumVertices();

	// Number of interior points in an edge
	const int numEdgeInteriorPoints = N - 1;

	// Number of interior points in a triangle
	const int numTriangleInteriorPoints = (N - 1) * (N - 2) / 2;

	// Number of coords to be computed
	const int numCoords =
		topology.GetNumVertices()
		+ numEdgeInteriorPoints * topology.GetNumEdges()
		+ numTriangleInteriorPoints * topology.GetNumFaces();

	// Size tessCoords
	tessCoords.resize(numCoords);

	// Get vertex local coordinates in given face
	auto getVertexLocalCoords = [&topology](int face, int vertex) {
		auto faceVertices = topology.GetFaceVertices(face);
		if (vertex == faceVertices[0]) {
			return LocalCoords(face, 0.f, 0.f);
		} else if (vertex == faceVertices[1]) {
			return LocalCoords(face, 1.f, 0.f);
		} else if (vertex == faceVertices[2]) {
			return LocalCoords(face, 0.f, 1.f);
		}

		throw std::runtime_error("Error in getVertexLocalCoords");
	};

	// Step #1 - Initialize with initial (coarse) vertices
	#pragma omp parallel for
	for (int vertex = 0; vertex < numCoarseVertices; ++vertex) {
		auto vertexFaces = topology.GetVertexFaces(vertex);
		if (vertexFaces.size() > 0) {
			// Choose first face (arbitrary)
			int face = vertexFaces[0];
			tessCoords[vertex] = getVertexLocalCoords(face, vertex);
		} else {
			// Isolated vertex (not referenced by any face) — assign
			// a default coordinate; it won't appear in any triangle
			tessCoords[vertex] = LocalCoords(0, 0.f, 0.f);
		}
	}
	offset = numCoarseVertices;

	// Step #2 - Add edge vertices
	#pragma omp parallel for
	for (int edge = 0; edge < topology.GetNumEdges(); ++edge) {
		// Find face
		int face = topology.GetEdgeFaces(edge)[0];
		// Find edge vertices.
		auto edgeVerts = topology.GetEdgeVertices(edge);
		int v0 = edgeVerts[0];
		int v1 = edgeVerts[1];

		LocalCoords c0 = getVertexLocalCoords(face, v0);
		LocalCoords c1 = getVertexLocalCoords(face, v1);

		for (int i = 1; i < N; ++i) {  // Only edge interior vertices, not extremities
			LocalCoords coords(c0, c1, i, N);  // Interpolate from c0 and c1
			tessCoords[offset + edge * numEdgeInteriorPoints + i - 1 ] = coords;
		}
	}
	offset = numCoarseVertices + numEdgeInteriorPoints * topology.GetNumEdges();

	// Step #3 - Add interior points
	#pragma omp parallel for
	for (int f = 0; f < FACE_COUNT; ++f) {  // for each face
		const auto faceVertices = topology.GetFaceVertices(f);
		const auto faceEdges = topology.GetFaceEdges(f);
		assert(faceVertices.size() == 3);

		// If (i,j) are local coordinates, and edge length is N,
		// we have in the triangle: i >= 0, j >= 0, i + j <= N
		// We then consider k so that i + j + k = N.
		// If we are on an edge, i * j * k == 0.
		// If we are on a vertex, i == N or j == N or k == N.
		// In the face (triangle), after tessellation, we'll have N² points.
		// Some of them will be shared with other faces, we'll have to deal
		// with that.

		std::vector<int> pointMap;  // Map points between local numeration and global one

		for (int j = 0; j <= N ; ++j) {
			for (int i = 0; i <= N - j; ++i) {
				int k = N - i - j;
				int vertex;  // Output
				// Find point type: vertex, edge point, interior point
				if (i == N || j == N || k == N) {  // Vertex
					// Vertex point in initial triangle
					vertex = faceVertices[(i + 2 * j) / N];
				} else if (i == 0 || j == 0 || k == 0) {  // Edge
					// Find edge vertices
					int v0, v1, pos;
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
					int edge = topology.FindEdge(v0, v1);
					auto edgeVertices = topology.GetEdgeVertices(edge);
					if (v0 != edgeVertices[0]) {
						pos = N - pos;
						std::swap(v0, v1);
					}
					vertex = numCoarseVertices + edge * numEdgeInteriorPoints + (pos - 1);

				} else { // Interior point
					int idx = offset
						+ numTriangleInteriorPoints * f
						+ numTriangleInteriorPoints - (N - j) * (N - j - 1) / 2
						+ i - 1;
					// Create position
					tessCoords[idx] = LocalCoords(f, i, j, N);
					// Record point in map
					vertex = idx;
				}

				pointMap.push_back(vertex);
			}  // ~for j
		}  // ~for i

		// Create triangles

		// Lambda to get a point in the pointMap, given (i, j)
		auto pnt = [N, &pointMap](int i, int j) {
			int idx = (2 * N + 3 - j) * j / 2 + i;
			return pointMap[idx];
		};

		int idxTri = f * N * N;

		// Up triangles
		for (int j = 0; j < N; ++j) {
			for (int i = 0; i < N - j; ++i, ++idxTri) {
				tessTris[idxTri] = Triangle(pnt(i, j), pnt(i + 1, j), pnt(i, j + 1));
				//tessTris[idxTri] = Triangle(pnt(i, j), pnt(i, j +1), pnt(i+1, j));
			}
		}
		// Down triangles
		for (int j = 0; j < N; ++j) {
			for (int i = 0; i < N - j - 1; ++i, ++idxTri) {
				tessTris[idxTri] = Triangle(pnt(i, j + 1), pnt(i + 1, j), pnt(i + 1, j + 1));
				//tessTris[idxTri] = Triangle(pnt(i, j + 1), pnt(i + 1, j + 1), pnt(i + 1, j));
			}
		}

	}  // ~for face

	return std::make_tuple(std::move(tessCoords), std::move(tessTris), numPoints, numTriangles);

}


//------------------------------------------------------------------------------
// TessellateShape: standalone flat tessellation of a mesh
//------------------------------------------------------------------------------

TessellateShape::TessellateShape(
	ExtTriangleMeshRef srcMesh,
	const u_int rate
) {
	const double startTime = WallClockTime();

	if (rate > 0)
		mesh = ApplyTessellate(srcMesh, rate);
	else
		mesh = srcMesh.Copy();

	SDL_LOG("Tessellated shape from " << srcMesh.GetTotalTriangleCount()
		<< " to " << mesh->GetTotalTriangleCount() << " faces in "
		<< WallClockTime() - startTime << " secs");
}

TessellateShape::~TessellateShape() {
}

ExtTriangleMeshUPtr TessellateShape::RefineImpl(SceneConstRef scene) {
	return std::move(mesh);
}

ExtTriangleMeshUPtr TessellateShape::ApplyTessellate(
	ExtTriangleMeshRef srcMesh,
	const u_int rate
) {
	// Build a plain (no crease) uniform refiner over the source mesh
	Far::TopologyDescriptor desc;
	desc.numVertices = srcMesh.GetTotalVertexCount();
	desc.numFaces = srcMesh.GetTotalTriangleCount();
	std::vector<int> vertPerFace(desc.numFaces, 3);
	desc.numVertsPerFace = vertPerFace.data();
	desc.vertIndicesPerFace = reinterpret_cast<const int *>(srcMesh.GetTrianglesAsInts().data());

	using RefinerFactory = Far::TopologyRefinerFactory<Far::TopologyDescriptor>;
	std::unique_ptr<Far::TopologyRefiner> refiner(RefinerFactory::Create(
		desc,
		RefinerFactory::Options(Sdc::SCHEME_LOOP, Sdc::Options())
	));

	// Tessellate the base level
	CoordVector coords;
	TriangleBuffer tris;
	int numPoints, numTriangles;
	std::tie(coords, tris, numPoints, numTriangles) =
		TessellateTopology(*refiner, rate);

	// 3D positions: barycentric interpolation inside the base triangles
	// (local coords convention: face vertex 0 -> (0,0), 1 -> (1,0), 2 -> (0,1))
	const auto &topology = refiner->GetLevel(0);
	const Points verts = srcMesh.GetVertices();
	VertexBuffer points(numPoints);
	for (int i = 0; i < numPoints; ++i) {
		const auto &c = coords[i];
		const auto faceVerts = topology.GetFaceVertices(c.face);
		const Point &p0 = verts[faceVerts[0]];
		const Point &p1 = verts[faceVerts[1]];
		const Point &p2 = verts[faceVerts[2]];
		const float b0 = 1.f - c.x - c.y;
		points[i] = b0 * p0 + c.x * p1 + c.y * p2;
	}

	return std::make_unique<ExtTriangleMesh>(
		std::move(points), std::move(tris), NormalBuffer()
	);
}

}  // ~namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
