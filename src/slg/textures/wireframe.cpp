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

#include "slg/textures/wireframe.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// WireFrame texture
//------------------------------------------------------------------------------

bool WireFrameTexture::Evaluate(const HitPoint &hitPoint) const {
	const ExtMesh *mesh = hitPoint.mesh;
	if (!mesh)
		return false;
	
	const Triangle &tri = mesh->GetTriangles()[hitPoint.triangleIndex];
	const Point v0 = mesh->GetVertex(hitPoint.localToWorld, tri.v[0]);
	const Point v1 = mesh->GetVertex(hitPoint.localToWorld, tri.v[1]);
	const Point v2 = mesh->GetVertex(hitPoint.localToWorld, tri.v[2]);

	const float e0 = (v1 - v0).Length();
	const float e1 = (v2 - v1).Length();
	const float e2 = (v0 - v2).Length();

	const float b0 = (hitPoint.p - v0).Length();
	const float b1 = (hitPoint.p - v1).Length();
	const float b2 = (hitPoint.p - v2).Length();

	const float dist0 = Triangle::GetHeight(e0, b1, b0);
	if ((dist0 < width) && (!mesh->HasTriAOV(0) || (mesh->GetTriAOV(hitPoint.triangleIndex, 0) != 0.f)))
		return true;

	const float dist1 = Triangle::GetHeight(e1, b2, b1);
	if ((dist1 < width) && (!mesh->HasTriAOV(1) || (mesh->GetTriAOV(hitPoint.triangleIndex, 1) != 0.f)))
			return true;

	const float dist2 = Triangle::GetHeight(e2, b0, b2);
	if ((dist2 < width) && (!mesh->HasTriAOV(2) || (mesh->GetTriAOV(hitPoint.triangleIndex, 2) != 0.f)))
		return true;

	return false;
}

float WireFrameTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return Evaluate(hitPoint) ? borderTex->GetFloatValue(hitPoint) :
		insideTex->GetFloatValue(hitPoint);
}

Spectrum WireFrameTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Evaluate(hitPoint) ? borderTex->GetSpectrumValue(hitPoint) :
		insideTex->GetSpectrumValue(hitPoint);
}

Properties WireFrameTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("wireframe"));
	props.Set(Property("scene.textures." + name + ".border")(borderTex->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".inside")(insideTex->GetSDLValue()));

	return props;
}
