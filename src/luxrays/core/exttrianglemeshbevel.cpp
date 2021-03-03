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

#include "luxrays/core/exttrianglemesh.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// ExtTriangleMesh bevel related methods
//------------------------------------------------------------------------------

void ExtTriangleMesh::PreprocessBevel() {
	const double start = WallClockTime();

	if (bevelRadius > 0.f) {
		//----------------------------------------------------------------------
		// Edge class
		//----------------------------------------------------------------------

		class Edge {
		public:
			Edge(const u_int triIndex, const u_int e, const u_int va, const u_int vb) : tri(triIndex),
					edge(e), v0(va), v1(vb), alreadyFound(false) { }
			~Edge() { }

			const u_int tri, edge;
			const u_int v0, v1;

			bool alreadyFound;
		};

		//----------------------------------------------------------------------
		// Build the edge information
		//----------------------------------------------------------------------

		vector<Edge> edges;
		for (u_int i = 0; i < triCount; ++i) {
			edges.push_back(Edge(i, 0, tris[i].v[0], tris[i].v[1]));
			edges.push_back(Edge(i, 1, tris[i].v[1], tris[i].v[2]));
			edges.push_back(Edge(i, 2, tris[i].v[2], tris[i].v[0]));
		}

		cout << "ExtTriangleMesh " << this << " edges count: " << edges.size() << endl;

		//----------------------------------------------------------------------
		// Initialize bevelCylinders and triBevelCylinders
		//----------------------------------------------------------------------

		vector<BevelCylinder> bevelCyls;
		vector<TriangleBevelCylinders> triBevelCyls(triCount);

		//----------------------------------------------------------------------
		// Look for bevel edges
		//----------------------------------------------------------------------

		auto IsSameVertex = [&](const u_int v0, const u_int v1) {
			return DistanceSquared(vertices[v0], vertices[v1]) < DEFAULT_EPSILON_STATIC;
		};

		auto IsSameEdge = [&](const u_int edge0Index, const u_int edge1Index) {
			const u_int e0v0 = edges[edge0Index].v0;
			const u_int e0v1 = edges[edge0Index].v1;
			const u_int e1v0 = edges[edge1Index].v0;
			const u_int e1v1 = edges[edge1Index].v1;

			return
				// Check if the vertices are near enough
				(IsSameVertex(e0v0, e1v0) && IsSameVertex(e0v1, e1v1)) ||
					(IsSameVertex(e0v0, e1v1) && IsSameVertex(e0v1, e1v0));
		};

		for (u_int edge0Index = 0; edge0Index < edges.size(); ++edge0Index) {
			Edge &e0 = edges[edge0Index];
			e0.alreadyFound = true;

			for (u_int edge1Index = edge0Index + 1; edge1Index < edges.size(); ++edge1Index) {
				Edge &e1 = edges[edge1Index];

				if (!e1.alreadyFound && IsSameEdge(edge0Index, edge1Index)) {
					// It is a candidate. Check if it a convex edge.
					
					// Pick the normal of the first triangles
					const Normal &tri0Normal = triNormals[e0.tri];

					// Pick the vertex, not part of the edge, of the first triangle
					const Point &tri0Vertex = vertices[(e0.edge + 2) % 3];

					// Pick the vertex, not part of the edge, of the second triangle
					const Point &tri1Vertex = vertices[(e1.edge + 2) % 3];
					
					// Compare the vector between the vertices not part of the shared edge and
					// the triangle 0 normal
					const float angle = Dot(tri0Normal, Normalize(tri1Vertex - tri0Vertex));
					
					if (angle <= DEFAULT_EPSILON_STATIC) {
						// Ok, it is a convex edge. It is an edge to bevel.
						e1.alreadyFound = true;
						
						const Normal &tri1Normal = triNormals[e1.tri];

						// /Normals half vector direction
						const Vector h(-Normalize(tri0Normal + tri1Normal));
						const float cosHAngle = AbsDot(h, tri0Normal);

						// Compute the bevel cylinder vertices

						const float sinAlpha = sinf(acosf(cosHAngle));
						const float distance = bevelRadius / sinAlpha;
						const Vector vertexOffset(distance * h);
						Vector cv0(vertices[e0.v0] + vertexOffset);
						Vector cv1(vertices[e0.v1] + vertexOffset);
						
						// Add a new BevelCylinder
						const u_int bevelCylinderIndex = bevelCyls.size();
						bevelCyls.push_back(BevelCylinder(cv0, cv1));

						// Add the BevelCylinder to the 2 triangles sharing the edge
						if (!triBevelCyls[e0.tri].IsFull())
							triBevelCyls[e0.tri].AddBevelCylinderIndex(bevelCylinderIndex);
						if (!triBevelCyls[e1.tri].IsFull())
							triBevelCyls[e1.tri].AddBevelCylinderIndex(bevelCylinderIndex);
					}
				}
			}			
		}

		cout << "ExtTriangleMesh " << this << " bevel cylinders count: " << bevelCyls.size() << endl;

		delete[] bevelCylinders;
		bevelCylinders = new BevelCylinder[bevelCyls.size()];
		copy(bevelCyls.begin(), bevelCyls.end(), bevelCylinders);

		delete[] triBevelCylinders;
		triBevelCylinders = new TriangleBevelCylinders[triCount];
		copy(triBevelCyls.begin(), triBevelCyls.end(), triBevelCylinders);
	} else {
		delete[] bevelCylinders;
		bevelCylinders = nullptr;
		delete[] triBevelCylinders;
		triBevelCylinders = nullptr;
	}
	
	const double endTotal = WallClockTime();
	cout << "ExtTriangleMesh " << this << " bevel preprocessing time: " << (endTotal - start) << "secs" << endl;
}

bool ExtTriangleMesh::IntersectBevel(luxrays::Ray &ray, luxrays::RayHit rayHit) const {
	return true;
}
