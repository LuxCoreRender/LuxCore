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

#include <boost/format.hpp>

#include "slg/shapes/displacement.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

DisplacementShape::DisplacementShape(luxrays::ExtTriangleMesh *srcMesh, const Texture &dispMap,
			const float dispScale, const float dispOffset, const bool dispNormalSmooth) {
	SDL_LOG("Displacement shape " << srcMesh->GetName() << " with texture " << dispMap.GetName());

	if (!srcMesh->HasNormals())
		throw runtime_error("Displacement shape can be used only with mesh having vertex normals defined");
			
	const double startTime = WallClockTime();

	const u_int vertCount = srcMesh->GetTotalVertexCount();
	const Point *vertices = srcMesh->GetVertices();
	Point *newVertices = ExtTriangleMesh::AllocVerticesBuffer(vertCount);
	
	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < vertCount; ++i) {
		HitPoint hitPoint;

		hitPoint.fixedDir = Vector(0.f, 0.f, 1.f);
		hitPoint.p = srcMesh->GetVertex(0.f, i);

		hitPoint.uv = srcMesh->HasUVs() ? srcMesh->GetUV(i) : UV(0.f, 0.f);
		
		hitPoint.geometryN = srcMesh->GetShadeNormal(0.f, i);
		hitPoint.interpolatedN = hitPoint.geometryN;
		hitPoint.shadeN = hitPoint.interpolatedN;

		hitPoint.color =  srcMesh->HasColors() ? srcMesh->GetColor(i) : Spectrum(1.f);
		hitPoint.dpdu = 0.f;
		hitPoint.dpdv = 0.f;
		hitPoint.dndu = 0.f;
		hitPoint.dndv = 0.f;
		hitPoint.alpha = srcMesh->HasAlphas() ? srcMesh->GetAlpha(i) : 1.f;
		hitPoint.passThroughEvent = 0.f;
		srcMesh->GetLocal2World(0.f, hitPoint.localToWorld);
		hitPoint.interiorVolume = nullptr;
		hitPoint.exteriorVolume = nullptr;
		hitPoint.objectID = 0;
		hitPoint.fromLight = false;
		hitPoint.intoObject = true;
		hitPoint.throughShadowTransparency = false;

		const Vector disp = (dispMap.GetFloatValue(hitPoint) * dispScale + dispOffset) *
				Vector(hitPoint.shadeN);

		newVertices[i] = vertices[i] + disp;
	}
	
	// Make a copy of the original mesh and overwrite vertex informations
	mesh = srcMesh->Copy(newVertices, nullptr, nullptr, nullptr, nullptr, nullptr);
	if (dispNormalSmooth)
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
