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
#include "luxrays/core/shape.h"
#include "slg/shapes/harlequinshape.h"
#include "slg/scene/scene.h"
#include "slg/utils/harlequincolors.h"

using namespace std;
using namespace luxrays;
using namespace slg;

HarlequinShape::HarlequinShape(luxrays::ExtTriangleMeshRef srcMesh) {
	SDL_LOG("Harlequin shape " << srcMesh.GetName());

	const double startTime = WallClockTime();

	const auto triCount = srcMesh.GetTotalTriangleCount();
	const auto vertices = srcMesh.GetVertices();
	const auto tris = srcMesh.GetTriangles();

	VertexBuffer newVertices(triCount * 3);
	TriangleBuffer newTris(triCount);
	auto newVertCols = std::make_shared<Spectrum[]>(triCount * 3);
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

	shape = std::make_unique<Shape>("harlequin", std::move(newVertices));
	mesh = std::make_unique<ExtTriangleMesh>(
		*shape,
		std::move(newTris),
		NormalBuffer(),  // Normals (empty)
		nullptr,  // UVs
		newVertCols
	);

	// For some debugging
	//mesh->Save("debug.ply");

	const double endTime = WallClockTime();
	SDL_LOG("Harlequin time: " << (boost::format("%.3f") % (endTime - startTime)) << "secs");
}

HarlequinShape::~HarlequinShape() {
}

ExtTriangleMeshUPtr HarlequinShape::RefineImpl(SceneConstRef scene) {
	return std::move(mesh);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
