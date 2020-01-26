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

#include "slg/bsdf/hitpoint.h"
#include "slg/scene/scene.h"

using namespace luxrays;
using namespace slg;
using namespace std;

// Used when hitting a surface
//
// Note: very important, this method assume localToWorld file has been _already_
// initialized. This is done for performance reasons. 
//
// Note: This is also _not_ initializing volume related information.

void HitPoint::Init(const bool fixedFromLight, const bool throughShadowTransp,
		const Scene &scene, const u_int meshIndex, const u_int triangleIndex,
		const Point &pnt, const Vector &dir,
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
	geometryN = mesh->GetGeometryNormal(localToWorld, triangleIndex);
	interpolatedN = mesh->InterpolateTriNormal(localToWorld, triangleIndex, b1, b2);
	shadeN = interpolatedN;
	intoObject = (Dot(-fixedDir, geometryN) < 0.f);

	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		// Interpolate UV coordinates
		uv[i] = mesh->InterpolateTriUV(triangleIndex, b1, b2, i);

		// Interpolate color
		color[i] = mesh->InterpolateTriColor(triangleIndex, b1, b2, i);

		// Interpolate alpha
		alpha[i] = mesh->InterpolateTriAlpha(triangleIndex, b1, b2, i);
	}

	// Compute geometry differentials (always with the first set of UVs)
	mesh->GetDifferentials(localToWorld,
			triangleIndex, shadeN,
			0, // The UV set to use, always the first
			&dpdu, &dpdv, &dndu, &dndv);

	// Note: I'm not initializing volume related information here
	interiorVolume = nullptr;
	exteriorVolume = nullptr;
}

// Initialize all fields (i.e. the one missing a default constructor)
void HitPoint::Init() {
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		color[i] = Spectrum(1.f);
		alpha[i] = 1.f;
	}
	
	passThroughEvent = 0.f;
	interiorVolume = nullptr;
	exteriorVolume = nullptr;
	objectID = 0;
	fromLight = false;
	intoObject = true;
	throughShadowTransparency = false;
}
