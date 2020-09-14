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

#ifndef _SLG_FILTER_H
#define	_SLG_FILTER_H

#include "luxrays/core/color/color.h"
#include "luxrays/utils/utils.h"
#include "luxrays/utils/mc.h"
#include "luxrays/utils/mcdistribution.h"
#include "luxrays/core/namedobject.h"

namespace slg {

//------------------------------------------------------------------------------
// OpenCL data types
//------------------------------------------------------------------------------

namespace ocl {
using luxrays::ocl::Spectrum;
#include "slg/film/filters/filter_types.cl"
} 

//------------------------------------------------------------------------------
// Filters
//------------------------------------------------------------------------------

typedef enum {
	FILTER_NONE, FILTER_BOX, FILTER_GAUSSIAN, FILTER_MITCHELL, FILTER_MITCHELL_SS,
	FILTER_BLACKMANHARRIS, FILTER_SINC, FILTER_CATMULLROM,
	FILTER_TYPE_COUNT
} FilterType;

class Filter : public luxrays::NamedObject {
public:
	// Filter Interface
	Filter(const float xw, const float yw) : NamedObject("pixelfilter"),
			xWidth(xw), yWidth(yw),
			invXWidth(1.f / xw), invYWidth(1.f / yw) { }
	virtual ~Filter() { }

	virtual FilterType GetType() const = 0;
	virtual std::string GetTag() const = 0;
	virtual float Evaluate(const float x, const float y) const = 0;

	// Transform the current object in Properties
	virtual luxrays::Properties ToProperties() const;

	//--------------------------------------------------------------------------
	// Static methods used by ObjectRegistry
	//--------------------------------------------------------------------------

	// Transform the current configuration Properties in a complete list of
	// object Properties (including all defaults values)
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	// Allocate a Object based on the cfg definition
	static Filter *FromProperties(const luxrays::Properties &cfg);
	static slg::ocl::Filter *FromPropertiesOCL(const luxrays::Properties &cfg);

	static FilterType String2FilterType(const std::string &type);
	static const std::string FilterType2String(const FilterType type);

	// Filter Public Data
	float xWidth, yWidth;
	float invXWidth, invYWidth;

	friend class boost::serialization::access;

protected:
	static const luxrays::Properties &GetDefaultProps();

	// Used by serialization
	Filter() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(NamedObject);
		ar & xWidth;
		ar & yWidth;
		ar & invXWidth;
		ar & invYWidth;
	}
};

}

BOOST_SERIALIZATION_ASSUME_ABSTRACT(slg::Filter)

BOOST_CLASS_VERSION(slg::Filter, 3)

#endif	/* _SLG_FILTER_H */
