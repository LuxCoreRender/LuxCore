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

#ifndef _SLG_SINC_FILTER_H
#define	_SLG_SINC_FILTER_H

#include "luxrays/utils/serializationutils.h"
#include "slg/film/filters/filter.h"

namespace slg {

//------------------------------------------------------------------------------
// SincFilter
//------------------------------------------------------------------------------

class SincFilter : public Filter {
public:
	// SincFilter Public Methods
	SincFilter(const float xw, const float yw, const float t) :
		Filter(xw, yw) {
		tau = t;
	}
	virtual ~SincFilter() { }

	virtual FilterType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	float Evaluate(const float x, const float y) const {
		return Sinc1D(x) * Sinc1D(y);
	}

	// Transform the current object in Properties
	virtual luxrays::Properties ToProperties() const;

	//--------------------------------------------------------------------------
	// Static methods used by FilterRegistry
	//--------------------------------------------------------------------------

	static FilterType GetObjectType() { return FILTER_SINC; }
	static std::string GetObjectTag() { return "SINC"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Filter *FromProperties(const luxrays::Properties &cfg);
	static slg::ocl::Filter *FromPropertiesOCL(const luxrays::Properties &cfg);

	float tau;

	friend class boost::serialization::access;

private:
	static const luxrays::Properties &GetDefaultProps();

	// Used by serialization
	SincFilter() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Filter);
		ar & tau;
	}

	// SincFilter Utility Functions
	float Sinc1D(float x) const {
		x = fabsf(x);

		if (x < 1e-5f)
			return 1.f;
		if (x > 1.)
			return 0.f;

		x *= M_PI;
		const float sinc = sinf(x * tau) / (x * tau);
		const float lanczos = sinf(x) / x;
		return sinc * lanczos;
	}
};

}

BOOST_CLASS_VERSION(slg::SincFilter, 1)

BOOST_CLASS_EXPORT_KEY(slg::SincFilter)

#endif	/* _SLG_SINC_FILTER_H */
