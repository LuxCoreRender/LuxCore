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

#include <stdexcept>

#include "luxrays/utils/strutils.h"
#include "slg/shapes/tessellate_core.h"
#include "slg/scene/scene.h"

using namespace luxrays;

namespace slg {

//------------------------------------------------------------------------------
// TessellateShape: standalone flat tessellation of a mesh
//------------------------------------------------------------------------------

TessellateShape::TessellateShape(
	ExtTriangleMeshRef srcMesh,
	const u_int rate,
	const bool interpolatedNormals
) {
	const double startTime = WallClockTime();

	if (rate > 0)
		mesh = ApplyTessellate(srcMesh, rate, interpolatedNormals);
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
	const u_int rate,
	const bool interpolatedNormals
) {
	// N = 2^rate segments per edge, same convention as the subdivision
	// levels (subdiv.cpp uses 1 << maxLevel). The core works in int
	// internally, so N must stay well below INT_MAX.
	if (rate > 30)
		throw std::runtime_error(
			"Tessellate rate too large (max 30): " + ToString(rate));
	const size_t N = (size_t)1 << rate;

	// Flat (ApplyTessellate) API flavor: mesh in, tessellated mesh out
	return TessellateTopology(srcMesh, N, interpolatedNormals);
}

}  // ~namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
