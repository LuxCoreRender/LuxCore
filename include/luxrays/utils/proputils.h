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


#ifndef _LUXRAYS_PROPUTILS_H
#define	_LUXRAYS_PROPUTILS_H

#include "luxrays/luxrays.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/serializationutils.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/matrix4x4.h"
#include "luxrays/core/color/color.h"

//------------------------------------------------------------------------------
// LuxRays data types related methods
//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------
// Serialization
//------------------------------------------------------------------------------

BOOST_SERIALIZATION_SPLIT_FREE(luxrays::Property)
namespace boost {
namespace serialization {

template<class Archive>
void save(Archive &ar, const luxrays::Property &prop, const unsigned int version) {
	const std::string s = prop.ToString();
	ar << s;
}

template<class Archive>
void load(Archive &ar, luxrays::Property &prop, const unsigned int version) {
	std::string s;
	ar & s;

	prop.FromString(s);
}

}
}

BOOST_SERIALIZATION_SPLIT_FREE(luxrays::Properties)
namespace boost {
namespace serialization {

template<class Archive>
void save(Archive &ar, const luxrays::Properties &props, const unsigned int version) {
	const size_t count = props.GetSize();
	ar & count;

	const std::vector<std::string> &names = props.GetAllNames();
	for (size_t i = 0; i < count; ++i)
		ar << props.Get(names[i]);
}

template<class Archive>
void load(Archive &ar, luxrays::Properties &props, const unsigned int version) {
	size_t count;
	ar & count;

	for (size_t i = 0; i < count; ++i) {
		luxrays::Property p;
		ar & p;

		props << p;
	}
}

}
}

BOOST_CLASS_VERSION(luxrays::Property, 3)
BOOST_CLASS_VERSION(luxrays::Properties, 3)

#endif	/* _LUXRAYS_PROPUTILS_H */
