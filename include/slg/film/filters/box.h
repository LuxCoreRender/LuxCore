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

#ifndef _SLG_BOX_FILTER_H
#define	_SLG_BOX_FILTER_H

#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/vector.hpp>

#include "slg/film/filters/filter.h"

namespace slg {
	
//------------------------------------------------------------------------------
// BoxFilter
//------------------------------------------------------------------------------

class BoxFilter : public Filter {
public:
	// BoxFilter Public Methods
	BoxFilter(const float xw = 2.f, const float yw = 2.f) :
		Filter(xw, yw) { }
	virtual ~BoxFilter() { }

	virtual FilterType GetType() const { return FILTER_BOX; }

	float Evaluate(const float x, const float y) const {
		return 1.f;
	}

	virtual Filter *Clone() const { return new BoxFilter(xWidth, yWidth); }

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Filter);
	}
};

}


BOOST_CLASS_VERSION(slg::BoxFilter, 1)

BOOST_CLASS_EXPORT_KEY(slg::BoxFilter)

#endif	/* _SLG_BOX_FILTER_H */
