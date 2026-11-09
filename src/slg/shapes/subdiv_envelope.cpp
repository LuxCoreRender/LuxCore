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

#include "slg/shapes/subdiv_envelope.h"

#include "slg/scene/scene.h"
#include "slg/scene/extmeshcache.h"

namespace slg {

using luxrays::ExtTriangleMesh;
using luxrays::ExtTriangleMeshUPtr;
using luxrays::TriangleBuffer;
using luxrays::NormalBuffer;

//------------------------------------------------------------------------------
// BuildSubdivEnvelope
//------------------------------------------------------------------------------

// Collect the meshes sharing the same vertex buffer storage as srcMesh
static std::vector<const ExtTriangleMesh *> CollectPointBufferSiblings(
	SceneConstRef scene,
	const ExtTriangleMesh &srcMesh
) {
	const void *srcStorage = srcMesh.GetVertexBuffer().Data();

	std::vector<const ExtTriangleMesh *> siblings;
	for (const auto &name : scene.GetExtMeshes().GetExtMeshNames()) {
		auto &mesh = scene.GetExtMeshes().GetExtMesh(name);
		auto *triMesh = dynamic_cast<const ExtTriangleMesh *>(&mesh);
		if (triMesh && triMesh->GetVertexBuffer().Data() == srcStorage)
			siblings.push_back(triMesh);
	}
	return siblings;
}

ExtTriangleMeshUPtr BuildSubdivEnvelope(
	SceneConstRef scene,
	const ExtTriangleMesh &srcMesh
) {
	const auto siblings = CollectPointBufferSiblings(scene, srcMesh);
	if (siblings.size() <= 1)
		return nullptr;  // no sharing: subdivide the source directly

	// Union of all triangles (they all index the same shared point array)
	size_t totalTris = 0;
	for (const auto *m : siblings)
		totalTris += m->GetTotalTriangleCount();

	TriangleBuffer tris(totalTris);
	auto matSlots = std::make_shared<float[]>(totalTris);
	size_t offset = 0;
	for (const auto *m : siblings) {
		const auto srcTris = m->GetTriangles();
		for (size_t i = 0; i < srcTris.size(); ++i)
			tris[offset + i] = srcTris[i];

		// Per-face material slot of this submesh (default 0)
		const float defaultSlot = 0.f;
		if (m->HasTriAOV(MATERIAL_TRI_AOV_INDEX)) {
			const auto layer = m->GetTriAOVs(MATERIAL_TRI_AOV_INDEX);
			for (size_t i = 0; i < srcTris.size(); ++i)
				matSlots[offset + i] = layer[i];
		} else {
			for (size_t i = 0; i < srcTris.size(); ++i)
				matSlots[offset + i] = defaultSlot;
		}
		offset += srcTris.size();
	}

	// Per-point varying data: each submesh only fills the points of its own
	// island (the rest is zero), so merge per point taking the first non-zero
	// value across siblings (at seams one side's value is kept). Zero normals
	// would otherwise produce NaN shading downstream.
	const size_t numPoints = srcMesh.GetTotalVertexCount();

	NormalBuffer normals;
	for (const auto *m : siblings) {
		if (!m->HasNormals())
			continue;
		const auto n = m->GetNormals();
		if (!normals) {
			normals.Allocate(numPoints);
			for (size_t i = 0; i < numPoints; ++i)
				normals[i] = n[i];
		} else {
			for (size_t i = 0; i < numPoints; ++i)
				if (normals[i].LengthSquared() == 0.f && n[i].LengthSquared() > 0.f)
					normals[i] = n[i];
		}
	}

	// Same first-non-zero merge for the UV / color / alpha layers
	luxrays::ExtMeshProp<luxrays::UV> uvs;
	luxrays::ExtMeshProp<luxrays::Spectrum> cols;
	luxrays::ExtMeshProp<float> alphas;
	for (u_int l = 0; l < EXTMESH_MAX_DATA_COUNT; ++l) {
		for (const auto *m : siblings) {
			if (m->HasUVs(l) && !uvs.LayerHasValues(l)) {
				const auto layer = m->GetAllUVs().GetLayer(l);
				auto merged = std::make_shared<luxrays::UV[]>(numPoints);
				for (size_t i = 0; i < numPoints; ++i)
					merged[i] = layer[i];
				uvs.SetLayer(l, std::shared_ptr<luxrays::UV[]>(merged), numPoints);
			}
			if (m->HasColors(l) && !cols.LayerHasValues(l)) {
				const auto layer = m->GetAllColors().GetLayer(l);
				auto merged = std::make_shared<luxrays::Spectrum[]>(numPoints);
				for (size_t i = 0; i < numPoints; ++i)
					merged[i] = layer[i];
				cols.SetLayer(l, std::shared_ptr<luxrays::Spectrum[]>(merged), numPoints);
			}
			if (m->HasAlphas(l) && !alphas.LayerHasValues(l)) {
				const auto layer = m->GetAllAlphas().GetLayer(l);
				auto merged = std::make_shared<float[]>(numPoints);
				for (size_t i = 0; i < numPoints; ++i)
					merged[i] = layer[i];
				alphas.SetLayer(l, std::shared_ptr<float[]>(merged), numPoints);
			}
		}
		// fill the points left at zero by the first sibling from the others
		for (const auto *m : siblings) {
			if (uvs.LayerHasValues(l) && m->HasUVs(l)) {
				const auto layer = m->GetAllUVs().GetLayer(l);
				auto cur = uvs.GetLayer(l);
				for (size_t i = 0; i < numPoints; ++i)
					if (cur[i].u == 0.f && cur[i].v == 0.f && (layer[i].u != 0.f || layer[i].v != 0.f))
						cur[i] = layer[i];
			}
			if (cols.LayerHasValues(l) && m->HasColors(l)) {
				const auto layer = m->GetAllColors().GetLayer(l);
				auto cur = cols.GetLayer(l);
				for (size_t i = 0; i < numPoints; ++i)
					if (cur[i].Black() && !layer[i].Black())
						cur[i] = layer[i];
			}
			if (alphas.LayerHasValues(l) && m->HasAlphas(l)) {
				const auto layer = m->GetAllAlphas().GetLayer(l);
				auto cur = alphas.GetLayer(l);
				for (size_t i = 0; i < numPoints; ++i)
					if (cur[i] == 0.f && layer[i] != 0.f)
						cur[i] = layer[i];
			}
		}
	}

	// Sharing ctor: the envelope uses the same vertex storage as the submeshes
	auto envelope = std::make_unique<ExtTriangleMesh>(
		srcMesh.GetVertexBuffer(),
		std::move(tris),
		std::move(normals),
		uvs, cols, alphas
	);
	envelope->SetTriAOV(MATERIAL_TRI_AOV_INDEX, matSlots, totalTris);
	envelope->SetName(srcMesh.GetName() + "_env");
	return envelope;
}

//------------------------------------------------------------------------------
// PropagateMaterialSlots
//------------------------------------------------------------------------------

void PropagateMaterialSlots(
	const ExtTriangleMesh &envelope,
	const std::vector<u_int> &triBaseFace,
	ExtTriangleMesh &subdivided
) {
	const auto srcSlots = envelope.GetTriAOVs(MATERIAL_TRI_AOV_INDEX);
	const size_t nTris = subdivided.GetTotalTriangleCount();
	assert(triBaseFace.size() == nTris);

	auto slots = std::make_shared<float[]>(nTris);
	for (size_t i = 0; i < nTris; ++i)
		slots[i] = srcSlots[triBaseFace[i]];
	subdivided.SetTriAOV(MATERIAL_TRI_AOV_INDEX, slots, nTris);
}

//------------------------------------------------------------------------------
// FilterByMaterialSlot
//------------------------------------------------------------------------------

ExtTriangleMeshUPtr FilterByMaterialSlot(
	const ExtTriangleMesh &mesh,
	const int matSlot
) {
	const auto slots = mesh.GetTriAOVs(MATERIAL_TRI_AOV_INDEX);
	const auto allTris = mesh.GetTriangles();
	const size_t nTris = allTris.size();

	std::vector<size_t> kept;
	kept.reserve(nTris);
	for (size_t i = 0; i < nTris; ++i)
		if (static_cast<int>(slots[i]) == matSlot)
			kept.push_back(i);

	TriangleBuffer tris(kept.size());
	for (size_t i = 0; i < kept.size(); ++i)
		tris[i] = allTris[kept[i]];

	// Subset all existing triangle AOV layers (incl. the material slots)
	luxrays::ExtMeshProp<float> triAOVs;
	for (u_int l = 0; l < EXTMESH_MAX_DATA_COUNT; ++l) {
		if (!mesh.HasTriAOV(l))
			continue;
		const auto layer = mesh.GetTriAOVs(l);
		auto subset = std::make_shared<float[]>(kept.size());
		for (size_t i = 0; i < kept.size(); ++i)
			subset[i] = layer[kept[i]];
		triAOVs.SetLayer(l, subset, kept.size());
	}

	NormalBuffer normals;
	if (mesh.HasNormals())
		normals = mesh.GetNormals();  // copy shares the storage

	auto filtered = std::make_unique<ExtTriangleMesh>(
		mesh.GetVertexBuffer(),  // share the point storage
		std::move(tris),
		std::move(normals),
		std::optional<luxrays::ExtMeshProp<luxrays::UV>>(),
		std::optional<luxrays::ExtMeshProp<luxrays::Spectrum>>(),
		std::optional<luxrays::ExtMeshProp<float>>()
	);
	for (u_int l = 0; l < EXTMESH_MAX_DATA_COUNT; ++l)
		if (triAOVs.LayerHasValues(l))
			filtered->SetTriAOV(l, triAOVs.GetLayer(l), kept.size());
	filtered->SetName(mesh.GetName());
	return filtered;
}

}

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
