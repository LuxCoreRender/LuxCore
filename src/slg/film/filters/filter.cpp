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

#include "slg/film/filters/filter.h"
#include "slg/film/filters/box.h"
#include "slg/film/filters/gaussian.h"
#include "slg/film/filters/mitchell.h"
#include "slg/film/filters/mitchellss.h"
#include "slg/film/filters/blackmanharris.h"

using namespace luxrays;
using namespace slg;

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BoxFilter)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::GaussianFilter)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::MitchellFilter)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::MitchellFilterSS)
BOOST_CLASS_EXPORT_IMPLEMENT(slg::BlackmanHarrisFilter)

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
	else if ((type.compare("5") == 0) || (type.compare("BLACKMANHARRIS") == 0))
		return FILTER_BLACKMANHARRIS;
	throw std::runtime_error("Unknown filter type: " + type);
}

//------------------------------------------------------------------------------
// PrecomputedFilter
//------------------------------------------------------------------------------

FilterDistribution::FilterDistribution(const Filter *f, const u_int s) {
	filter = f;
	size = s;
	distrib = NULL;

	float *data = new float[size * size];

	const float isize = 1.f / (float)size;
	for (u_int y = 0; y < size; ++y) {
		for (u_int x = 0; x < size; ++x) {
			// Use an uniform distribution if the Filter is missing
			data[x + y * size] = (filter) ? filter->Evaluate(
					2.f * filter->xWidth * ((x + .5f) * isize - .5),
					2.f * filter->yWidth * ((y + .5f) * isize - .5)) : 1.f;
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

		*su0 = (uv[0] - .5f) * (2.f * filter->xWidth);
		*su1 = (uv[1] - .5f) * (2.f * filter->yWidth);
	} else {
		*su0 = u0 - .5f;
		*su1 = u1 - .5f;
	}
}

//------------------------------------------------------------------------------
// FilterLUT
//------------------------------------------------------------------------------

FilterLUT::FilterLUT(const Filter &filter, const float offsetX, const float offsetY) {
	const int x0 = luxrays::Floor2Int(offsetX - filter.xWidth * .5f + .5f);
	const int x1 = luxrays::Floor2Int(offsetX + filter.xWidth * .5f + .5f);
	const int y0 = luxrays::Floor2Int(offsetY - filter.yWidth * .5f + .5f);
	const int y1 = luxrays::Floor2Int(offsetY + filter.yWidth * .5f + .5f);
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
			const float x = (ix + .5f) * step - 0.5f;
			const float y = (iy + .5f) * step - 0.5f;

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
