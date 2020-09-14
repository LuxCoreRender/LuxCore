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

#include "luxrays/core/randomgen.h"
#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/randomtriangleaovshape.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

RandomTriangleAOVShape::RandomTriangleAOVShape(luxrays::ExtTriangleMesh *srcMesh,
		const u_int srcDataIndex, const u_int dstDataIndex) {
	SDL_LOG("RandomTriangleAOV shape " << srcMesh->GetName());

	if (!srcMesh->HasTriAOV(srcDataIndex)) {
		SDL_LOG("RandomTriangleAOV shape has no triangle AOV: " << srcDataIndex);
		mesh = srcMesh->Copy();
		return;
	}
	
	const double startTime = WallClockTime();

	const u_int triCount = srcMesh->GetTotalTriangleCount();
	float *dstTriAOV = new float[triCount];
	for (u_int i = 0; i < triCount; ++i) {
		// Use here the same algorithm used in RandomTexture
		const u_int seed = (int)srcMesh->GetTriAOV(i, srcDataIndex);

		TauswortheRandomGenerator rnd(seed);

		dstTriAOV[i] = rnd.floatValue();
	}
	
	mesh = srcMesh->Copy();
	mesh->DeleteTriAOV(dstDataIndex);
	mesh->SetTriAOV(dstDataIndex, &dstTriAOV[0]);

	const double endTime = WallClockTime();
	SDL_LOG("RandomTriangleAOV time: " << (boost::format("%.3f") % (endTime - startTime)) << "secs");
}

RandomTriangleAOVShape::~RandomTriangleAOVShape() {
	if (!refined)
		delete mesh;
}

ExtTriangleMesh *RandomTriangleAOVShape::RefineImpl(const Scene *scene) {
	return mesh;
}
