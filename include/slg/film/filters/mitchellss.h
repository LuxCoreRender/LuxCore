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

#ifndef _SLG_MITCHELLSS_FILTER_H
#define	_SLG_MITCHELLSS_FILTER_H

#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/vector.hpp>

#include "slg/film/filters/filter.h"

namespace slg {

//------------------------------------------------------------------------------
// MitchellFilterSS
//------------------------------------------------------------------------------

class MitchellFilterSS : public Filter {
public:
	// GaussianFilter Public Methods
	MitchellFilterSS(const float xw = 2.f, const float yw = 2.f,
			const float b = 1.f / 3.f, const float c = 1.f / 3.f) :
		Filter(xw * 5.f / 3.f, yw * 5.f / 3.f), B(b), C(c),
		a0((76.f - 16.f * B + 8.f * C) / 81.f), a1((1.f - a0)/ 2.f) { }
	virtual ~MitchellFilterSS() { }

	virtual FilterType GetType() const { return FILTER_MITCHELL; }

	float Evaluate(const float x, const float y) const {
		const float distance = sqrtf(x * x * invXWidth * invXWidth +
			y * y * invYWidth * invYWidth);

		const float dist = distance / .6f;
		return a1 * Mitchell1D(dist - 2.f / 3.f) +
			a0 * Mitchell1D(dist) +
			a1 * Mitchell1D(dist + 2.f / 3.f);
	}

	virtual Filter *Clone() const { return new MitchellFilterSS(xWidth, yWidth, B, C); }

	float B, C;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Filter);
		ar & B;
		ar & C;
	}

	float Mitchell1D(float x) const {
		if (x >= 1.f)
			return 0.f;
		x = fabsf(2.f * x);
		if (x > 1.f)
			return (((-B / 6.f - C) * x + (B + 5.f * C)) * x +
				(-2.f * B - 8.f * C)) * x + (4.f / 3.f * B + 4.f * C);
		else
			return ((2.f - 1.5f * B - C) * x +
				(-3.f + 2.f * B + C)) * x * x +
				(1.f - B / 3.f);
	}

	const float a0, a1;
};

}

BOOST_CLASS_VERSION(slg::MitchellFilterSS, 1)

BOOST_CLASS_EXPORT_KEY(slg::MitchellFilterSS)

#endif	/* _SLG_MITCHELLSS_FILTER_H */
