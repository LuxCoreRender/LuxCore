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

#ifndef _SLG_TESSELLATE_CORE_H
#define	_SLG_TESSELLATE_CORE_H

#include <cassert>
#include <cstddef>
#include <tuple>
#include <vector>

#include "slg/usings.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/utils/buffer.h"

namespace slg {

// Core triforce tessellation algorithm. Independent from both the
// subdivision shape and the tessellate shape modifier, and from OpenSubdiv:
// it works on a plain ExtTriangleMesh. See subdiv_tessellate.h and
// tessellate.h for the users of this API.

// Storage of local coordinates (coordinates in a given face) during
// tessellation, in the form (face, x, y). Indices use the LuxCore u_int
// type; conversion to the int type OpenSubdiv expects happens in the
// subdivision adapter (subdiv_tessellate.h).
struct LocalCoords {
	u_int face = static_cast<u_int>(-1);  // no face
	float x = 0.f;
	float y = 0.f;

	LocalCoords() {}

	LocalCoords(u_int p_face, float p_x, float p_y):
		face(p_face),
		x(p_x),
		y(p_y)
	{ }

	LocalCoords(u_int p_face, int p_i, int p_j, int N):
		face(p_face),
		x(float(p_i) / float(N)),
		y(float(p_j) / float(N))
	{ }

	// Avoid unexpected use of LocalCoords(int, float, float)
	// with implicit conversion
	LocalCoords(int, int, int) = delete;

	// Interpolating constructor
	LocalCoords(LocalCoords p0, LocalCoords p1, u_int weight, u_int total) {
		assert(p0.face == p1.face);
		assert(weight <= total);

		face = p0.face;

		float weight_f = float(weight);
		float total_f = float(total);
		x = (weight_f * p1.x + (total_f - weight_f) * p0.x) / total_f;
		y = (weight_f * p1.y + (total_f - weight_f) * p0.y) / total_f;
	}
};

typedef std::vector<LocalCoords> CoordVector;

// Raw result of a tessellation: (per-point local coords, triangles, numPoints,
// numTriangles). This is the subdivision API flavor of TessellateTopology.
using TessellationData = std::tuple<CoordVector, luxrays::TriangleBuffer, u_int, u_int>;

// Triforce tessellation of a triangle mesh: splits each edge in N segments
// and generates N^2 triangles per input triangle. Multithreaded
// reimplementation of the OSD tessellation (the OSD one is monothreaded).
//
// Two overloads, one per API flavor:
//
//  - TessellateTopology(mesh, N, interpolatedNormals): the ApplyTessellate
//    (tessellate shape) API — returns the tessellated mesh with
//    barycentrically interpolated (flat) positions and all the per-vertex
//    and per-triangle mesh data of the source: UVs, colors, alphas and
//    vertex AOVs are interpolated like the positions, triangle AOVs are
//    broadcast from their source face (each tessellated triangle lies
//    inside one source face), triangle normals are recomputed by
//    ExtTriangleMesh::Preprocess() (tessellated triangles are coplanar
//    with their source face). Vertex normals are interpolated and
//    normalized only if interpolatedNormals is true; otherwise the output
//    mesh has no vertex normals and shades flat via the triangle normals.
//  - TessellateTopology(mesh, N, cornerCoords): the subdivision API —
//    returns the raw tessellation (coords + triangles); positions are NOT
//    evaluated, the subdivision shape evaluates limit-surface positions
//    itself from the returned tessCoords. No mesh data is evaluated on
//    this path (subdivision computes its own normals).
//
// @param srcMesh The mesh to tessellate
// @param N Tessellation rate: each edge is subdivided into N sub-edges
// @param interpolatedNormals (flat overload) interpolate+normalize the
//        source vertex normals onto the tessellated points (default true);
//        false keeps flat shading
// @param cornerCoords (subdivision overload) filled with the local
//        coordinates of each tessellated triangle corner WITHIN ITS OWN base
//        face (3 * numTriangles entries): the per-point coords of points
//        shared between faces carry the face of one arbitrary incident
//        face, which is not suitable for face-varying data evaluation.
luxrays::ExtTriangleMeshUPtr TessellateTopology(
	const luxrays::ExtTriangleMesh &srcMesh,
	const size_t N,
	const bool interpolatedNormals = true
);
TessellationData TessellateTopology(
	const luxrays::ExtTriangleMesh &srcMesh,
	const size_t N,
	std::vector<LocalCoords> *cornerCoords
);

// Number of positions / triangles after an N-tessellation of the mesh
u_int GetTessellationPosCount(const luxrays::ExtTriangleMesh &srcMesh, int N);
u_int GetTessellationTriangleCount(const luxrays::ExtTriangleMesh &srcMesh, int N);

}

#endif	/* _SLG_TESSELLATE_CORE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
