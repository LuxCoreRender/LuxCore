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

#ifndef _SLG_SUBDIV_ENVELOPE_H
#define	_SLG_SUBDIV_ENVELOPE_H

#include <vector>

#include "slg/usings.h"
#include "slg/shapes/shape.h"
#include "luxrays/core/exttrianglemesh.h"

namespace slg {

// Reserved triangle AOV layer carrying the material slot index of each face
// (written by BlendLuxCore at mesh export, one float per triangle).
constexpr u_int MATERIAL_TRI_AOV_INDEX = 2;

// Build the "envelope" of a mesh: the union of the triangles of ALL scene
// meshes that share srcMesh's vertex buffer (i.e. the submeshes of one source
// mesh, exported sharing one welded point array). Subdividing the envelope
// keeps shared edges interior (watertight), instead of refining each submesh
// as an open patch (cracks/warping).
//
// The per-face material slot of each submesh triangle (triangle AOV layer
// MATERIAL_TRI_AOV_INDEX) is carried over to the envelope, so the subdivided
// result can be split back per material.
//
// Returns nullptr when srcMesh shares its vertex buffer with no other mesh
// (no envelope needed: subdivide srcMesh directly).
luxrays::ExtTriangleMeshUPtr BuildSubdivEnvelope(
	SceneConstRef scene,
	const luxrays::ExtTriangleMesh &srcMesh
);

// Given the base face index of each triangle of a subdivided mesh
// (triBaseFace[tri]), write the material slot of the corresponding envelope
// face into the subdivided mesh's triangle AOV layer MATERIAL_TRI_AOV_INDEX.
void PropagateMaterialSlots(
	const luxrays::ExtTriangleMesh &envelope,
	const std::vector<u_int> &triBaseFace,
	luxrays::ExtTriangleMesh &subdivided
);

// Keep only the triangles of one material slot (triangle AOV layer
// MATERIAL_TRI_AOV_INDEX). The vertex buffer is shared, not copied.
luxrays::ExtTriangleMeshUPtr FilterByMaterialSlot(
	const luxrays::ExtTriangleMesh &mesh,
	const int matSlot
);

}

#endif	/* _SLG_SUBDIV_ENVELOPE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
