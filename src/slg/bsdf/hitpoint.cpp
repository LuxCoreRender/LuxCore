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

#include "slg/bsdf/hitpoint.h"
#include "slg/scene/scene.h"

using namespace luxrays;
using namespace slg;
using namespace std;

// Used when hitting a surface
void HitPoint::Init(const bool fixedFromLight, const bool throughShadowTransp,
		const Scene &scene, const u_int meshIndex, const u_int triangleIndex,
		const float time, const Point &pnt, const Vector &dir,
		const float b1, const float b2,
		const float passThroughEvnt) {
	fromLight = fixedFromLight;
	throughShadowTransparency = throughShadowTransp;
	passThroughEvent = passThroughEvnt;

	p = pnt;
	fixedDir = dir;

	// Get the scene object
	const SceneObject *sceneObject = scene.objDefs.GetSceneObject(meshIndex);
	objectID = sceneObject->GetID();
	
	// Get the triangle
	const ExtMesh *mesh = sceneObject->GetExtMesh();

	// Interpolate face normal
	geometryN = mesh->GetGeometryNormal(time, triangleIndex);
	interpolatedN = mesh->InterpolateTriNormal(time, triangleIndex, b1, b2);
	shadeN = interpolatedN;
	intoObject = (Dot(-fixedDir, geometryN) < 0.f);

	// Interpolate UV coordinates
	uv = mesh->InterpolateTriUV(triangleIndex, b1, b2);

	// Interpolate color
	color = mesh->InterpolateTriColor(triangleIndex, b1, b2);

	// Interpolate alpha
	alpha = mesh->InterpolateTriAlpha(triangleIndex, b1, b2);

	// Compute geometry differentials
	mesh->GetDifferentials(time,
			triangleIndex, shadeN,
			&dpdu, &dpdv, &dndu, &dndv);
}
