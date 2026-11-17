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

#include "slg/shapes/tessellate_core.h"
#include "slg/scene/scene.h"

using namespace luxrays;

namespace slg {

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
	// Flat (ApplyTessellate) API flavor: mesh in, tessellated mesh out
	return TessellateTopology(srcMesh, rate);
}

}  // ~namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
