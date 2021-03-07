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
					// It is a shared edge. Check if the 2 triangle are not coplanar.
					
					// Pick the triangle normals
					const Normal &tri0Normal = triNormals[e0.tri];
					const Normal &tri1Normal = triNormals[e1.tri];

					if (AbsDot(tri0Normal, tri1Normal) < 1.f -  DEFAULT_EPSILON_STATIC) {
						// It is a candidate. Check if it is a convex edge.

						// Pick the vertex, not part of the edge, of the first triangle
						const Point &tri0Vertex = vertices[tris[e0.tri].v[(e0.edge + 2) % 3]];

						// Pick the vertex, not part of the edge, of the second triangle
						const Point &tri1Vertex = vertices[tris[e1.tri].v[(e1.edge + 2) % 3]];

						// Compare the vector between the vertices not part of the shared edge and
						// the triangle 0 normal
						const float angle = Dot(tri0Normal, Normalize(tri1Vertex - tri0Vertex));

						if (angle < -DEFAULT_EPSILON_STATIC) {
							// Ok, it is a convex edge. It is an edge to bevel.
							e1.alreadyFound = true;

							const Normal &tri1Normal = triNormals[e1.tri];

							// Normals half vector direction
							const Vector h(-Normalize(tri0Normal + tri1Normal));
							const float cosHAngle = AbsDot(h, tri0Normal);

							// Compute the bevel cylinder vertices
							const float distance = bevelRadius / cosHAngle;

							const Vector vertexOffset(distance * h);
							const Vector bevelOffset =  Normalize(vertices[e0.v1] - vertices[e0.v0]) * bevelRadius;
							Point cv0(vertices[e0.v0] + vertexOffset + bevelOffset);
							Point cv1(vertices[e0.v1] + vertexOffset - bevelOffset);

							// Compute the max. distance from the edge of beveled point
							const float hAngle = acosf(cosHAngle);
							const float maxEdgeDistance = distance * sinf(hAngle);

							// Add a new BevelCylinder
							const u_int bevelCylinderIndex = bevelCyls.size();
							bevelCyls.push_back(BevelCylinder(cv0, cv1, e0.v0, e0.v1, maxEdgeDistance));

							// Add the BevelCylinder to the 2 triangles sharing the edge
							if (!triBevelCyls[e0.tri].IsFull())
								triBevelCyls[e0.tri].AddBevelCylinderIndex(bevelCylinderIndex);
							if (!triBevelCyls[e1.tri].IsFull())
								triBevelCyls[e1.tri].AddBevelCylinderIndex(bevelCylinderIndex);
						}

						// I will bevel only edges shared by only 2 triangles
						break;
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

bool ExtTriangleMesh::BevelCylinder::CanIntersect(const Point *vertices, const luxrays::Point &pos) const {
	const Point &a = pos;
	const Point &b = vertices[indexEdgeV0];
	const Point &c = vertices[indexEdgeV1];

	const Vector d = Normalize(c - b);
	const Vector v = a - b;

	const float t = Dot(v, d);
	const Point p = b + t * d;

	const float distance = (a - p).Length();

	return (distance < maxEdgeDistance);
}

float ExtTriangleMesh::BevelCylinder::Intersect(const Ray &ray, const float bevelRadius) const {

	// From Capsule intersection code at https://iquilezles.org/www/articles/intersectors/intersectors.htm
	// See also Distance between Lines at http://geomalgorithms.com/a07-_distance.html
	
	const Point &pa = v0;
	const Point &pb = v1;
	const Point &ro = ray.o;
	const Vector &rd = ray.d;
	const float &ra = bevelRadius;
	
    const Vector  ba = pb - pa;
    const Vector  oa = ro - pa;

    const float baba = Dot(ba, ba);
    const float bard = Dot(ba, rd);
    const float baoa = Dot(ba, oa);
    const float rdoa = Dot(rd, oa);
    const float oaoa = Dot(oa, oa);

    float a = baba - bard * bard;
    float b = baba * rdoa - baoa * bard;
	float c = baba * oaoa - baoa * baoa - ra * ra * baba;
    
	float h = b * b - a * c;
    if (h >= 0.f) {
		float t = (-b - sqrtf(h)) / a;
		const float y = baoa + t * bard;

		// Cylinder body of the BevelCylinder
		if ((y > 0.f) && (y < baba) && (t > ray.mint) && (t < ray.maxt))
			return t;

		// Spherical caps of the BevelCylinder
		const Vector oc = (y <= 0.f) ? oa : ro - pb;
		b = Dot(rd, oc);
		c = Dot(oc, oc) - ra * ra;
		h = b * b - c;
		if (h > 0.f) {
			t = -b - sqrtf(h);
			
			if ((t > ray.mint) && (t < ray.maxt))
				return t;
		}
	}

    return -1.f;
}

void ExtTriangleMesh::BevelCylinder::IntersectNormal(const Point &pos, const float bevelRadius,
		Normal &n) const {
	const Point &a = v0;
	const Point &b = v1;
	const float &r = bevelRadius;
	
    const Vector ba = b - a;
    const Vector pa = pos - a;
	const float h = Clamp(Dot(pa, ba) / Dot(ba, ba), 0.f, 1.f);

	n = Normal((pa - h * ba) * (1.f / r));
}

bool ExtTriangleMesh::IntersectBevel(const Ray &ray, const RayHit &rayHit,
		bool &continueToTrace, float &rayHitT, Point &p, Normal &n) const {
	// Check the intersection with TriangleBevelCylinders
	const TriangleBevelCylinders &triBevelCyl = triBevelCylinders[rayHit.triangleIndex];

	// Check the intersection with all BevelCylinder
	for (u_int i = 0; i < 3; ++i) {
		if (triBevelCyl.indices[i] != NULL_INDEX) {
			const BevelCylinder &bevelCylinder = bevelCylinders[triBevelCyl.indices[i]];

			// Check if p distance form the edge is small enough to be a candidate
			// for the bevel process
			p = ray(rayHit.t);
			if (bevelCylinder.CanIntersect(vertices, p)) {
				rayHitT = bevelCylinder.Intersect(ray, bevelRadius);
				if (rayHitT > 0.f) {
					continueToTrace = false;

					bevelCylinder.IntersectNormal(p, bevelRadius, n);

					return true;
				} else {
					continueToTrace = true;

					return false;
				}
			}
		}
	}

	continueToTrace = false;

	return false;
}
