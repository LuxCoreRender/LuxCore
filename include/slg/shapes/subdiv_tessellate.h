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

#ifndef _SLG_SUBDIV_TESSELLATE_H
#define	_SLG_SUBDIV_TESSELLATE_H

#include <vector>

#include "slg/shapes/tessellate_core.h"

namespace slg {

// Checked conversion of LuxCore (u_int) indices to the int type OpenSubdiv
// expects: throws on overflow instead of silently truncating.
int ToOpenSubdivInt(u_int value);

// Face index array of a mesh in the flat int layout OpenSubdiv expects
// (3 * numFaces entries, checked)
std::vector<int> ToOpenSubdivFaceIndices(const luxrays::ExtTriangleMesh &mesh);

// Adapter between the subdivision shape (subdiv.cpp) and the core
// tessellation algorithm (tessellate_core.h). The subdivision shape
// tessellates the base topology of its source mesh (identical to the level-0
// topology of its OpenSubdiv refiner), then evaluates limit-surface positions
// and normals from the returned local coordinates instead of the core's flat
// barycentric ones.

// Tessellate the base topology of a subdivided mesh.
// Returns (tessCoords, tessTris, numPoints, numTriangles); see
// TessellateTopology() for the cornerCoords semantics.
std::tuple<CoordVector, luxrays::TriangleBuffer, int, int>
TessellateSubdivTopology(
	const luxrays::ExtTriangleMesh &srcMesh,
	const size_t N,
	std::vector<LocalCoords> *cornerCoords = nullptr
);

// Number of positions / triangles after an N-tessellation of the mesh
u_int GetSubdivTessellationPosCount(const luxrays::ExtTriangleMesh &srcMesh, int N);
u_int GetSubdivTessellationTriangleCount(const luxrays::ExtTriangleMesh &srcMesh, int N);

}

#endif	/* _SLG_SUBDIV_TESSELLATE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
