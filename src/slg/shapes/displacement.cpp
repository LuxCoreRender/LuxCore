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
#include "slg/shapes/displacement.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

DisplacementShape::DisplacementShape(luxrays::ExtTriangleMesh *srcMesh, const Texture &dispMap,
		const Params &params) {
	SDL_LOG("Displacement shape " << srcMesh->GetName() << " with texture " << dispMap.GetName());

	// I need vertex normals
	if (!srcMesh->HasNormals())
		srcMesh->ComputeNormals();

	// I need vertex UVs for vector displacement
	if ((params.mapType == VECTOR_DISPLACEMENT) && !srcMesh->HasUVs(params.uvIndex))
		throw runtime_error("Displacement shape for vector displacement can be used only with mesh having UVs defined");

	const double startTime = WallClockTime();

	const u_int vertCount = srcMesh->GetTotalVertexCount();
	const Point *vertices = srcMesh->GetVertices();
	Point *newVertices = ExtTriangleMesh::AllocVerticesBuffer(vertCount);

	// I need to build the dpdu, dpdv, dndu, dndv for each vertex. They are mostly
	// used for vector displacement but they may be used by the texture too.
	vector<Vector> dpdu(vertCount);
	vector<Vector> dpdv(vertCount);
	vector<Normal> dndu(vertCount);
	vector<Normal> dndv(vertCount);

	vector<u_int> triangleIndex(vertCount);

	// Go trough the faces and save the information
	vector<bool> doneVerts(vertCount, false);
	const u_int triCount = srcMesh->GetTotalTriangleCount();
	const Triangle *tris = srcMesh->GetTriangles();
	for (u_int i = 0; i < triCount; ++i) {
		const Triangle &tri = tris[i];

		for (u_int j = 0; j < 3; ++j) {
			const u_int vertIndex = tri.v[j];

			if (!doneVerts[vertIndex]) {
				const Normal shadeN = srcMesh->GetShadeNormal(Transform::TRANS_IDENTITY, vertIndex);

				// Compute geometry differentials
				srcMesh->GetDifferentials(Transform::TRANS_IDENTITY, i, shadeN, 0,
						&dpdu[vertIndex], &dpdv[vertIndex],
						&dndu[vertIndex], &dndv[vertIndex]);

				triangleIndex[vertIndex] = i;

				doneVerts[vertIndex] = true;
			}
		}
	}

	const u_int binormalIndex = params.mapChannels[0];
	const u_int tangentIndex = params.mapChannels[1];
	const u_int normalIndex = params.mapChannels[2];

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < vertCount; ++i) {
		HitPoint hitPoint;
		
		hitPoint.fixedDir = Vector(0.f, 0.f, 1.f);
		hitPoint.p = srcMesh->GetVertex(Transform::TRANS_IDENTITY, i);

		hitPoint.geometryN = srcMesh->GetShadeNormal(Transform::TRANS_IDENTITY, i);
		hitPoint.interpolatedN = hitPoint.geometryN;
		hitPoint.shadeN = hitPoint.interpolatedN;

		hitPoint.defaultUV = srcMesh->HasUVs(params.uvIndex) ? srcMesh->GetUV(i, params.uvIndex) : UV(0.f, 0.f);
		hitPoint.mesh = srcMesh;
		hitPoint.triangleIndex = triangleIndex[i];
		if (i == tris[hitPoint.triangleIndex].v[0]) {
			// First vertex of the triangle
			hitPoint.triangleBariCoord1 = 0.f;
			hitPoint.triangleBariCoord2 = 0.f;
		} else if (i == tris[hitPoint.triangleIndex].v[1]) {
			// Second vertex of the triangle
			hitPoint.triangleBariCoord1 = 1.f;
			hitPoint.triangleBariCoord2 = 0.f;
		} else {
			// Last vertex of the triangle
			hitPoint.triangleBariCoord1 = 0.f;
			hitPoint.triangleBariCoord2 = 1.f;
		}

		hitPoint.dpdu = dpdu[i];
		hitPoint.dpdv = dpdv[i];
		hitPoint.dndu = dndu[i];
		hitPoint.dndv = dndv[i];

		hitPoint.passThroughEvent = 0.f;
		srcMesh->GetLocal2World(0.f, hitPoint.localToWorld);
		hitPoint.interiorVolume = nullptr;
		hitPoint.exteriorVolume = nullptr;
		hitPoint.objectID = 0;
		hitPoint.fromLight = false;
		hitPoint.intoObject = true;
		hitPoint.throughShadowTransparency = false;

		Vector disp;
		if (params.mapType == HIGHT_DISPLACEMENT)
			disp = (dispMap.GetFloatValue(hitPoint) * params.scale + params.offset) *
					Vector(hitPoint.shadeN);
		else {
			// Not using dispOffset parameter because it doesn't make very much sense
			// for vector displacement
			const Spectrum dispValue = dispMap.GetSpectrumValue(hitPoint) * params.scale;

			// Build the local reference system, uses shadeN, dpdu and dpdv
			const Frame frame = hitPoint.GetFrame();
			
			disp = frame.ToWorld(Vector(dispValue.c[binormalIndex], dispValue.c[tangentIndex], dispValue.c[normalIndex]));


			// I work on tangent space and in this case: R is an offset along
			// the tangent, G along the normal and B along the bitangent.
			// This is the Blender standard.
			//disp = frame.ToWorld(Vector(dispValue.c[2], dispValue.c[0], dispValue.c[1]));
			
			// This is the Mudbox standard.
			//disp = frame.ToWorld(Vector(dispValue.c[0], dispValue.c[2], dispValue.c[1]));
		}

		newVertices[i] = vertices[i] + disp;
	}
	
	// Make a copy of the original mesh and overwrite vertex information
	mesh = srcMesh->Copy(newVertices, nullptr, nullptr, nullptr, nullptr, nullptr);
	if (params.normalSmooth)
		mesh->ComputeNormals();
	
	// For some debugging
	//mesh->Save("debug.ply");
	
	const double endTime = WallClockTime();
	SDL_LOG("Displacement time: " << (boost::format("%.3f") % (endTime - startTime)) << "secs");
}

DisplacementShape::~DisplacementShape() {
	if (!refined)
		delete mesh;
}

ExtTriangleMesh *DisplacementShape::RefineImpl(const Scene *scene) {
	return mesh;
}
