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

#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/bloom.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// GaussianBlur filter plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BloomFilterPlugin)

BloomFilterPlugin::BloomFilterPlugin(const float r, const float w) :
		radius(r), weight(w), bloomBuffer(NULL), bloomBufferTmp(NULL),
		bloomFilter(NULL), bloomBufferSize(0) {
}

BloomFilterPlugin::~BloomFilterPlugin() {
	delete[] bloomBuffer;
	delete[] bloomBufferTmp;
	delete[] bloomFilter;
}

ImagePipelinePlugin *BloomFilterPlugin::Copy() const {
	return new BloomFilterPlugin(radius, weight);
}

void BloomFilterPlugin::BloomFilterX(const Film &film, Spectrum *pixels, vector<bool> &pixelsMask) const {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	// Apply bloom filter to image pixels
	#pragma omp parallel for
	for (
		// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
		unsigned
#endif
		int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x) {
			// Compute bloom for pixel (x, y)
			// Compute extent of pixels contributing bloom
			const u_int x0 = Max<u_int>(x, bloomWidth) - bloomWidth;
			const u_int x1 = Min<u_int>(x + bloomWidth, width - 1);

			float sumWt = 0.f;
			const u_int by = y;
			Spectrum &pixel(bloomBufferTmp[x + y * width]);
			pixel = Spectrum();
			for (u_int bx = x0; bx <= x1; ++bx) {
				if (pixelsMask[bx + by * width]) {
					// Accumulate bloom from pixel (bx, by)
					const u_int dist2 = (x - bx) * (x - bx) + (y - by) * (y - by);
					const float wt = bloomFilter[dist2];
					if (wt == 0.f)
						continue;

					const u_int bloomOffset = bx + by * width;
					sumWt += wt;
					pixel += wt * pixels[bloomOffset];
				}
			}
			if (sumWt > 0.f)
				pixel /= sumWt;
		}
	}
}

void BloomFilterPlugin::BloomFilterY(const Film &film, Spectrum *pixels, vector<bool> &pixelsMask) const {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	// Apply bloom filter to image pixels
	#pragma omp parallel for
	for (
		// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
		unsigned
#endif
		int x = 0; x < width; ++x) {
		for (u_int y = 0; y < height; ++y) {
			// Compute bloom for pixel (x, y)
			// Compute extent of pixels contributing bloom
			const u_int y0 = Max<u_int>(y, bloomWidth) - bloomWidth;
			const u_int y1 = Min<u_int>(y + bloomWidth, height - 1);

			float sumWt = 0.f;
			const u_int bx = x;
			Spectrum &pixel(bloomBuffer[x + y * width]);
			pixel = Spectrum();
			for (u_int by = y0; by <= y1; ++by) {
				if (pixelsMask[bx + by * width]) {
					// Accumulate bloom from pixel (bx, by)
					const u_int dist2 = (x - bx) * (x - bx) + (y - by) * (y - by);
					const float wt = bloomFilter[dist2];
					if (wt == 0.f)
						continue;

					const u_int bloomOffset = bx + by * width;
					sumWt += wt;
					pixel += wt * bloomBufferTmp[bloomOffset];
				}
			}

			if (sumWt > 0.f)
				pixel /= sumWt;
		}
	}
}

void BloomFilterPlugin::Apply(const Film &film, Spectrum *pixels, vector<bool> &pixelsMask) const {
	//const double t1 = WallClockTime();

	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	// Allocate the temporary buffer if required
	if ((!bloomBuffer) || (width * height != bloomBufferSize)) {
		delete[] bloomBuffer;

		bloomBufferSize = width * height;
		bloomBuffer = new Spectrum[bloomBufferSize];
		bloomBufferTmp = new Spectrum[bloomBufferSize];

		// Compute image-space extent of bloom effect
		const u_int bloomSupport = Float2UInt(radius * Max(width, height));
		bloomWidth = bloomSupport / 2;

		// Initialize bloom filter table
		delete[] bloomFilter;
		bloomFilter = new float[2 * bloomWidth * bloomWidth + 1];
		for (u_int i = 0; i < bloomWidth * bloomWidth; ++i) {
			const float z0 = 3.8317f;
			const float dist = z0 * sqrtf(i) / bloomWidth;
			if (dist == 0.f)
				bloomFilter[i] = 1.f;
			else if (dist >= z0)
				bloomFilter[i] = 0.f;
			else {
				// Airy function
				//const float b = boost::math::cyl_bessel_j(1, dist);
				//bloomFilter[i] = powf(2*b/dist, 2.f);

				// Gaussian approximation
				// best-fit sigma^2 for above airy function, based on RMSE
				// depends on choice of zero
				const float sigma2 = 1.698022698724f; 
				bloomFilter[i] = expf(-dist * dist / sigma2);
			}
		}
	}

	// Clear the bloom buffer
	for (u_int i = 0; i < bloomBufferSize; ++i)
		bloomBuffer[i] = Spectrum();

	// Apply separable filter
	BloomFilterX(film, pixels, pixelsMask);
	BloomFilterY(film, bloomBuffer, pixelsMask);

	for (u_int i = 0; i < bloomBufferSize; ++i) {
		if (pixelsMask[i])
			pixels[i] = Lerp(weight, pixels[i], bloomBuffer[i]);
	}

	//const double t2 = WallClockTime();
	//SLG_LOG("Bloom time: " << int((t2 - t1) * 1000.0) << "ms");
}
