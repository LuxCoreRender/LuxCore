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
#include "slg/shapes/subdiv_corner_data.h"
#include "luxrays/core/exttrianglemesh.h"

namespace slg {

// Runtime cache of subdivided envelopes: ONE subdivision run per subdiv
// parameter set, shared by all the per-material-slot (matslot) shapes
// subdividing the same source Shape, which then resplit the result per slot.
// It replaces the old per-Shape cache (Shape::SubdivEnvelopeCache). The cache
// is owned by a static member of SubdivShape (see subdiv.h) so the subdiv run
// survives across the several SubdivShape instances of one source mesh.
class SubdivEnvelopeCache {
public:
	// One cached subdivided envelope over the envelope of a source Shape: the
	// full subdivided mesh, the base-face index of each triangle (to resplit
	// per source submesh), and the per-corner varying data of the subdivided
	// mesh (so seams between submeshes are preserved through subdivision).
	struct Entry {
		luxrays::ExtTriangleMeshSPtr mesh;  // full subdivided envelope
		std::vector<u_int> triBaseFace;
		SubdivCornerData corner;  // per-corner varying data of the subdivided mesh
	};

	std::mutex &GetMutex() { return mtx; }
	std::unordered_map<std::string, Entry> &GetCache() { return cache; }
	// Clear all entries (called when a mesh is (re)defined, see
	// Scene::ClearSubdivEnvelopeCache).
	void Clear() {
		std::lock_guard<std::mutex> lock(mtx);
		cache.clear();
	}

private:
	mutable std::mutex mtx;
	std::unordered_map<std::string, Entry> cache;
};

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
//
// When cornerData is given, it is filled with the per-corner (face-varying)
// UV / color / alpha values of the envelope: each envelope face takes the
// values of the submesh it belongs to, so seams between submeshes are
// preserved through subdivision.
luxrays::ExtTriangleMeshUPtr BuildSubdivEnvelope(
	SceneConstRef scene,
	const luxrays::ExtTriangleMesh &srcMesh,
	SubdivCornerData *cornerData = nullptr
);

// Same envelope, built from an explicit list of meshes sharing one vertex
// buffer (in the given order), e.g. the submeshes of a Shape. Used when the
// sibling meshes are already known (no scene lookup needed).
luxrays::ExtTriangleMeshUPtr BuildEnvelopeFromMeshes(
	const std::vector<const luxrays::ExtTriangleMesh *> &siblings,
	SubdivCornerData *cornerData = nullptr
);

// Given the base face index of each triangle of a subdivided mesh
// (triBaseFace[tri]), write the material slot of the corresponding envelope
// face into the subdivided mesh's triangle AOV layer MATERIAL_TRI_AOV_INDEX.
void PropagateMaterialSlots(
	const luxrays::ExtTriangleMesh &envelope,
	const std::vector<u_int> &triBaseFace,
	luxrays::ExtTriangleMesh &subdivided
);

// Keep only the triangles of one slot (triangle AOV layer
// MATERIAL_TRI_AOV_INDEX; one slot per source submesh). The vertex buffer is
// shared, not copied. When corner data of the subdivided mesh is given, the
// per-point varying arrays are rebuilt from it, so each output submesh gets
// the values of its own corners at shared (seam) points.
luxrays::ExtTriangleMeshUPtr FilterByMaterialSlot(
	const luxrays::ExtTriangleMesh &mesh,
	const int matSlot,
	const SubdivCornerData *corner = nullptr
);

// Same resplit, but keeping the triangles whose base (envelope) face is in
// [face0, face1): one output submesh per source submesh of a Shape.
luxrays::ExtTriangleMeshUPtr FilterByFaceRange(
	const luxrays::ExtTriangleMesh &mesh,
	const std::vector<u_int> &triBaseFace,
	const size_t face0,
	const size_t face1,
	const SubdivCornerData *corner = nullptr
);

}

#endif	/* _SLG_SUBDIV_ENVELOPE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
