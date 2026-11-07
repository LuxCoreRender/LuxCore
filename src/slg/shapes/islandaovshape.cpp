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

#include <unordered_map>

#include <boost/format.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/islandaovshape.h"
#include "slg/scene/scene.h"
#include "slg/utils/group_by_equivalence.h"

using namespace std;
using namespace luxrays;
using namespace slg;

IslandAOVShape::IslandAOVShape(ExtTriangleMeshRef srcMesh, const u_int dataIndex) {
	SDL_LOG("IslandAOV shape " << srcMesh.GetName());

	const double startTime = WallClockTime();

	const auto vertexCount = srcMesh.GetTotalVertexCount();
	const auto triCount = srcMesh.GetTotalTriangleCount();
	const auto tris = srcMesh.GetTriangles();
	SDL_LOG("IslandAOV shape vertex count: " << vertexCount);
	SDL_LOG("IslandAOV shape triangle count: " << triCount);

	// Built a mapping to have all very near vertices
	auto compareVerts = [](const TriangleMesh &mesh, const u_int vertIndex1, const u_int vertIndex2) {
		auto& triMesh = dynamic_cast<const ExtTriangleMesh&>(mesh);

		const Point v1 = triMesh.GetVertex(Transform::TRANS_IDENTITY, vertIndex1);
		const Point v2 = triMesh.GetVertex(Transform::TRANS_IDENTITY, vertIndex2);

		return (DistanceSquared(v1, v2) < DEFAULT_EPSILON_STATIC);
	};
	vector<u_int> uniqueVertices;
	const u_int uniqueVertCount = srcMesh.GetUniqueVerticesMapping(uniqueVertices, compareVerts);
	SDL_LOG("IslandAOV shape has " << uniqueVertCount << " unique vertices over " << vertexCount);

	vector<u_int> triangleIndices(triCount);
	for (u_int i = 0; i < triCount; ++i)
		triangleIndices[i] = i;

	// Set the group leaders
	vector<u_int> leaderIndices(vertexCount, NULL_INDEX);
	for (u_int i = 0; i < triCount; ++i) {
		const Triangle &tri = tris[i];

		for (u_int j = 0; j < 3; ++j) {
			if (leaderIndices[uniqueVertices[tri.v[j]]] == NULL_INDEX) {
				// I can set this triangle as leader
				leaderIndices[uniqueVertices[tri.v[j]]] = i;
			}
		}
	}

	// Link each triangle with the group leader of its vertices, and compute
	// the islands with the parallel union-find (labels variant)
	vector<slg::equiv::Relation> relations;
	relations.reserve(triCount * 3);
	for (u_int i = 0; i < triCount; ++i) {
		const Triangle &tri = tris[i];

		for (u_int j = 0; j < 3; ++j) {
			relations.emplace_back(triangleIndices[i],
				triangleIndices[leaderIndices[uniqueVertices[tri.v[j]]]]);
		}
	}
	const std::vector<uint32_t> labels = slg::equiv::GroupByEquivalenceLabels(
		triCount, std::span<const slg::equiv::Relation>(relations));

	// Build the list of islands
	auto triAOV = std::make_shared<float[]>(triCount);
	unordered_map<u_int, u_int> islandIndices;
	u_int islandCount = 0;
	for (u_int i = 0; i < triCount; ++i) {
		const u_int leaderIndex = labels[i];

		// Check if it is a new leader
		u_int islandIndex;
		if (islandIndices.find(leaderIndex) == islandIndices.end()) {
			islandIndex = islandCount;
			islandIndices[leaderIndex] = islandCount++;
		} else
			islandIndex = islandIndices[leaderIndex];

		triAOV[i] = islandIndex;
	}

	SDL_LOG("IslandAOV shape island count: " << islandCount);

	mesh = srcMesh.Copy();
	mesh->DeleteTriAOV(dataIndex);
	mesh->SetTriAOV(dataIndex, triAOV, triCount);

	const double endTime = WallClockTime();
	SDL_LOG("IslandAOV time: " << (boost::format("%.3f") % (endTime - startTime)) << "secs");
}

IslandAOVShape::~IslandAOVShape() {
}

ExtTriangleMeshUPtr IslandAOVShape::RefineImpl(SceneConstRef scene) {
	return std::move(mesh);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
