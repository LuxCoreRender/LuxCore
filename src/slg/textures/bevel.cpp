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

#include "luxrays/core/geometry/quaternion.h"

#include "slg/textures/bevel.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Bevel texture
//------------------------------------------------------------------------------

BevelTexture::BevelTexture(const Texture *t, const float r) : tex(t), radius(r) {
}

BevelTexture::~BevelTexture() {
}

Normal BevelTexture::Bump(const HitPoint &hitPoint, const float sampleDistance) const {
	const ExtMesh *mesh = hitPoint.mesh;
	if (!mesh)
		return hitPoint.shadeN;

	const Triangle &tri = mesh->GetTriangles()[hitPoint.triangleIndex];
	const Point v0 = mesh->GetVertex(hitPoint.localToWorld, tri.v[0]);
	const Point v1 = mesh->GetVertex(hitPoint.localToWorld, tri.v[1]);
	const Point v2 = mesh->GetVertex(hitPoint.localToWorld, tri.v[2]);

	const Vector e0 = v1 - v0;
	const Vector e1 = v2 - v1;
	const Vector e2 = v0 - v2;
	
	const float e0len = e0.Length();
	const float e1len = e1.Length();
	const float e2len = e2.Length();

	const float b0 = (hitPoint.p - v0).Length();
	const float b1 = (hitPoint.p - v1).Length();
	const float b2 = (hitPoint.p - v2).Length();

	const float dist0 = Triangle::GetHeight(e0len, b1, b0);
	const float dist1 = Triangle::GetHeight(e1len, b2, b1);
	const float dist2 = Triangle::GetHeight(e2len, b0, b2);

	bool hasBevel = false;

	const float theta0 = mesh->HasTriAOV(0) ? mesh->GetTriAOV(hitPoint.triangleIndex, 0) : 0.f;
	const float theta1 = mesh->HasTriAOV(1) ? mesh->GetTriAOV(hitPoint.triangleIndex, 1) : 0.f;
	const float theta2 = mesh->HasTriAOV(2) ? mesh->GetTriAOV(hitPoint.triangleIndex, 2) : 0.f;

	const Normal normal = mesh->GetGeometryNormal(Transform::TRANS_IDENTITY,
			hitPoint.triangleIndex);

	Normal edge0Normal;
	float k0 = 0.f;
	if ((dist0 < radius) && (theta0 != 0.f)) {
		// Interpolate between the edge normal angle and face normal angle (i.e. 0.0)
		k0 = dist0 / radius;
		const float theta = Lerp(k0, theta0, 0.f);
		const float halfTheta = theta * .5f;
		k0 = 1.f - k0;
		
		// Rotate the normal around the edge by theta using quaternions
		const Vector w = sinf(halfTheta) * (e0 / e0len);
		Quaternion q(cosf(halfTheta), w);
		
		edge0Normal = Normal(q.RotateVector(Vector(normal)));
		hasBevel = true;
	}

	Normal edge1Normal;
	float k1 = 0.f;
	if ((dist1 < radius) && (theta1 != 0.f)) {
		// Interpolate between the edge normal angle and face normal angle (i.e. 0.0)
		k1 = dist1 / radius;
		const float theta = Lerp(k1, theta1, 0.f);
		const float halfTheta = theta * .5f;
		k1 = 1.f - k1;

		// Rotate the normal around the edge by theta using quaternions
		const Vector w = sinf(halfTheta) * (e1 / e1len);
		Quaternion q(cosf(halfTheta), w);
		
		edge1Normal = Normal(q.RotateVector(Vector(normal)));
		hasBevel = true;
	}
	
	Normal edge2Normal;
	float k2 = 0.f;
	if ((dist2 < radius) && (theta2 != 0.f)) {
		// Interpolate between the edge normal angle and face normal angle (i.e. 0.0)
		k2 = dist2 / radius;
		const float theta = Lerp(k2, theta2, 0.f);
		const float halfTheta = theta * .5f;
		k2 = 1.f - k2;

		// Rotate the normal around the edge by theta using quaternions
		const Vector w = sinf(halfTheta) * (e2 / e2len);
		Quaternion q(cosf(halfTheta), w);
		
		edge2Normal = Normal(q.RotateVector(Vector(normal)));
		hasBevel = true;
	}

	if (hasBevel)
		return Normalize(k0 * edge0Normal + k1 * edge1Normal + k2 * edge2Normal);
	else
		return hitPoint.shadeN;
}

Properties BevelTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;
	
	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("bevel"));
	if (tex)
		props.Set(Property("scene.textures." + name + ".texture")(tex->GetSDLValue()));
	props.Set(Property("scene.textures." + name + ".radius")(radius));

	return props;
}
