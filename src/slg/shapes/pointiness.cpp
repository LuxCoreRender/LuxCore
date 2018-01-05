/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "slg/shapes/pointiness.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

class Edge {
public:
	Edge(const u_int va, const u_int vb) : v0(va), v1(vb) { }
	~Edge() { }

	// Used by std::set
	bool operator<(const Edge &e) const {
		if (((e.v0 == v0) && (e.v1 == v1)) ||
				((e.v0 == v1) && (e.v1 == v0)))
			return false;
		else
			return this < &e;
	}

	const u_int v0, v1;
};

PointinessShape::PointinessShape(ExtTriangleMesh *srcMesh) : Shape() {
	const u_int vertCount = srcMesh->GetTotalVertexCount();
	const u_int triCount = srcMesh->GetTotalTriangleCount();

	const Point *vertices = srcMesh->GetVertices();
	const Triangle *tris = srcMesh->GetTriangles();

	// Build the edge information
	set<Edge> edges;
	for (u_int i = 0; i < triCount; ++i) {
		edges.insert(Edge(tris[i].v[0], tris[i].v[1]));
		edges.insert(Edge(tris[i].v[1], tris[i].v[2]));
		edges.insert(Edge(tris[i].v[2], tris[i].v[0]));
	}

	// Build the vertex information
	vector<Vector> vertexEdgeVecs(vertCount, Vector());
	vector<u_int> vertexCounters(vertCount, 0);
	BOOST_FOREACH(const Edge &e, edges) {
		const Vector ev = Normalize(vertices[e.v0] - vertices[e.v1]);

		vertexEdgeVecs[e.v0] += ev;
		vertexCounters[e.v0]++;

		vertexEdgeVecs[e.v1] -= ev;
		vertexCounters[e.v1]++;
	}

	// Build the normal information
	vector<Normal> vertexNormal(vertCount);
	if (srcMesh->HasNormals()) {
		for (u_int i = 0; i < vertCount; ++i)
			vertexNormal[i] = srcMesh->GetShadeNormal(0.f, i);
	} else {
		fill(vertexNormal.begin(), vertexNormal.end(), Normal());

		for (u_int i = 0; i < triCount; ++i) {
			const Normal triNormal = srcMesh->GetGeometryNormal(0.f, i);

			vertexNormal[tris[i].v[0]] += triNormal;
			vertexNormal[tris[i].v[1]] += triNormal;
			vertexNormal[tris[i].v[2]] += triNormal;
		}
		for (u_int i = 0; i < vertCount; ++i)
			vertexNormal[i] = Normalize(vertexNormal[i]);
	}

	// Build the curvature information
	vector<float> rawCurvature(vertCount);
	for (u_int i = 0; i < vertCount; ++i) {
		if (vertexCounters[i] > 0)
			rawCurvature[i] = -Dot(vertexNormal[i], vertexEdgeVecs[i] / vertexCounters[i]);
		else
			rawCurvature[i] = 0.f;
	}
	
	// Blur the curvature information
	float *curvature = new float[vertCount];
	fill(curvature, curvature + vertCount, 0.f);
	fill(vertexCounters.begin(), vertexCounters.end(), 1);
	BOOST_FOREACH(const Edge &e, edges) {
		curvature[e.v0] += rawCurvature[e.v1];
		vertexCounters[e.v0]++;

		curvature[e.v1] += rawCurvature[e.v0];
		vertexCounters[e.v1]++;
	}

	// Normalize and factor also old vertex alphas
	if (srcMesh->HasAlphas()) {
		for (u_int i = 0; i < vertCount; ++i)
			curvature[i] *= srcMesh->GetAlpha(i) / vertexCounters[i];
	} else {
		for (u_int i = 0; i < vertCount; ++i)
			curvature[i] /= vertexCounters[i];		
	}

	// Make a copy of the original mesh and overwrite vertex color informations
	mesh = srcMesh->Copy(NULL, NULL, NULL, NULL, NULL, curvature);
}

PointinessShape::~PointinessShape() {
	if (!refined)
		delete mesh;
}

ExtTriangleMesh *PointinessShape::RefineImpl(const Scene *scene) {
	return mesh;
}
