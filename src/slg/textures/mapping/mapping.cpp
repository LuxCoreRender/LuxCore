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

#include <math.h>

#include "slg/textures/mapping/mapping.h"
#include "luxrays/core/randomgen.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// TextureMapping2D
//------------------------------------------------------------------------------

Properties TextureMapping2D::ToProperties(const string &name) const {
	return Properties() <<
			Property(name + ".uvindex")(dataIndex);
}

//------------------------------------------------------------------------------
// UVMapping2D
//------------------------------------------------------------------------------

UVMapping2D::UVMapping2D(const u_int index,
		const float rot, const float uscale, const float vscale,
		const float udelta, const float vdelta) :
		TextureMapping2D(index),
		uvRotation(rot), uScale(uscale),
		vScale(vscale), uDelta(udelta), vDelta(vdelta),
		sinTheta(sinf(Radians(-uvRotation))), cosTheta(cosf(Radians(-uvRotation))) {
}

UV UVMapping2D::Map(const HitPoint &hitPoint) const {
	const UV uv = hitPoint.GetUV(dataIndex);

	// Scale
	const float uScaled = uv.u * uScale;
	const float vScaled = uv.v * vScale;

	// Rotate
	const float uRotated = uScaled * cosTheta - vScaled * sinTheta;
	const float vRotated = vScaled * cosTheta + uScaled * sinTheta;

	// Translate
	const float uTranslated = uRotated + uDelta;
	const float vTranslated = vRotated + vDelta;
	
	return UV(uTranslated, vTranslated);
}

UV UVMapping2D::MapDuv(const HitPoint &hitPoint, UV *ds, UV *dt) const {
	const float signUScale = Sgn(uScale);
	const float signVScale = Sgn(vScale);
	
	*ds = UV(signUScale * cosTheta, signUScale * sinTheta);
	*dt = UV(-signVScale * sinTheta, signVScale * cosTheta);

	return Map(hitPoint);
}

Properties UVMapping2D::ToProperties(const string &name) const {
	return Properties() <<
			Property(name + ".type")("uvmapping2d") <<
			TextureMapping2D::ToProperties(name) <<
			Property(name + ".rotation")(uvRotation) <<
			Property(name + ".uvscale")(uScale, vScale) <<
			Property(name + ".uvdelta")(uDelta, vDelta);
}

//------------------------------------------------------------------------------
// UVRandomMapping2D
//------------------------------------------------------------------------------

UVRandomMapping2D::UVRandomMapping2D(const u_int index, const SeedType seedTyp,
		const u_int triAOVIdx,
		const float uvRotationMn, const float uvRotationMx,
		const float uScaleMn, const float uScaleMx,
		const float vScaleMn, const float vScaleMx,
		const float uDeltaMn, const float uDeltaMx,
		const float vDeltaMn, const float vDeltaMx,
		const bool uniformScl) :
		TextureMapping2D(index), seedType(seedTyp), triAOVIndex(triAOVIdx),
		uvRotationMin(uvRotationMn), uvRotationMax(uvRotationMx),
		uScaleMin(uScaleMn), uScaleMax(uScaleMx),
		vScaleMin(vScaleMn), vScaleMax(vScaleMx),
		uDeltaMin(uDeltaMn), uDeltaMax(uDeltaMx),
		vDeltaMin(vDeltaMn), vDeltaMax(vDeltaMx),
		uniformScale(uniformScl) {
}

UV UVRandomMapping2D::Map(const HitPoint &hitPoint, UV *ds, UV *dt) const {
	// Select random parameters
	u_int seed;
	switch (seedType) {
		case OBJECT_ID:
			seed = hitPoint.objectID;
			break;
		case TRIANGLE_AOV:
			seed = (u_int)hitPoint.GetTriAOV(triAOVIndex);
			break;
		default:
			throw runtime_error("Unknown seed type in UVRandomMapping2D::Map(): " + ToString(seedType));
	}
	
	TauswortheRandomGenerator rndGen(seed);

	const float uvRotation = Lerp(rndGen.floatValue(), uvRotationMin, uvRotationMax);
	const float uScale = Lerp(rndGen.floatValue(), uScaleMin, uScaleMax);
	const float vScale = uniformScale ? uScale : Lerp(rndGen.floatValue(), vScaleMin, vScaleMax);
	const float uDelta = Lerp(rndGen.floatValue(), uDeltaMin, uDeltaMax);
	const float vDelta = Lerp(rndGen.floatValue(), vDeltaMin, vDeltaMax);

	// Get the hit point UV
	const UV uv = hitPoint.GetUV(dataIndex);
	
	// Scale
	const float uScaled = uv.u * uScale;
	const float vScaled = uv.v * vScale;

	// Rotate
	const float rad = Radians(-uvRotation);
	const float sinTheta = sinf(rad);
	const float cosTheta = cosf(rad);
	const float uRotated = uScaled * cosTheta - vScaled * sinTheta;
	const float vRotated = vScaled * cosTheta + uScaled * sinTheta;

	// Translate
	const float uTranslated = uRotated + uDelta;
	const float vTranslated = vRotated + vDelta;
	
	if (ds && dt) {
		const float signUScale = Sgn(uScale);
		const float signVScale = Sgn(vScale);

		*ds = UV(signUScale * cosTheta, signUScale * sinTheta);
		*dt = UV(-signVScale * sinTheta, signVScale * cosTheta);
	}
	
	return UV(uTranslated, vTranslated);
}

UV UVRandomMapping2D::Map(const HitPoint &hitPoint) const {
	return Map(hitPoint, nullptr, nullptr);
}

UV UVRandomMapping2D::MapDuv(const HitPoint &hitPoint, UV *ds, UV *dt) const {
	return Map(hitPoint, ds, dt);
}

Properties UVRandomMapping2D::ToProperties(const string &name) const {
	return Properties() <<
			Property(name + ".type")("uvrandommapping2d") <<
			TextureMapping2D::ToProperties(name) <<
			Property(name + ".seed.type")(SeedType2String(seedType)) <<
			Property(name + ".triangleaov.index")(triAOVIndex) <<
			Property(name + ".rotation")(uvRotationMin, uvRotationMax) <<
			Property(name + ".uvscale")(uScaleMin, uScaleMax, vScaleMin, uScaleMax) <<
			Property(name + ".uvscale.uniform")(uniformScale) <<
			Property(name + ".uvdelta")(uDeltaMin, uDeltaMax, vDeltaMin, vDeltaMax);
}

UVRandomMapping2D::SeedType UVRandomMapping2D::String2SeedType(const string &type) {
	if (type == "object_id")
		return OBJECT_ID;
	else if (type == "triangle_aov")
		return TRIANGLE_AOV;
	else
		throw runtime_error("Unknown seed type in UVRandomMapping2D::String2SeedType(): " + type);
}

string UVRandomMapping2D::SeedType2String(const SeedType type) {
	switch (type) {
		case OBJECT_ID:
			return "object_id";
		case TRIANGLE_AOV:
			return "triangle_aov";
		default:
			throw runtime_error("Unknown seed type in UVRandomMapping2D::SeedType2String(): " + ToString(type));
	}
}

//------------------------------------------------------------------------------
// UVMapping3D
//------------------------------------------------------------------------------

Point UVMapping3D::Map(const HitPoint &hitPoint, Normal *shadeN) const {
	if (shadeN)
		*shadeN = Normalize(worldToLocal * hitPoint.shadeN);

	const UV uv = hitPoint.GetUV(dataIndex);
	return worldToLocal * Point(uv.u, uv.v, 0.f);
}

Properties UVMapping3D::ToProperties(const string &name) const {
	Properties props;
	props.Set(Property(name + ".type")("uvmapping3d"));
	props.Set(Property(name + ".index")(dataIndex));
	props.Set(Property(name + ".transformation")(worldToLocal.m));

	return props;
}

//------------------------------------------------------------------------------
// GlobalMapping3D
//------------------------------------------------------------------------------

Point GlobalMapping3D::Map(const HitPoint &hitPoint, Normal *shadeN) const {
	if (shadeN)
		*shadeN = Normalize(worldToLocal * hitPoint.shadeN);

	return worldToLocal * hitPoint.p;
}

Properties GlobalMapping3D::ToProperties(const string &name) const {
	Properties props;
	props.Set(Property(name + ".type")("globalmapping3d"));
	props.Set(Property(name + ".transformation")(worldToLocal.m));

	return props;
}

//------------------------------------------------------------------------------
// LocalMapping3D
//------------------------------------------------------------------------------

Point LocalMapping3D::Map(const HitPoint &hitPoint, Normal *shadeN) const {
	const Transform w2t = worldToLocal / hitPoint.localToWorld;

	if (shadeN)
		*shadeN = Normalize(w2t * hitPoint.shadeN);

	return w2t * hitPoint.p;
}

Properties LocalMapping3D::ToProperties(const string &name) const {
	Properties props;
	props.Set(Property(name + ".type")("localmapping3d"));
	props.Set(Property(name + ".transformation")(worldToLocal.m));

	return props;
}
