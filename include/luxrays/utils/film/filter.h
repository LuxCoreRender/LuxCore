/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _LUXRAYS_UTILS_FILTER_H
#define	_LUXRAYS_UTILS_FILTER_H

#include "luxrays/core/utils.h"

namespace luxrays { namespace utils {

//------------------------------------------------------------------------------
// Filters
//------------------------------------------------------------------------------

typedef enum {
	FILTER_NONE, FILTER_BOX, FILTER_GAUSSIAN, FILTER_MITCHELL, FILTER_MITCHELL_SS
} FilterType;

class Filter {
public:
	// Filter Interface
	Filter(const float xw, const float yw) : xWidth(xw), yWidth(yw),
			invXWidth(1.f / xw), invYWidth(1.f / yw) { }
	virtual ~Filter() { }

	virtual FilterType GetType() const = 0;
	virtual float Evaluate(const float x, const float y) const = 0;

	static FilterType String2FilterType(const std::string &type);

	// Filter Public Data
	const float xWidth, yWidth;
	const float invXWidth, invYWidth;
};

class BoxFilter : public Filter {
public:
	// GaussianFilter Public Methods
	BoxFilter(const float xw = 2.f, const float yw = 2.f) :
		Filter(xw, yw) {
	}
	virtual ~BoxFilter() { }

	virtual FilterType GetType() const { return FILTER_BOX; }

	float Evaluate(const float x, const float y) const {
		return 1.f;
	}
};

class GaussianFilter : public Filter {
public:
	// GaussianFilter Public Methods
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

private:
	// GaussianFilter Private Data
	float alpha;
	float expX, expY;

	// GaussianFilter Utility Functions
	float Gaussian(float d, float expv) const {
		return Max(0.f, expf(-alpha * d * d) - expv);
	}
};

class MitchellFilter : public Filter {
public:
	// GaussianFilter Public Methods
	MitchellFilter(const float xw = 2.f, const float yw = 2.f,
			const float b = 1.f / 3.f, const float c = 1.f / 3.f) :
		Filter(xw, yw), B(b), C(c),
		a0((76.f - 16.f * B + 8.f * C) / 81.f), a1((1.f - a0)/ 2.f) { }
	virtual ~MitchellFilter() { }

	virtual FilterType GetType() const { return FILTER_MITCHELL; }

	float Evaluate(const float x, const float y) const {
		const float distance = sqrtf(x * x * invXWidth * invXWidth +
			y * y * invYWidth * invYWidth);

		return Mitchell1D(distance);

	}

private:
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

	const float B, C, a0, a1;
};

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

private:
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

	const float B, C, a0, a1;
};

//------------------------------------------------------------------------------
// Filter Look Up Table
//------------------------------------------------------------------------------

class FilterLUT {
public:
	FilterLUT(const Filter &filter, const float offsetX, const float offsetY) {
		const int x0 = Ceil2Int(offsetX - filter.xWidth);
		const int x1 = Floor2Int(offsetX + filter.xWidth);
		const int y0 = Ceil2Int(offsetY - filter.yWidth);
		const int y1 = Floor2Int(offsetY + filter.yWidth);
		lutWidth = x1 - x0 + 1;
		lutHeight = y1 - y0 + 1;
		lut = new float[lutWidth * lutHeight];

		float filterNorm = 0.f;
		unsigned int index = 0;
		for (int iy = y0; iy <= y1; ++iy) {
			for (int ix = x0; ix <= x1; ++ix) {
				const float filterVal = filter.Evaluate(fabsf(ix - offsetX), fabsf(iy - offsetY));
				filterNorm += filterVal;
				lut[index++] = filterVal;
			}
		}

		// Normalize LUT
		filterNorm = 1.f / filterNorm;
		index = 0;
		for (int iy = y0; iy <= y1; ++iy) {
			for (int ix = x0; ix <= x1; ++ix)
				lut[index++] *= filterNorm;
		}
	}

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
	FilterLUTs(const Filter &filter, const unsigned int size) {
		lutsSize = size + 1;
		step = 1.f / float(size);

		luts = new FilterLUT*[lutsSize * lutsSize];

		for (unsigned int iy = 0; iy < lutsSize; ++iy) {
			for (unsigned int ix = 0; ix < lutsSize; ++ix) {
				const float x = ix * step - 0.5f + step / 2.f;
				const float y = iy * step - 0.5f + step / 2.f;

				luts[ix + iy * lutsSize] = new FilterLUT(filter, x, y);
				/*std::cout << "===============================================\n";
				std::cout << ix << "," << iy << "\n";
				std::cout << x << "," << y << "\n";
				std::cout << *luts[ix + iy * lutsSize] << "\n";
				std::cout << "===============================================\n";*/
			}
		}
	}

	~FilterLUTs() {
		for (unsigned int iy = 0; iy < lutsSize; ++iy)
			for (unsigned int ix = 0; ix < lutsSize; ++ix)
				delete luts[ix + iy * lutsSize];

		delete[] luts;
	}

	const FilterLUT *GetLUT(const float x, const float y) const {
		const int ix = Max<unsigned int>(0, Min<unsigned int>(Floor2Int(lutsSize * (x + 0.5f)), lutsSize - 1));
		const int iy = Max<unsigned int>(0, Min<unsigned int>(Floor2Int(lutsSize * (y + 0.5f)), lutsSize - 1));

		return luts[ix + iy * lutsSize];
	}

private:
	unsigned int lutsSize;
	float step;
	FilterLUT **luts;
};

} }

#endif	/* _LUXRAYS_UTILS_FILTER_H */
