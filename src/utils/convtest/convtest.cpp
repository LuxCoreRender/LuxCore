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

#include <cstdio>
#include <math.h>
#include <algorithm>

#include "luxrays/utils/convtest/convtest.h"
#include "luxrays/utils/convtest/pdiff/metric.h"

using namespace luxrays;
using namespace luxrays::utils;

//------------------------------------------------------------------------------
// ConvergenceTest class
//------------------------------------------------------------------------------

ConvergenceTest::ConvergenceTest(const unsigned int w, const unsigned int h) :
				width(w), height(h), reference(NULL), tvi(NULL) {
}

ConvergenceTest::~ConvergenceTest() {
	delete[] reference;
	delete[] tvi;
}

void ConvergenceTest::NeedTVI() {
	delete[] tvi;

	unsigned int nPix = width * height;
	tvi = new float[nPix];
	std::fill(tvi, tvi + nPix, 0.f);
}

void ConvergenceTest::Reset() {
	delete[] reference;
	reference = NULL;
	
}

void ConvergenceTest::Reset(const unsigned int w, const unsigned int h) {
	width = w;
	height = h;
	delete[] reference;
	reference = NULL;
	
}

unsigned int ConvergenceTest::Test(const float *image) {
	const unsigned int pixelCount = width * height;

	if (reference == NULL) {
		reference = new float[pixelCount * 3];
		std::copy(image, image + pixelCount * 3, reference);
		return pixelCount;
	} else {
		const unsigned int count = Yee_Compare(reference, image, NULL, tvi, width, height);
		std::copy(image, image + pixelCount * 3, reference);
		return count;
	}
}
