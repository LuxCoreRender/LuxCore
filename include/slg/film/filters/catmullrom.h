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

#ifndef _SLG_CATMULLROM_FILTER_H
#define	_SLG_CATMULLROM_FILTER_H

#include "luxrays/utils/serializationutils.h"
#include "slg/film/filters/filter.h"

namespace slg {

//------------------------------------------------------------------------------
// CatmullRomFilter
//------------------------------------------------------------------------------

class CatmullRomFilter : public Filter {
public:
	// CatmullRomFilter Public Methods
	CatmullRomFilter(const float xw, const float yw) :
		Filter(xw, yw) {
	}
	virtual ~CatmullRomFilter() { }

	virtual FilterType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	float Evaluate(const float x, const float y) const {
		return CatmullRom1D(x) * CatmullRom1D(y);
	}

	// Transform the current object in Properties
	virtual luxrays::Properties ToProperties() const;

	//--------------------------------------------------------------------------
	// Static methods used by FilterRegistry
	//--------------------------------------------------------------------------

	static FilterType GetObjectType() { return FILTER_CATMULLROM; }
	static std::string GetObjectTag() { return "CATMULLROM"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Filter *FromProperties(const luxrays::Properties &cfg);
	static slg::ocl::Filter *FromPropertiesOCL(const luxrays::Properties &cfg);

	float alpha;

	friend class boost::serialization::access;

private:
	static const luxrays::Properties &GetDefaultProps();

	// Used by serialization
	CatmullRomFilter() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Filter);
	}

	// CatmullRomFilter Utility Functions
	float CatmullRom1D(float x) const {
		x = fabsf(x);
		float x2 = x * x;

		return (x >= 2.f) ? 0.f : ((x < 1.f) ?
				(3.f * (x * x2) - 5.f * x2 + 2.f) :
				(-(x * x2) + 5.f * x2 - 8.f * x + 4.f));
	}
};

}

BOOST_CLASS_VERSION(slg::CatmullRomFilter, 1)

BOOST_CLASS_EXPORT_KEY(slg::CatmullRomFilter)

#endif	/* _SLG_CATMULLROM_FILTER_H */
