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

#include "slg/shapes/subdiv_tessellate.h"

#include <limits>
#include <stdexcept>

#include <oneapi/tbb.h>

#include "luxrays/utils/strutils.h"

namespace slg {

int ToOpenSubdivInt(u_int value) {
	if (value > (u_int)std::numeric_limits<int>::max())
		throw std::runtime_error(
			"OpenSubdiv index overflow: " + luxrays::ToString(value));
	return (int)value;
}

std::vector<int> ToOpenSubdivFaceIndices(const luxrays::ExtTriangleMesh &mesh) {
	const auto faces = mesh.GetTriangles();
	std::vector<int> out(3 * faces.size());
	tbb::parallel_for(tbb::blocked_range<size_t>(0, faces.size(), 4096),
		[&](const tbb::blocked_range<size_t> &r) {
		for (size_t f = r.begin(); f < r.end(); ++f)
			for (u_int c = 0; c < 3; ++c)
				out[3 * f + c] = ToOpenSubdivInt(faces[f].v[c]);
	});
	return out;
}

std::tuple<CoordVector, luxrays::TriangleBuffer, int, int>
TessellateSubdivTopology(
	const luxrays::ExtTriangleMesh &srcMesh,
	const size_t N,
	std::vector<LocalCoords> *cornerCoords
) {
	// The level-0 topology of the subdivision refiner is the source mesh
	// topology, so tessellate the source mesh directly with the subdivision
	// API flavor (raw coords + triangles; limit-surface positions are
	// evaluated by the subdivision shape from tessCoords)
	return TessellateTopology(srcMesh, N, cornerCoords);
}

u_int GetSubdivTessellationPosCount(const luxrays::ExtTriangleMesh &srcMesh, int N) {
	return GetTessellationPosCount(srcMesh, N);
}

u_int GetSubdivTessellationTriangleCount(const luxrays::ExtTriangleMesh &srcMesh, int N) {
	return GetTessellationTriangleCount(srcMesh, N);
}

}  // ~namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
