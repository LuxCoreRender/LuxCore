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

#ifndef _SLG_FILTER_H
#define	_SLG_FILTER_H

#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/vector.hpp>

#include "luxrays/core/color/color.h"
#include "luxrays/core/utils.h"
#include "luxrays/utils/mc.h"
#include "luxrays/utils/mcdistribution.h"

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
	FILTER_BLACKMANHARRIS
} FilterType;

class Filter {
public:
	// Filter Interface
	Filter(const float xw, const float yw) : xWidth(xw), yWidth(yw),
			invXWidth(1.f / xw), invYWidth(1.f / yw) { }
	virtual ~Filter() { }

	virtual FilterType GetType() const = 0;
	virtual float Evaluate(const float x, const float y) const = 0;

	virtual Filter *Clone() const = 0;

	static FilterType String2FilterType(const std::string &type);

	// Filter Public Data
	float xWidth, yWidth;
	float invXWidth, invYWidth;

	friend class boost::serialization::access;

private:
	// Used by serialization
	Filter() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & xWidth;
		ar & yWidth;
		ar & invXWidth;
		ar & invYWidth;
	}
};

class FilterDistribution {
public:
	FilterDistribution(const Filter *filter, const u_int size);
	~FilterDistribution();

	void SampleContinuous(const float u0, const float u1, float *su0, float *su1) const;

	const luxrays::Distribution2D *GetDistribution2D() const { return distrib; }

private:
	const Filter *filter;
	u_int size;

	luxrays::Distribution2D *distrib;
};

//------------------------------------------------------------------------------
// Filter Look Up Table
//------------------------------------------------------------------------------

class FilterLUT {
public:
	FilterLUT(const Filter &filter, const float offsetX, const float offsetY);
	~FilterLUT() {
		delete[] lut;
	}

	const unsigned int GetWidth() const { return lutWidth; }
	const unsigned int GetHeight() const { return lutHeight; }

	const float *GetLUT() const {
		return lut;
	}

	friend std::ostream &operator<<(std::ostream &os, const FilterLUT &f);

private:
	unsigned int lutWidth, lutHeight;
	float *lut;
};

inline std::ostream &operator<<(std::ostream &os, const FilterLUT &f) {
	os << "FilterLUT[(" << f.lutWidth << "x" << f.lutHeight << "),";

	os << "[";
	for (unsigned int iy = 0; iy < f.lutHeight; ++iy) {
		os << "[";
		for (unsigned int ix = 0; ix < f.lutWidth; ++ix) {
			os << f.lut[ix + iy * f.lutWidth];
			if (ix != f.lutWidth - 1)
				os << ",";
		}
		os << "]";
	}
	os << "]";

	return os;
}

class FilterLUTs {
public:
	FilterLUTs(const Filter &filter, const unsigned int size);
	~FilterLUTs() ;

	const FilterLUT *GetLUT(const float x, const float y) const {
		const int ix = luxrays::Max<unsigned int>(0, luxrays::Min<unsigned int>(luxrays::Floor2Int(lutsSize * (x + 0.5f)), lutsSize - 1));
		const int iy = luxrays::Max<unsigned int>(0, luxrays::Min<unsigned int>(luxrays::Floor2Int(lutsSize * (y + 0.5f)), lutsSize - 1));

		return luts[ix + iy * lutsSize];
	}

private:
	unsigned int lutsSize;
	float step;
	FilterLUT **luts;
};

}

BOOST_SERIALIZATION_ASSUME_ABSTRACT(slg::Filter)

BOOST_CLASS_VERSION(slg::Filter, 1)

#endif	/* _SLG_FILTER_H */
