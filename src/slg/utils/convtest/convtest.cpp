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

#include <cstdio>
#include <cmath>
#include <algorithm>

#include "slg/utils/convtest/convtest.h"
#include "slg/utils/convtest/pdiff/metric.h"

using namespace slg;

//------------------------------------------------------------------------------
// ConvergenceTest class
//------------------------------------------------------------------------------

ConvergenceTest::ConvergenceTest(const u_int w, const u_int h) : width(w), height(h) {
}

ConvergenceTest::~ConvergenceTest() {
}

void ConvergenceTest::NeedTVI() {
	tvi.resize(width * height, 0.f);
}

void ConvergenceTest::Reset() {
	reference.resize(0);
}

void ConvergenceTest::Reset(const u_int w, const u_int h) {
	width = w;
	height = h;
	reference.resize(0);	
}

u_int ConvergenceTest::Test(const float *image) {
	const u_int pixelCount = width * height;

	if (reference.size() == 0) {
		reference.resize(pixelCount * 3);
		std::copy(image, image + pixelCount * 3, reference.begin());
		return pixelCount;
	} else {
		const u_int count = Yee_Compare(&reference[0], image, NULL, &tvi[0], width, height);
		std::copy(image, image + pixelCount * 3, reference.begin());
		return count;
	}
}
