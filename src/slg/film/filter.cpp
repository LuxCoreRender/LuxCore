/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#include "slg/film/filter.h"

using namespace luxrays;
using namespace slg;

FilterType Filter::String2FilterType(const std::string &type) {
	if ((type.compare("0") == 0) || (type.compare("NONE") == 0))
		return FILTER_NONE;
	else if ((type.compare("1") == 0) || (type.compare("BOX") == 0))
		return FILTER_BOX;
	else if ((type.compare("2") == 0) || (type.compare("GAUSSIAN") == 0))
		return FILTER_GAUSSIAN;
	else if ((type.compare("3") == 0) || (type.compare("MITCHELL") == 0))
		return FILTER_MITCHELL;
	else if ((type.compare("4") == 0) || (type.compare("MITCHELL_SS") == 0))
		return FILTER_MITCHELL_SS;
	throw std::runtime_error("Unknown filter type: " + type);
}

//------------------------------------------------------------------------------
// PrecomputedFilter
//------------------------------------------------------------------------------

FilterDistribution::FilterDistribution(const Filter *f, const u_int s) {
	filter = f;
	size = s;
	distrib = NULL;

	if (!filter)
		return;

	float *data = new float[size * size];

	const float isize = 1.f / (float)size;
	for (u_int y = 0; y < size; ++y) {
		for (u_int x = 0; x < size; ++x) {
			data[x + y * size] = filter->Evaluate(
					filter->xWidth *((x + .5f) * isize - .5),
					filter->yWidth *((y + .5f) * isize - .5));
		}
	}

	distrib = new Distribution2D(data, size, size);
	delete data;
}

FilterDistribution::~FilterDistribution() {
	delete distrib;
}
void FilterDistribution::SampleContinuous(const float u0, const float u1, float *su0, float *su1) const {
	if (filter) {
		float uv[2];
		float pdf;
		distrib->SampleContinuous(u0, u1, uv, &pdf);

		*su0 = (uv[0] - .5f) * filter->xWidth;
		*su1 = (uv[1] - .5f) * filter->yWidth;
	} else {
		*su0 = (u0 - .5f) * filter->xWidth;
		*su1 = (u1 - .5f) * filter->yWidth;
	}
}

//------------------------------------------------------------------------------
// FilterLUT
//------------------------------------------------------------------------------

FilterLUT::FilterLUT(const Filter &filter, const float offsetX, const float offsetY) {
	const int x0 = luxrays::Ceil2Int(offsetX - filter.xWidth);
	const int x1 = luxrays::Floor2Int(offsetX + filter.xWidth);
	const int y0 = luxrays::Ceil2Int(offsetY - filter.yWidth);
	const int y1 = luxrays::Floor2Int(offsetY + filter.yWidth);
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

//------------------------------------------------------------------------------
// FilterLUTs
//------------------------------------------------------------------------------

FilterLUTs::FilterLUTs(const Filter &filter, const unsigned int size) {
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

FilterLUTs::~FilterLUTs() {
	for (unsigned int iy = 0; iy < lutsSize; ++iy)
		for (unsigned int ix = 0; ix < lutsSize; ++ix)
			delete luts[ix + iy * lutsSize];

	delete[] luts;
}