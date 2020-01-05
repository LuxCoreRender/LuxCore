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

#include <math.h>

#include "slg/textures/mapping/mapping.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// UVMapping2D
//------------------------------------------------------------------------------

Properties TextureMapping2D::ToProperties(const std::string &name) const {
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
		vScale(vscale), uDelta(udelta), vDelta(vdelta) {
	sinTheta = sinf(Radians(-uvRotation));
	cosTheta = cosf(Radians(-uvRotation));
}

UV UVMapping2D::Map(const UV &uv) const {
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

	return Map(hitPoint.uv[dataIndex]);
}

Properties UVMapping2D::ToProperties(const std::string &name) const {
	return Properties() <<
			Property(name + ".type")("uvmapping2d") <<
			TextureMapping2D::ToProperties(name) <<
			Property(name + ".rotation")(uvRotation) <<
			Property(name + ".uvscale")(uScale, vScale) <<
			Property(name + ".uvdelta")(uDelta, vDelta);
}

//------------------------------------------------------------------------------
// UVMapping3D
//------------------------------------------------------------------------------

Point UVMapping3D::Map(const HitPoint &hitPoint, Normal *shadeN) const {
	if (shadeN)
		*shadeN = Normalize(worldToLocal * hitPoint.shadeN);

	return worldToLocal * Point(hitPoint.uv[dataIndex].u, hitPoint.uv[dataIndex].v, 0.f);
}

Properties UVMapping3D::ToProperties(const std::string &name) const {
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

Properties GlobalMapping3D::ToProperties(const std::string &name) const {
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

Properties LocalMapping3D::ToProperties(const std::string &name) const {
	Properties props;
	props.Set(Property(name + ".type")("localmapping3d"));
	props.Set(Property(name + ".transformation")(worldToLocal.m));

	return props;
}
