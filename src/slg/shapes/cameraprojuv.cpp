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
#include "slg/shapes/cameraprojuv.h"
#include "slg/scene/scene.h"
#include "slg/cameras/camera.h"

using namespace std;
using namespace luxrays;
using namespace slg;

CameraProjUVShape::CameraProjUVShape(ExtTriangleMesh *m, const u_int index) :
		uvIndex(index) {
	mesh = m->Copy();
}

CameraProjUVShape::~CameraProjUVShape() {
	if (!refined)
		delete mesh;
}

ExtTriangleMesh *CameraProjUVShape::RefineImpl(const Scene *scene) {
	SDL_LOG("CameraProjUV shape " << mesh->GetName());

	const u_int vertCount = mesh->GetTotalVertexCount();
	SDL_LOG("CameraProjUV shape has " << vertCount << " vertices");

	const Camera *camera = scene->camera;

	UV *uvs = new UV[vertCount];
	const float invFilmWidth = 1.f / camera->filmWidth;
	const float invFilmHeight = 1.f / camera->filmHeight;

#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < vertCount; ++i) {
		const Point p = mesh->GetVertex(Transform::TRANS_IDENTITY, i);

		float filmX = 0.f;
		float filmY = 0.f;
		camera->GetSamplePosition(p, &filmX, &filmY);
		uvs[i].u = filmX * invFilmWidth;
		uvs[i].v = filmY * invFilmHeight;
	}
	
	if (mesh->HasUVs(uvIndex))
		mesh->DeleteUVs(uvIndex);
	mesh->SetUVs(uvIndex, uvs);
	
	return mesh;
}
