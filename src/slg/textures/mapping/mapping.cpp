/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#include "slg/textures/mapping/mapping.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// UVMapping2D
//------------------------------------------------------------------------------

UV UVMapping2D::Map(const UV &uv) const {
	return UV(uv.u * uScale + uDelta, uv.v * vScale + vDelta);
}

UV UVMapping2D::MapDuv(const HitPoint &hitPoint,
	UV *ds, UV *dt) const {
	*ds = UV(uScale, 0.f);
	*dt = UV(0.f, vScale);
	return Map(hitPoint.uv);
}

Properties UVMapping2D::ToProperties(const std::string &name) const {
	Properties props;
	props.Set(Property(name + ".type")("uvmapping2d"));
	props.Set(Property(name + ".uvscale")(uScale, vScale));
	props.Set(Property(name + ".uvdelta")(uDelta, vDelta));

	return props;
}

//------------------------------------------------------------------------------
// UVMapping3D
//------------------------------------------------------------------------------

Point UVMapping3D::Map(const HitPoint &hitPoint) const {
	return worldToLocal * Point(hitPoint.uv.u, hitPoint.uv.v, 0.f);
}

Properties UVMapping3D::ToProperties(const std::string &name) const {
	Properties props;
	props.Set(Property(name + ".type")("uvmapping3d"));
	props.Set(Property(name + ".transformation")(worldToLocal.mInv));

	return props;
}

//------------------------------------------------------------------------------
// GlobalMapping3D
//------------------------------------------------------------------------------

Point GlobalMapping3D::Map(const HitPoint &hitPoint) const {
	return worldToLocal * hitPoint.p;
}

Properties GlobalMapping3D::ToProperties(const std::string &name) const {
	Properties props;
	props.Set(Property(name + ".type")("globalmapping3d"));
	props.Set(Property(name + ".transformation")(worldToLocal.mInv));

	return props;
}

//------------------------------------------------------------------------------
// LocalMapping3D
//------------------------------------------------------------------------------

Point LocalMapping3D::Map(const HitPoint &hitPoint) const {
	const Transform w2t = worldToLocal / hitPoint.local2World;

	return w2t * hitPoint.p;
}

Properties LocalMapping3D::ToProperties(const std::string &name) const {
	Properties props;
	props.Set(Property(name + ".type")("localmapping3d"));
	props.Set(Property(name + ".transformation")(worldToLocal.mInv));

	return props;
}
