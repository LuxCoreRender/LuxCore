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
#include <boost/pending/disjoint_sets.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/islandaovshape.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

bool IslandAOVShape::IsSameVertex(const ExtTriangleMesh *srcMesh,
		const u_int vertex1Index, const u_int vertex2Index) const {
	if (DistanceSquared(srcMesh->GetVertex(Transform::TRANS_IDENTITY, vertex1Index),
			srcMesh->GetVertex(Transform::TRANS_IDENTITY, vertex2Index)) > DEFAULT_EPSILON_STATIC)
		return false;

	return true;
}

IslandAOVShape::IslandAOVShape(luxrays::ExtTriangleMesh *srcMesh, const u_int dataIndex) {
	SDL_LOG("IslandAOV shape " << srcMesh->GetName());

	const double startTime = WallClockTime();

	const u_int vertexCount = srcMesh->GetTotalVertexCount();
	const u_int triCount = srcMesh->GetTotalTriangleCount();
	const Triangle *tris = srcMesh->GetTriangles();


	// Built a mapping to have all very near vertices 
	vector<u_int> uniqueVertices(vertexCount);
	vector<bool> uniqueVerticesDone(vertexCount, false);
	u_int uniqueVertCount = 0;
	for (u_int i = 0; i < vertexCount; ++i) {
		if (uniqueVerticesDone[i])
			continue;

		// The current one is an unique vertex
		uniqueVertices[i] = i;
		++uniqueVertCount;

		#pragma omp parallel for
		for (
				// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
				unsigned
#endif
				int j = i + 1; j < vertexCount; ++j) {
			if (IsSameVertex(srcMesh, i, j)) {
				uniqueVertices[j] = i;
				uniqueVerticesDone[j] = true;
			}
		}
	}
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

	vector<u_int> rank(triCount);
	vector<u_int> parent(triCount);
	boost::disjoint_sets<u_int *,u_int *> ds(&rank[0], &parent[0]);

	// Create the initial sets with each triangle
	for (u_int i = 0; i < triCount; ++i)
		ds.make_set(triangleIndices[i]);

	// Link each triangle with the group leader
	for (u_int i = 0; i < triCount; ++i) {
		const Triangle &tri = tris[i];

		for (u_int j = 0; j < 3; ++j) {
			ds.union_set(triangleIndices[i], triangleIndices[leaderIndices[uniqueVertices[tri.v[j]]]]);
		}
	}

	// Build the list of islands
	float *triAOV = new float[triCount];
	unordered_map<u_int, u_int> islandIndices;
	u_int islandCount = 0;
	for (u_int i = 0; i < triCount; ++i) {
		const u_int leaderIndex = ds.find_set(triangleIndices[i]);

		// Check if it is a new leader
		u_int islandIndex;
		if (islandIndices.find(leaderIndex) == islandIndices.end()) {
			islandIndex = islandCount;
			islandIndices[leaderIndex] = islandCount++;
		} else
			islandIndex = islandIndices[leaderIndex];
		
		triAOV[i] = islandIndex;
	}

	mesh = srcMesh->Copy();
	mesh->SetTriAOV(dataIndex, &triAOV[0]);

	const double endTime = WallClockTime();
	SDL_LOG("IslandAOV time: " << (boost::format("%.3f") % (endTime - startTime)) << "secs");
}

IslandAOVShape::~IslandAOVShape() {
	if (!refined)
		delete mesh;
}

ExtTriangleMesh *IslandAOVShape::RefineImpl(const Scene *scene) {
	return mesh;
}
