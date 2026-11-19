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

#include "slg/scene/shape.h"

#include "luxrays/core/exttrianglemesh.h"

using namespace luxrays;

namespace slg {

// Expand one per-vertex attribute layer from local (countVertices) rows to
// canonical (P) rows through the local->global vertex map
template <typename T>
static void ExpandLayer(
	const ExtMeshProp<T> &srcProp,
	ExtMeshProp<T> &dstProp,
	const std::vector<u_int> &map,
	const size_t P
) {
	for (u_int l = 0; l < EXTMESH_MAX_DATA_COUNT; ++l) {
		if (!srcProp.LayerHasValues(l))
			continue;
		const auto src = srcProp.GetLayer(l);
		const size_t n = srcProp.GetLayerSize();
		assert(n == map.size());
		auto dst = std::make_shared<T[]>(P);
		for (size_t i = 0; i < n; ++i)
			dst[map[i]] = src[i];
		dstProp.SetLayer(l, std::shared_ptr<T[]>(dst), P);
	}
}

ExtTriangleMeshUPtr MakeGlobalView(
	const ExtTriangleMesh &mesh,
	const VertexBuffer &canonical
) {
	if (!mesh.HasVertexMap())
		return nullptr;

	const std::vector<u_int> &map = mesh.GetVertexMap();
	const size_t n = map.size();
	const size_t P = canonical.Count();
	assert(mesh.GetTotalVertexCount() == n);

	// Triangles remapped to global (canonical) indices
	const auto srcTris = mesh.GetTriangles();
	TriangleBuffer tris(mesh.GetTotalTriangleCount());
	for (size_t i = 0; i < tris.Count(); ++i)
		for (u_int c = 0; c < 3; ++c)
			tris[i].v[c] = map[srcTris[i].v[c]];

	// Normals (rows not owned by this submesh stay zero, so the envelope's
	// first-non-zero merge across sibling views ignores them)
	NormalBuffer normals;
	if (mesh.HasNormals()) {
		const auto src = mesh.GetNormals();
		normals.Allocate(P);
		for (size_t i = 0; i < P; ++i)
			normals[i] = Normal(0.f, 0.f, 0.f);
		for (size_t i = 0; i < n; ++i)
			normals[map[i]] = src[i];
	}

	// Per-vertex varying data, expanded to the canonical point count
	ExtMeshProp<UV> uvs;
	ExtMeshProp<Spectrum> cols;
	ExtMeshProp<float> alphas;
	ExpandLayer<UV>(mesh.GetAllUVs(), uvs, map, P);
	ExpandLayer<Spectrum>(mesh.GetAllColors(), cols, map, P);
	ExpandLayer<float>(mesh.GetAllAlphas(), alphas, map, P);

	auto view = std::make_unique<ExtTriangleMesh>(
		canonical,  // sharing ctor: the canonical buffer storage is shared
		std::move(tris),
		std::move(normals),
		uvs, cols, alphas
	);
	view->SetName(mesh.GetName());
	view->SetShapeName(mesh.GetShapeName());

	// Vertex AOVs
	for (u_int l = 0; l < EXTMESH_MAX_DATA_COUNT; ++l) {
		if (mesh.HasVertexAOV(l)) {
			const auto src = mesh.GetVertexAOVs(l);
			auto dst = std::make_shared<float[]>(P);
			for (size_t i = 0; i < n; ++i)
				dst[map[i]] = src[i];
			view->SetVertexAOV(l, std::shared_ptr<float[]>(dst), P);
		}
	}
	// Triangle AOVs are per-triangle: same indexing, copy as-is
	for (u_int l = 0; l < EXTMESH_MAX_DATA_COUNT; ++l)
		if (mesh.HasTriAOV(l))
			view->SetTriAOV(l, mesh.GetTriAOVs(l), mesh.GetTotalTriangleCount());

	// Creases are stored in canonical (global) indices already
	if (mesh.HasCreases())
		view->SetCreases(std::vector<u_int>(mesh.GetCreasePairs()),
			std::vector<float>(mesh.GetCreaseWeights()));

	Transform t;
	mesh.GetLocal2World(0.f, t);
	view->SetLocal2World(t);

	return view;
}

}  // namespace slg
