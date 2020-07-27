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

#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#include "luxrays/kernels/kernels.h"
#include "slg/kernels/kernels.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/bloom.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Bloom filter plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BloomFilterPlugin)

BloomFilterPlugin::BloomFilterPlugin(const float r, const float w) :
		radius(r), weight(w), bloomBuffer(nullptr), bloomBufferTmp(nullptr),
		bloomBufferSize(0), bloomFilter(nullptr), bloomFilterSize(0) {
	hardwareDevice = nullptr;
	hwBloomBuffer = nullptr;
	hwBloomBufferTmp = nullptr;
	hwBloomFilter = nullptr;

	bloomFilterXKernel = nullptr;
	bloomFilterYKernel = nullptr;
	bloomFilterMergeKernel = nullptr;
}

BloomFilterPlugin::BloomFilterPlugin() {
	bloomBuffer = nullptr;
	bloomBufferTmp = nullptr;
	bloomFilter = nullptr;

	hardwareDevice = nullptr;
	hwBloomBuffer = nullptr;
	hwBloomBufferTmp = nullptr;
	hwBloomFilter = nullptr;

	bloomFilterXKernel = nullptr;
	bloomFilterYKernel = nullptr;
	bloomFilterMergeKernel = nullptr;
}

BloomFilterPlugin::~BloomFilterPlugin() {
	delete[] bloomBuffer;
	delete[] bloomBufferTmp;
	delete[] bloomFilter;

	delete bloomFilterXKernel;
	delete bloomFilterYKernel;
	delete bloomFilterMergeKernel;

	if (hardwareDevice) {
		hardwareDevice->FreeBuffer(&hwBloomBuffer);
		hardwareDevice->FreeBuffer(&hwBloomBufferTmp);
		hardwareDevice->FreeBuffer(&hwBloomFilter);
	}
}

ImagePipelinePlugin *BloomFilterPlugin::Copy() const {
	return new BloomFilterPlugin(radius, weight);
}

void BloomFilterPlugin::InitFilterTable(const Film &film) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	// Compute image-space extent of bloom effect
	const u_int bloomSupport = Float2UInt(radius * Max(width, height));
	bloomWidth = bloomSupport / 2;

	// Initialize bloom filter table
	delete[] bloomFilter;
	bloomFilterSize = 2 * bloomWidth * bloomWidth + 1;
	bloomFilter = new float[bloomFilterSize];
	for (u_int i = 0; i < bloomFilterSize; ++i)
		bloomFilter[i] = 0.f;

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

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void BloomFilterPlugin::BloomFilterX(const Film &film, Spectrum *pixels) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	// Apply bloom filter to image pixels
	#pragma omp parallel for
	for (
		// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
		unsigned
#endif
		int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x) {
			if (film.HasSamples(hasPN, hasSN, x, y)) {
				// Compute bloom for pixel (x, y)
				// Compute extent of pixels contributing bloom
				const u_int x0 = Max<u_int>(x, bloomWidth) - bloomWidth;
				const u_int x1 = Min<u_int>(x + bloomWidth, width - 1);

				float sumWt = 0.f;
				const u_int by = y;
				Spectrum &pixel(bloomBufferTmp[x + y * width]);
				pixel = Spectrum();
				for (u_int bx = x0; bx <= x1; ++bx) {
					if (film.HasSamples(hasPN, hasSN, bx, by)) {
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
}

void BloomFilterPlugin::BloomFilterY(const Film &film) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	// Apply bloom filter to image pixels
	#pragma omp parallel for
	for (
		// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
		unsigned
#endif
		int x = 0; x < width; ++x) {
		for (u_int y = 0; y < height; ++y) {
			if (film.HasSamples(hasPN, hasSN, x, y)) {
				// Compute bloom for pixel (x, y)
				// Compute extent of pixels contributing bloom
				const u_int y0 = Max<u_int>(y, bloomWidth) - bloomWidth;
				const u_int y1 = Min<u_int>(y + bloomWidth, height - 1);

				float sumWt = 0.f;
				const u_int bx = x;
				Spectrum &pixel(bloomBuffer[x + y * width]);
				pixel = Spectrum();
				for (u_int by = y0; by <= y1; ++by) {
					if (film.HasSamples(hasPN, hasSN, bx, by)) {
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
}

void BloomFilterPlugin::BloomFilter(const Film &film, Spectrum *pixels) {
	BloomFilterX(film, pixels);
	BloomFilterY(film);
}

void BloomFilterPlugin::Apply(Film &film, const u_int index) {
	//const double t1 = WallClockTime();

	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	// Allocate the temporary buffer if required
	if ((!bloomBuffer) || (width * height != bloomBufferSize)) {
		delete[] bloomBuffer;
		delete[] bloomBufferTmp;

		bloomBufferSize = width * height;
		bloomBuffer = new Spectrum[bloomBufferSize];
		bloomBufferTmp = new Spectrum[bloomBufferSize];

		InitFilterTable(film);
	}

	// Apply separable filter
	BloomFilter(film, pixels);

	for (u_int i = 0; i < bloomBufferSize; ++i) {
		if (film.HasSamples(hasPN, hasSN, i))
			pixels[i] = Lerp(weight, pixels[i], bloomBuffer[i]);
	}

	//const double t2 = WallClockTime();
	//SLG_LOG("Bloom time: " << int((t2 - t1) * 1000.0) << "ms");
}

//------------------------------------------------------------------------------
// HardwareDevice version
//------------------------------------------------------------------------------

void BloomFilterPlugin::AddHWChannelsUsed(unordered_set<Film::FilmChannelType> &hwChannelsUsed) const {
	hwChannelsUsed.insert(Film::IMAGEPIPELINE);
}

void BloomFilterPlugin::ApplyHW(Film &film, const u_int index) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	if ((!bloomFilter) || (width * height != bloomBufferSize)) {
		bloomBufferSize = width * height;
		InitFilterTable(film);
	}

	if (!bloomFilterXKernel) {
		film.ctx->SetVerbose(true);

		hardwareDevice = film.hardwareDevice;

		// Allocate OpenCL buffers
		hardwareDevice->AllocBufferRW(&hwBloomBuffer, nullptr, bloomBufferSize * sizeof(Spectrum), "Bloom buffer");
		hardwareDevice->AllocBufferRW(&hwBloomBufferTmp, nullptr, bloomBufferSize * sizeof(Spectrum), "Bloom temporary buffer");
		hardwareDevice->AllocBufferRO(&hwBloomFilter, bloomFilter, bloomFilterSize * sizeof(float), "Bloom filter table");

		// Compile sources
		const double tStart = WallClockTime();

		vector<string> opts;
		opts.push_back("-D LUXRAYS_OPENCL_KERNEL");
		opts.push_back("-D SLG_OPENCL_KERNEL");

		HardwareDeviceProgram *program = nullptr;
		hardwareDevice->CompileProgram(&program,
				opts,
				luxrays::ocl::KernelSource_color_types +
				slg::ocl::KernelSource_plugin_bloom_funcs,
				"BloomFilterPlugin");

		//----------------------------------------------------------------------
		// BloomFilterPlugin_FilterX kernel
		//----------------------------------------------------------------------

		SLG_LOG("[BloomFilterPlugin] Compiling BloomFilterPlugin_FilterX Kernel");
		hardwareDevice->GetKernel(program, &bloomFilterXKernel, "BloomFilterPlugin_FilterX");

		// Set kernel arguments
		u_int argIndex = 0;
		hardwareDevice->SetKernelArg(bloomFilterXKernel, argIndex++, width);
		hardwareDevice->SetKernelArg(bloomFilterXKernel, argIndex++, height);
		hardwareDevice->SetKernelArg(bloomFilterXKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(bloomFilterXKernel, argIndex++, hwBloomBuffer);
		hardwareDevice->SetKernelArg(bloomFilterXKernel, argIndex++, hwBloomBufferTmp);
		hardwareDevice->SetKernelArg(bloomFilterXKernel, argIndex++, hwBloomFilter);
		hardwareDevice->SetKernelArg(bloomFilterXKernel, argIndex++, bloomWidth);

		//----------------------------------------------------------------------
		// BloomFilterPlugin_FilterY kernel
		//----------------------------------------------------------------------

		SLG_LOG("[BloomFilterPlugin] Compiling BloomFilterPlugin_FilterY Kernel");
		hardwareDevice->GetKernel(program, &bloomFilterYKernel, "BloomFilterPlugin_FilterY");

		// Set kernel arguments
		argIndex = 0;
		hardwareDevice->SetKernelArg(bloomFilterYKernel, argIndex++, width);
		hardwareDevice->SetKernelArg(bloomFilterYKernel, argIndex++, height);
		hardwareDevice->SetKernelArg(bloomFilterYKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(bloomFilterYKernel, argIndex++, hwBloomBuffer);
		hardwareDevice->SetKernelArg(bloomFilterYKernel, argIndex++, hwBloomBufferTmp);
		hardwareDevice->SetKernelArg(bloomFilterYKernel, argIndex++, hwBloomFilter);
		hardwareDevice->SetKernelArg(bloomFilterYKernel, argIndex++, bloomWidth);

		//----------------------------------------------------------------------
		// BloomFilterPlugin_Merge kernel
		//----------------------------------------------------------------------

		SLG_LOG("[BloomFilterPlugin] Compiling BloomFilterPlugin_Merge Kernel");
		hardwareDevice->GetKernel(program, &bloomFilterMergeKernel, "BloomFilterPlugin_Merge");

		// Set kernel arguments
		argIndex = 0;
		hardwareDevice->SetKernelArg(bloomFilterMergeKernel, argIndex++, width);
		hardwareDevice->SetKernelArg(bloomFilterMergeKernel, argIndex++, height);
		hardwareDevice->SetKernelArg(bloomFilterMergeKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(bloomFilterMergeKernel, argIndex++, hwBloomBuffer);
		hardwareDevice->SetKernelArg(bloomFilterMergeKernel, argIndex++, weight);

		//----------------------------------------------------------------------

		delete program;

		const double tEnd = WallClockTime();
		SLG_LOG("[BloomFilterPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		film.ctx->SetVerbose(false);
	}

	hardwareDevice->EnqueueKernel(bloomFilterXKernel, HardwareDeviceRange(RoundUp(width * height, 256u)),
			HardwareDeviceRange(256));
	hardwareDevice->EnqueueKernel(bloomFilterYKernel, HardwareDeviceRange(RoundUp(width * height, 256u)),
			HardwareDeviceRange(256));
	hardwareDevice->EnqueueKernel(bloomFilterMergeKernel, HardwareDeviceRange(RoundUp(width * height, 256u)),
			HardwareDeviceRange(256));
}
