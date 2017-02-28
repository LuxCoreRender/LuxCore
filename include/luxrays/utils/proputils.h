/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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


#ifndef _LUXRAYS_PROPUTILS_H
#define	_LUXRAYS_PROPUTILS_H

#include "luxrays/luxrays.h"
#include "luxrays/utils/properties.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/matrix4x4.h"
#include "luxrays/core/color/color.h"

namespace luxrays {

// Get LuxRays types
template<> UV Property::Get<UV>() const;
template<> Vector Property::Get<Vector>() const;
template<> Normal Property::Get<Normal>() const;
template<> Point Property::Get<Point>() const;
template<> Spectrum Property::Get<Spectrum>() const;
template<> Matrix4x4 Property::Get<Matrix4x4>() const;

// Add LuxRays types
template<> Property &Property::Add<UV>(const UV &val);
template<> Property &Property::Add<Vector>(const Vector &val);
template<> Property &Property::Add<Normal>(const Normal &val);
template<> Property &Property::Add<Point>(const Point &val);
template<> Property &Property::Add<Spectrum>(const Spectrum &val);
template<> Property &Property::Add<Matrix4x4>(const Matrix4x4 &val);

}

#endif	/* _LUXRAYS_PROPUTILS_H */
