/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_BLACKMANNHARRIS_FILTER_H
#define	_SLG_BLACKMANNHARRIS_FILTER_H

#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/vector.hpp>

#include "slg/film/filters/filter.h"

namespace slg {

//------------------------------------------------------------------------------
// BlackmanHarrisFilter
//------------------------------------------------------------------------------

class BlackmanHarrisFilter : public Filter {
public:
	// GaussianFilter Public Methods
	BlackmanHarrisFilter(const float xw = 2.f, const float yw = 2.f) :
		Filter(xw, yw) { }
	virtual ~BlackmanHarrisFilter() { }

	virtual FilterType GetType() const { return FILTER_BLACKMANHARRIS; }

	float Evaluate(const float x, const float y) const {
		return BlackmanHarris1D(x * invXWidth) * BlackmanHarris1D(y *  invYWidth);
	}

	virtual Filter *Clone() const { return new BlackmanHarrisFilter(xWidth, yWidth); }

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Filter);
	}

	float BlackmanHarris1D(float x) const {
		if (x < -1.f || x > 1.f)
			return 0.f;
		x = (x + 1.f) * .5f;
		x *= M_PI;
		const float A0 =  0.35875f;
		const float A1 = -0.48829f;
		const float A2 =  0.14128f;
		const float A3 = -0.01168f;
		return A0 + A1 * cosf(2.f * x) + A2 * cosf(4.f * x) + A3 * cosf(6.f * x);
	}
};

}

BOOST_CLASS_VERSION(slg::BlackmanHarrisFilter, 1)

BOOST_CLASS_EXPORT_KEY(slg::BlackmanHarrisFilter)

#endif	/* _SLG_BLACKMANNHARRIS_FILTER_H */
