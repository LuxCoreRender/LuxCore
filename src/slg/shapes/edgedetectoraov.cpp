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

#include <boost/format.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/edgedetectoraov.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

class Edge {
public:
	Edge(const u_int triIndex, const u_int e, const u_int va, const u_int vb) : tri(triIndex),
			edge(e), v0(va), v1(vb), aovValue(0.f), alreadyFound(false) { }
	~Edge() { }

	const u_int tri, edge;
	const u_int v0, v1;

	float aovValue;
	bool alreadyFound;
};

EdgeDetectorAOVShape::EdgeDetectorAOVShape(ExtTriangleMesh *srcMesh,
		const u_int destAOVIndex0, const u_int destAOVIndex1, const u_int destAOVIndex2) {
	assert (destAOVIndex0 < EXTMESH_MAX_DATA_COUNT);
	assert (destAOVIndex1 < EXTMESH_MAX_DATA_COUNT);
	assert (destAOVIndex2 < EXTMESH_MAX_DATA_COUNT);

	SDL_LOG("EdgeDetectorAOV " << srcMesh->GetName());

	const double startTime = WallClockTime();

	const Triangle *tris = srcMesh->GetTriangles();
	const u_int triCount = srcMesh->GetTotalTriangleCount();

	// Build the edge information
	vector<Edge> edges;
	for (u_int i = 0; i < triCount; ++i) {
		edges.push_back(Edge(i, 0, tris[i].v[0], tris[i].v[1]));
		edges.push_back(Edge(i, 1, tris[i].v[1], tris[i].v[2]));
		edges.push_back(Edge(i, 2, tris[i].v[2], tris[i].v[0]));
	}

	// Look for same edges

	auto IsSameVertex = [&](const u_int v0, const u_int v1) {
		return DistanceSquared(
					srcMesh->GetVertex(Transform::TRANS_IDENTITY, v0),
					srcMesh->GetVertex(Transform::TRANS_IDENTITY, v1)) < DEFAULT_EPSILON_STATIC;
	};

	auto IsSameEdge = [&](const u_int edge0Index, const u_int edge1Index) {
		const u_int e0v0 = edges[edge0Index].v0;
		const u_int e0v1 = edges[edge0Index].v1;
		const u_int e1v0 = edges[edge1Index].v0;
		const u_int e1v1 = edges[edge1Index].v1;
		
		return
			// Check if the vertices are near enough
			((IsSameVertex(e0v0, e1v0) && IsSameVertex(e0v1, e1v1)) ||
				(IsSameVertex(e0v0, e1v1) && IsSameVertex(e0v1, e1v0))) &&
			// Check if it is not a smoothed edge by interpolated normals
			(!srcMesh->HasNormals() || !(((e0v0 == e1v0) && (e0v1 == e1v1)) || ((e0v0 == e1v1) && (e0v1 == e1v0))));
	};

	for (u_int edge0Index = 0; edge0Index < edges.size(); ++edge0Index) {
		Edge &e0 = edges[edge0Index];
		e0.alreadyFound = true;

		for (u_int edge1Index = edge0Index + 1; edge1Index < edges.size(); ++edge1Index) {
			Edge &e1 = edges[edge1Index];

			if (!e1.alreadyFound && IsSameEdge(edge0Index, edge1Index)) {
				// It is a candidate

				// Check the triangles normals
				if (AbsDot(srcMesh->GetGeometryNormal(Transform::TRANS_IDENTITY, e0.tri),
						srcMesh->GetGeometryNormal(Transform::TRANS_IDENTITY, e1.tri)) < 1.f - DEFAULT_EPSILON_STATIC) {
					// Mark as detected
					e0.aovValue = 1.f;
					e1.aovValue = 1.f;
					e1.alreadyFound = true;
				}
			}
		}
	}

	// Create the processed mesh
	
	mesh = srcMesh->Copy();

	float *aovEdge[3];
	aovEdge[0] = new float[triCount];
	aovEdge[1] = new float[triCount];
	aovEdge[2] = new float[triCount];
	for (u_int edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
		Edge &e = edges[edgeIndex];

		aovEdge[e.edge][e.tri] = e.aovValue;
	}
	
	mesh->SetTriAOV(destAOVIndex0, aovEdge[0]);
	mesh->SetTriAOV(destAOVIndex1, aovEdge[1]);
	mesh->SetTriAOV(destAOVIndex2, aovEdge[2]);

	const double endTime = WallClockTime();
	SDL_LOG("EdgeDetectorAOV time: " << (boost::format("%.3f") % (endTime - startTime)) << "secs");
}

EdgeDetectorAOVShape::~EdgeDetectorAOVShape() {
	if (!refined)
		delete mesh;
}

ExtTriangleMesh *EdgeDetectorAOVShape::RefineImpl(const Scene *scene) {
	return mesh;
}
