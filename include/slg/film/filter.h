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
#include "slg/film/filter_types.cl"
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

//------------------------------------------------------------------------------
// GaussianFilter
//------------------------------------------------------------------------------

class GaussianFilter : public Filter {
public:
	// MitchellFilter Public Methods
	GaussianFilter(const float xw = 2.f, const float yw = 2.f, const float a = 2.f) :
		Filter(xw, yw) {
		alpha = a;
		expX = expf(-alpha * xWidth * xWidth);
		expY = expf(-alpha * yWidth * yWidth);
	}
	virtual ~GaussianFilter() { }

	virtual FilterType GetType() const { return FILTER_GAUSSIAN; }

	float Evaluate(const float x, const float y) const {
		return Gaussian(x, expX) * Gaussian(y, expY);
	}

	virtual Filter *Clone() const { return new GaussianFilter(xWidth, yWidth, alpha); }

	float alpha;

	friend class boost::serialization::access;

private:
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

//------------------------------------------------------------------------------
// MitchellFilter
//------------------------------------------------------------------------------

class MitchellFilter : public Filter {
public:
	// MitchellFilter Public Methods
	MitchellFilter(const float xw = 2.f, const float yw = 2.f,
			const float b = 1.f / 3.f, const float c = 1.f / 3.f) :
		Filter(xw, yw), B(b), C(c) { }
	virtual ~MitchellFilter() { }

	virtual FilterType GetType() const { return FILTER_MITCHELL; }

	float Evaluate(const float x, const float y) const {
		const float distance = sqrtf(x * x * invXWidth * invXWidth +
			y * y * invYWidth * invYWidth);

		return Mitchell1D(distance);

	}

	virtual Filter *Clone() const { return new MitchellFilter(xWidth, yWidth, B, C); }

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
};

//------------------------------------------------------------------------------
// MitchellFilter
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
BOOST_CLASS_VERSION(slg::BoxFilter, 1)
BOOST_CLASS_VERSION(slg::GaussianFilter, 1)
BOOST_CLASS_VERSION(slg::MitchellFilter, 1)
BOOST_CLASS_VERSION(slg::MitchellFilterSS, 1)
BOOST_CLASS_VERSION(slg::BlackmanHarrisFilter, 1)

BOOST_CLASS_EXPORT_KEY(slg::BoxFilter)
BOOST_CLASS_EXPORT_KEY(slg::GaussianFilter)
BOOST_CLASS_EXPORT_KEY(slg::MitchellFilter)
BOOST_CLASS_EXPORT_KEY(slg::MitchellFilterSS)
BOOST_CLASS_EXPORT_KEY(slg::BlackmanHarrisFilter)

#endif	/* _SLG_FILTER_H */
