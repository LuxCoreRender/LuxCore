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
// MitchellSSFilter
//------------------------------------------------------------------------------

class MitchellSSFilter : public Filter {
public:
	// MitchelSSFilter Public Methods
	MitchellSSFilter(const float xw, const float yw,
			const float b, const float c) :
		Filter(xw * 5.f / 3.f, yw * 5.f / 3.f), B(b), C(c),
		a0(CalcA0(B, C)), a1(CalcA1(a0)) { }
	virtual ~MitchellSSFilter() { }

	virtual FilterType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	float Evaluate(const float x, const float y) const {
		const float distance = sqrtf(x * x * invXWidth * invXWidth +
			y * y * invYWidth * invYWidth);

		const float dist = distance / .6f;
		return a1 * Mitchell1D(dist - 2.f / 3.f) +
			a0 * Mitchell1D(dist) +
			a1 * Mitchell1D(dist + 2.f / 3.f);
	}

	// Transform the current object in Properties
	virtual luxrays::Properties ToProperties() const;

	//--------------------------------------------------------------------------
	// Static methods used by FilterRegistry
	//--------------------------------------------------------------------------

	static FilterType GetObjectType() { return FILTER_MITCHELL_SS; }
	static std::string GetObjectTag() { return "MITCHELL_SS"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Filter *FromProperties(const luxrays::Properties &cfg);
	static slg::ocl::Filter *FromPropertiesOCL(const luxrays::Properties &cfg);

	float B, C, a0, a1;

	friend class boost::serialization::access;

private:
	static float CalcA0(const float B, const float C) { return (76.f - 16.f * B + 8.f * C) / 81.f; }
	static float CalcA1(const float a0) { return (1.f - a0)/ 2.f; }

	static const luxrays::Properties &GetDefaultProps();

	// Used by serialization
	MitchellSSFilter() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Filter);
		ar & B;
		ar & C;
		ar & a0;
		ar & a1;
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
};

}

BOOST_CLASS_VERSION(slg::MitchellSSFilter, 2)

BOOST_CLASS_EXPORT_KEY(slg::MitchellSSFilter)

#endif	/* _SLG_MITCHELLSS_FILTER_H */
