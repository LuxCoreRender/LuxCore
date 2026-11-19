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
#include <unordered_map>
#include <vector>

#include "luxrays/utils/strutils.h"
#include "slg/shapes/subdiv_envelope.h"
#include "slg/shapes/tessellate_core.h"
#include "slg/scene/scene.h"
#include "slg/scene/shape.h"

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

ShapeUPtr TessellateShape::ApplyToShape(
	ShapeConstRef srcShape,
	const u_int rate,
	const bool interpolatedNormals,
	const std::string &resultName
) {
	if (srcShape.GetSubmeshCount() == 0)
		throw std::runtime_error("Tessellate shape on an empty shape: " + resultName);
	// Same N = 2^rate convention as ApplyTessellate
	if (rate > 30)
		throw std::runtime_error(
			"Tessellate rate too large (max 30): " + ToString(rate));
	const size_t N = (size_t)1 << rate;
	const size_t NN = N * N;
	const double startTime = WallClockTime();

	// Group the submeshes by their point-buffer storage: the submeshes of one
	// object share the root buffer, but e.g. after another topology-changing
	// modifier they may have their own. Each group is tessellated ONCE as one
	// merged envelope mesh over the shared buffer (subdiv-envelope pattern),
	// then the result is split back per submesh. All the result submeshes of
	// a group share the single tessellated buffer, so shared edges stay
	// welded (watertight) and a following vertex-level modifier (e.g.
	// displacement) runs once on that buffer.
	const auto &submeshes = srcShape.GetSubmeshes();
	std::unordered_map<const void *, std::vector<size_t>> groups;
	for (size_t i = 0; i < submeshes.size(); ++i)
		groups[submeshes[i].get().GetVertexBuffer().Data()].push_back(i);

	struct GroupData {
		ExtTriangleMeshUPtr tess;                       // tessellated envelope
		std::vector<std::pair<size_t, size_t>> ranges;  // source face range per submesh
	};
	std::unordered_map<const void *, GroupData> perGroup;
	size_t totalSrcTris = 0, totalDstTris = 0;
	for (auto &[bufPtr, indices] : groups) {
		std::vector<const ExtTriangleMesh *> groupMeshes;
		groupMeshes.reserve(indices.size());
		for (size_t idx : indices)
			groupMeshes.push_back(&submeshes[idx].get());

		GroupData g;
		if (groupMeshes.size() == 1) {
			// Single submesh: tessellate it directly (no envelope needed,
			// all its mesh data is preserved as-is)
			g.tess = TessellateTopology(*groupMeshes[0], N, interpolatedNormals);
		} else {
			// Several submeshes sharing the buffer: tessellate the merged
			// envelope once, then resplit per submesh below
			g.tess = TessellateTopology(*BuildEnvelopeFromMeshes(groupMeshes), N, interpolatedNormals);
		}
		size_t faceOffset = 0;
		for (const auto *m : groupMeshes) {
			const size_t count = m->GetTotalTriangleCount();
			g.ranges.emplace_back(faceOffset, faceOffset + count);
			faceOffset += count;
			totalSrcTris += count;
		}
		totalDstTris += g.tess->GetTotalTriangleCount();
		perGroup.emplace(bufPtr, std::move(g));
	}

	const void *firstBuf = submeshes[0].get().GetVertexBuffer().Data();
	auto result = std::make_unique<Shape>(resultName, perGroup[firstBuf].tess->GetVertexBuffer());

	// Resplit the tessellated envelope(s) per source submesh, in source
	// order. The result submeshes share the tessellated vertex buffer (a
	// Buffer copy shares storage) and the per-vertex data layers; only the
	// triangles and the per-triangle AOVs are sliced.
	const bool single = submeshes.size() == 1;
	std::vector<ExtTriangleMeshUPtr> resultMeshes(submeshes.size());
	for (const auto &[bufPtr, indices] : groups) {
		const GroupData &g = perGroup[bufPtr];
		const auto srcTris = g.tess->GetTriangles();
		const u_int numPoints = g.tess->GetTotalVertexCount();
		for (size_t li = 0; li < indices.size(); ++li) {
			const size_t gi = indices[li];
			const auto [f0, f1] = g.ranges[li];
			const size_t triCount = (f1 - f0) * NN;

			TriangleBuffer slice(srcTris.subspan(f0 * NN, triCount));
			auto mesh = std::make_unique<ExtTriangleMesh>(
				g.tess->GetVertexBuffer(),  // sharing ctor: storage shared
				std::move(slice),
				NormalBuffer(g.tess->GetNormals()),
				std::optional<ExtMeshProp<UV>>(g.tess->GetAllUVs()),
				std::optional<ExtMeshProp<Spectrum>>(g.tess->GetAllColors()),
				std::optional<ExtMeshProp<float>>(g.tess->GetAllAlphas()),
				g.tess->GetBevelRadius()
			);
			for (u_int l = 0; l < EXTMESH_MAX_DATA_COUNT; ++l) {
				if (g.tess->HasVertexAOV(l))
					mesh->SetVertexAOV(l, g.tess->GetVertexAOVs(l), numPoints);
			}
			for (u_int l = 0; l < EXTMESH_MAX_DATA_COUNT; ++l) {
				if (!g.tess->HasTriAOV(l))
					continue;
				const auto src = g.tess->GetTriAOVs(l);
				auto dst = std::make_shared<float[]>(triCount);
				for (size_t t = 0; t < triCount; ++t)
					dst[t] = src[f0 * NN + t];
				mesh->SetTriAOV(l, dst, triCount);
			}
			mesh->SetName(single ? resultName : resultName + "i" + ToString(gi));
			mesh->SetShapeName(resultName);
			resultMeshes[gi] = std::move(mesh);
		}
	}
	for (auto &m : resultMeshes)
		result->AddSubmesh(std::move(m));

	SDL_LOG("Tessellated shape " << resultName << " from " << totalSrcTris
		<< " to " << totalDstTris << " faces in "
		<< WallClockTime() - startTime << " secs");

	return result;
}

}  // ~namespace slg

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
