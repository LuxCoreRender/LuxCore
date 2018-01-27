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

#ifndef _SLG_NONE_FILTER_H
#define	_SLG_NONE_FILTER_H

#include "luxrays/utils/serializationutils.h"
#include "slg/film/filters/filter.h"

namespace slg {
	
//------------------------------------------------------------------------------
// NoneFilter
//------------------------------------------------------------------------------

class NoneFilter : public Filter {
public:
	// NoneFilter Public Methods
	NoneFilter() :
		Filter(.5f, .5f) { }
	virtual ~NoneFilter() { }

	virtual FilterType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	float Evaluate(const float x, const float y) const {
		return 1.f;
	}

	//--------------------------------------------------------------------------
	// Static methods used by FilterRegistry
	//--------------------------------------------------------------------------

	static FilterType GetObjectType() { return FILTER_NONE; }
	static std::string GetObjectTag() { return "NONE"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static Filter *FromProperties(const luxrays::Properties &cfg);
	static slg::ocl::Filter *FromPropertiesOCL(const luxrays::Properties &cfg);

	friend class boost::serialization::access;

private:
	static const luxrays::Properties &GetDefaultProps();

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(Filter);
	}
};

}

BOOST_CLASS_VERSION(slg::NoneFilter, 2)

BOOST_CLASS_EXPORT_KEY(slg::NoneFilter)

#endif	/* _SLG_NONE_FILTER_H */
