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

#ifndef _SLG_GAUSSIAN_FILTER_H
#define	_SLG_GAUSSIAN_FILTER_H

#include "luxrays/utils/serializationutils.h"
#include "slg/film/filters/filter.h"

namespace slg {

//------------------------------------------------------------------------------
// GaussianFilter
//------------------------------------------------------------------------------

class GaussianFilter : public Filter {
public:
	// GaussianFilter Public Methods
	GaussianFilter(const float xw, const float yw, const float a) :
		Filter(xw, yw) {
		alpha = a;
		expX = expf(-alpha * xWidth * xWidth);
		expY = expf(-alpha * yWidth * yWidth);
	}
	virtual ~GaussianFilter() { }

	virtual FilterType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	float Evaluate(const float x, const float y) const {
		return Gaussian(x, expX) * Gaussian(y, expY);
	}

	// Transform the current object in Properties
	virtual luxrays::Properties ToProperties() const;

	//--------------------------------------------------------------------------
	// Static methods used by FilterRegistry
	//--------------------------------------------------------------------------

	static FilterType GetObjectType() { return FILTER_GAUSSIAN; }
	static std::string GetObjectTag() { return "GAUSSIAN"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Filter *FromProperties(const luxrays::Properties &cfg);
	static slg::ocl::Filter *FromPropertiesOCL(const luxrays::Properties &cfg);

	float alpha;

	friend class boost::serialization::access;

private:
	static const luxrays::Properties &GetDefaultProps();

	// Used by serialization
	GaussianFilter() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Filter);
		ar & alpha;
		ar & expX;
		ar & expY;
	}

	// GaussianFilter Private Data
	float expX, expY;

	// GaussianFilter Utility Functions
	float Gaussian(float d, float expv) const {
		return luxrays::Max(0.f, expf(-alpha * d * d) - expv);
	}
};

}

BOOST_CLASS_VERSION(slg::GaussianFilter, 1)

BOOST_CLASS_EXPORT_KEY(slg::GaussianFilter)

#endif	/* _SLG_GAUSSIAN_FILTER_H */
