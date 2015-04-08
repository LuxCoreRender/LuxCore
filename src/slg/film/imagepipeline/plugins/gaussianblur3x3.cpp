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

#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/gaussianblur3x3.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// GaussianBlur filter plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::GaussianBlur3x3FilterPlugin)

ImagePipelinePlugin *GaussianBlur3x3FilterPlugin::Copy() const {
	return new GaussianBlur3x3FilterPlugin(weight);
}

void GaussianBlur3x3FilterPlugin::ApplyBlurFilterXR1(
		const u_int filmWidth, const u_int filmHeight,
		const Spectrum *src,
		Spectrum *dst,
		const float aF,
		const float bF,
		const float cF) const {
	// Do left edge
	Spectrum a;
	Spectrum b = src[0];
	Spectrum c = src[1];

	const float leftTotF = bF + cF;
	const Spectrum bLeftK = bF / leftTotF;
	const Spectrum cLeftK = cF / leftTotF;
	dst[0] = bLeftK  * b + cLeftK * c;

    // Main loop
	const float totF = aF + bF + cF;
	const Spectrum aK = aF / totF;
	const Spectrum bK = bF / totF;
	const Spectrum cK = cF / totF;

	for (unsigned int x = 1; x < filmWidth - 1; ++x) {
		a = b;
		b = c;
		c = src[x + 1];

		dst[x] = aK * a + bK  * b + cK * c;
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const Spectrum aRightK = aF / rightTotF;
	const Spectrum bRightK = bF / rightTotF;
	a = b;
	b = c;
	dst[filmWidth - 1] = aRightK  * a + bRightK * b;
}

void GaussianBlur3x3FilterPlugin::ApplyBlurFilterYR1(
		const u_int filmWidth, const u_int filmHeight,
		const Spectrum *src,
		Spectrum *dst,
		const float aF,
		const float bF,
		const float cF) const {
	// Do left edge
	Spectrum a;
	Spectrum b = src[0];
	Spectrum c = src[filmWidth];

	const float leftTotF = bF + cF;
	const Spectrum bLeftK = bF / leftTotF;
	const Spectrum cLeftK = cF / leftTotF;
	dst[0] = bLeftK  * b + cLeftK * c;

    // Main loop
	const float totF = aF + bF + cF;
	const Spectrum aK = aF / totF;
	const Spectrum bK = bF / totF;
	const Spectrum cK = cF / totF;

    for (unsigned int y = 1; y < filmHeight - 1; ++y) {
		const unsigned index = y * filmWidth;

		a = b;
		b = c;
		c = src[index + filmWidth];

		dst[index] = aK * a + bK  * b + cK * c;
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const Spectrum aRightK = aF / rightTotF;
	const Spectrum bRightK = bF / rightTotF;
	a = b;
	b = c;
	dst[(filmHeight - 1) * filmWidth] = aRightK  * a + bRightK * b;
}

void GaussianBlur3x3FilterPlugin::ApplyGaussianBlurFilterXR1(
		const u_int filmWidth, const u_int filmHeight,
		const Spectrum *src, Spectrum *dst) const {
	const float aF = weight;
	const float bF = 1.f;
	const float cF = weight;

	ApplyBlurFilterXR1(filmWidth, filmHeight, src, dst, aF, bF, cF);
}

void GaussianBlur3x3FilterPlugin::ApplyGaussianBlurFilterYR1(
		const u_int filmWidth, const u_int filmHeight,
		const Spectrum *src, Spectrum *dst) const {
	const float aF = weight;
	const float bF = 1.f;
	const float cF = weight;

	ApplyBlurFilterYR1(filmWidth, filmHeight, src, dst, aF, bF, cF);
}

void GaussianBlur3x3FilterPlugin::Apply(const Film &film, Spectrum *pixels, vector<bool> &pixelsMask) const {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	// Allocate the temporary buffer if required
	if ((!tmpBuffer) || (width * height != tmpBufferSize)) {
		delete tmpBuffer;

		tmpBufferSize = width * height;
		tmpBuffer = new Spectrum[tmpBufferSize];
	}

	for (u_int i = 0; i < 3; ++i) {
		for (u_int y = 0; y < height; ++y) {
			const u_int index = y * width;
			ApplyGaussianBlurFilterXR1(width, height, &pixels[index], &tmpBuffer[index]);
		}

		for (u_int x = 0; x < width; ++x)
			ApplyGaussianBlurFilterYR1(width, height, &tmpBuffer[x], &pixels[x]);
	}
}
