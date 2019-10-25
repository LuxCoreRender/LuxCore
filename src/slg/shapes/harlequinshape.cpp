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

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/harlequinshape.h"
#include "slg/scene/scene.h"
#include "slg/utils/harlequincolors.h"

using namespace std;
using namespace luxrays;
using namespace slg;

HarlequinShape::HarlequinShape(luxrays::ExtTriangleMesh *srcMesh) {
	SDL_LOG("Harlequin shape " << srcMesh->GetName());

	const double startTime = WallClockTime();

	const u_int triCount = srcMesh->GetTotalTriangleCount();
	const Point *vertices = srcMesh->GetVertices();
	const Triangle *tris = srcMesh->GetTriangles();

	Point *newVertices = ExtTriangleMesh::AllocVerticesBuffer(triCount * 3);
	Triangle *newTris = ExtTriangleMesh::AllocTrianglesBuffer(triCount);
	Spectrum *newVertCols = new Spectrum[triCount * 3];
	for (u_int i = 0; i < triCount; ++i) {
		const Triangle &tri = tris[i];
		Triangle &newTri = newTris[i];
		const Spectrum col = GetHarlequinColorByIndex(i);

		newTri.v[0] = i * 3;
		newTri.v[1] = i * 3 + 1;
		newTri.v[2] = i * 3 + 2;

		newVertices[newTri.v[0]] = vertices[tri.v[0]];
		newVertCols[newTri.v[0]] = col;

		newVertices[newTri.v[1]] = vertices[tri.v[1]];
		newVertCols[newTri.v[1]] = col;

		newVertices[newTri.v[2]] = vertices[tri.v[2]];
		newVertCols[newTri.v[2]] = col;
	}
	
	// Make a copy of the original mesh and overwrite vertex informations
	mesh = new ExtTriangleMesh(triCount * 3, triCount, newVertices, newTris,
			nullptr, nullptr, newVertCols);
	
	// For some debugging
	//mesh->Save("debug.ply");
	
	const double endTime = WallClockTime();
	SDL_LOG("Displacement time: " << (boost::format("%.3f") % (endTime - startTime)) << "secs");
}

HarlequinShape::~HarlequinShape() {
	if (!refined)
		delete mesh;
}

ExtTriangleMesh *HarlequinShape::RefineImpl(const Scene *scene) {
	return mesh;
}
