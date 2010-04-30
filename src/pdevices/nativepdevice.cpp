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

#include "luxrays/core/pixeldevice.h"
#include "luxrays/core/context.h"

using namespace luxrays;

//------------------------------------------------------------------------------
// Native CPU PixelDevice
//------------------------------------------------------------------------------

size_t NativePixelDevice::SampleBufferSize = 4096;

NativePixelDevice::NativePixelDevice(const Context *context,
		const unsigned int threadIndex, const unsigned int devIndex) :
			PixelDevice(context, DEVICE_TYPE_NATIVE_THREAD, devIndex) {
	char buf[64];
	sprintf(buf, "NativePixelThread-%03d", (int)threadIndex);
	deviceName = std::string(buf);

	sampleFrameBuffer = NULL;
	frameBuffer = NULL;

	SetGamma();

	// Initialize Gaussian2x2_filterTable
	const float alpha = 2.f;
	const float expX = expf(-alpha * Gaussian2x2_xWidth * Gaussian2x2_xWidth);
	const float expY = expf(-alpha * Gaussian2x2_yWidth * Gaussian2x2_yWidth);

	float *ftp2x2 = Gaussian2x2_filterTable;
	for (u_int y = 0; y < FilterTableSize; ++y) {
		const float fy = (static_cast<float>(y) + .5f) * Gaussian2x2_yWidth / FilterTableSize;
		for (u_int x = 0; x < FilterTableSize; ++x) {
			const float fx = (static_cast<float>(x) + .5f) * Gaussian2x2_xWidth / FilterTableSize;
			*ftp2x2++ = Max<float>(0.f, expf(-alpha * fx * fx) - expX) *
					Max<float>(0.f, expf(-alpha * fy * fy) - expY);
		}
	}

	sampleBuffers.resize(0);
	sampleBuffersUsed.resize(0);
}

NativePixelDevice::~NativePixelDevice() {
	if (started)
		PixelDevice::Stop();

	delete sampleFrameBuffer;
	delete frameBuffer;

	for (size_t i = 0; i < sampleBuffers.size(); ++i)
		delete sampleBuffers[i];
}

void NativePixelDevice::AllocateSampleBuffers(const unsigned int count) {
	assert (!started);

	// Free existing buffers
	for (size_t i = 0; i < sampleBuffers.size(); ++i)
		delete sampleBuffers[i];

	// Allocate new one
	sampleBuffers.resize(count);
	sampleBuffersUsed.resize(count);
	for (size_t i = 0; i < sampleBuffers.size(); ++i) {
		sampleBuffersUsed[i] = false;
		sampleBuffers[i] = new SampleBuffer(SampleBufferSize);
	}
}

void NativePixelDevice::Init(const unsigned int w, const unsigned int h) {
	PixelDevice::Init(w, h);

	delete sampleFrameBuffer;
	delete frameBuffer;

	sampleFrameBuffer = new SampleFrameBuffer(width, height);
	sampleFrameBuffer->Clear();

	frameBuffer = new FrameBuffer(width, height);
	frameBuffer->Clear();
}

void NativePixelDevice::ClearSampleFrameBuffer() {
	sampleFrameBuffer->Clear();
}

void NativePixelDevice::ClearFrameBuffer() {
	frameBuffer->Clear();
}

void NativePixelDevice::SetGamma(const float gamma) {
	float x = 0.f;
	const float dx = 1.f / GammaTableSize;
	for (unsigned int i = 0; i < GammaTableSize; ++i, x += dx)
		gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / gamma);
}

void NativePixelDevice::Start() {
	boost::mutex::scoped_lock lock(splatMutex);
	PixelDevice::Start();
}

void NativePixelDevice::Interrupt() {
	boost::mutex::scoped_lock lock(splatMutex);
	assert (started);
}

void NativePixelDevice::Stop() {
	boost::mutex::scoped_lock lock(splatMutex);
	PixelDevice::Stop();
}

SampleBuffer *NativePixelDevice::GetFreeSampleBuffer() {
	boost::mutex::scoped_lock lock(splatMutex);

	// Look for a free buffer
	for (size_t i = 0; i < sampleBuffersUsed.size(); ++i) {
		if (!sampleBuffersUsed[i]) {
			sampleBuffersUsed[i] = true;
			sampleBuffers[i]->Reset();
			return sampleBuffers[i];
		}
	}

	throw std::runtime_error("Internal error: out of SampleBuffer in NativePixelDevice::GetFreeSampleBuffer()");
}

void NativePixelDevice::FreeSampleBuffer(SampleBuffer *sampleBuffer) {
	boost::mutex::scoped_lock lock(splatMutex);

	for (size_t i = 0; i < sampleBuffers.size(); ++i) {
		if (sampleBuffers[i] == sampleBuffer) {
			sampleBuffersUsed[i] = false;
			return;
		}
	}

	throw std::runtime_error("Internal error: unable to find the SampleBuffer in NativePixelDevice::FreeSampleBuffer()");
}

void NativePixelDevice::SplatPreview(const SampleBufferElem *sampleElem) {
	const int splatSize = 4;

	// Compute sample's raster extent
	float dImageX = sampleElem->screenX - 0.5f;
	float dImageY = sampleElem->screenY - 0.5f;
	int x0 = Ceil2Int(dImageX - splatSize);
	int x1 = Floor2Int(dImageX + splatSize);
	int y0 = Ceil2Int(dImageY - splatSize);
	int y1 = Floor2Int(dImageY + splatSize);
	if (x1 < x0 || y1 < y0 || x1 < 0 || y1 < 0)
		return;

	for (u_int y = static_cast<u_int>(Max<int>(y0, 0)); y <= static_cast<u_int>(Min<int>(y1, height - 1)); ++y)
		for (u_int x = static_cast<u_int>(Max<int>(x0, 0)); x <= static_cast<u_int>(Min<int>(x1, width - 1)); ++x)
			SplatRadiance(sampleElem->radiance, x, y, 0.01f);
}

void NativePixelDevice::SplatGaussian2x2(const SampleBufferElem *sampleElem) {
	// Compute sample's raster extent
	float dImageX = sampleElem->screenX - 0.5f;
	float dImageY = sampleElem->screenY - 0.5f;
	int x0 = Ceil2Int(dImageX - Gaussian2x2_xWidth);
	int x1 = Floor2Int(dImageX + Gaussian2x2_xWidth);
	int y0 = Ceil2Int(dImageY - Gaussian2x2_yWidth);
	int y1 = Floor2Int(dImageY + Gaussian2x2_yWidth);
	if (x1 < x0 || y1 < y0 || x1 < 0 || y1 < 0)
		return;
	// Loop over filter support and add sample to pixel arrays
	int ifx[32];
	for (int x = x0; x <= x1; ++x) {
		float fx = fabsf((x - dImageX) *
				Gaussian2x2_invXWidth * FilterTableSize);
		ifx[x - x0] = Min<int>(Floor2Int(fx), FilterTableSize - 1);
	}

	int ify[32];
	for (int y = y0; y <= y1; ++y) {
		float fy = fabsf((y - dImageY) *
				Gaussian2x2_invYWidth * FilterTableSize);
		ify[y - y0] = Min<int>(Floor2Int(fy), FilterTableSize - 1);
	}
	float filterNorm = 0.f;
	for (int y = y0; y <= y1; ++y) {
		for (int x = x0; x <= x1; ++x) {
			const int offset = ify[y - y0] * FilterTableSize + ifx[x - x0];
			filterNorm += Gaussian2x2_filterTable[offset];
		}
	}
	filterNorm = 1.f / filterNorm;

	for (u_int y = static_cast<u_int>(Max<int>(y0, 0)); y <= static_cast<u_int>(Min<int>(y1, height - 1)); ++y) {
		for (u_int x = static_cast<u_int>(Max<int>(x0, 0)); x <= static_cast<u_int>(Min<int>(x1, width - 1)); ++x) {
			const int offset = ify[y - y0] * FilterTableSize + ifx[x - x0];
			const float filterWt = Gaussian2x2_filterTable[offset] * filterNorm;
			SplatRadiance(sampleElem->radiance, x, y, filterWt);
		}
	}
}

void NativePixelDevice::AddSampleBuffer(const FilterType type, SampleBuffer *sampleBuffer) {
	boost::mutex::scoped_lock lock(splatMutex);
	assert (started);

	const double t = WallClockTime();
	switch (type) {
		case FILTER_GAUSSIAN: {
			const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
			for (unsigned int i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				SplatGaussian2x2(&sbe[i]);
			break;
		}
		case FILTER_PREVIEW: {
			const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
			for (unsigned int i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				SplatPreview(&sbe[i]);
			break;
		}
		case FILTER_NONE: {
			const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
			for (unsigned int i = 0; i < sampleBuffer->GetSampleCount(); ++i) {
				const SampleBufferElem *sampleElem = &sbe[i];
				const int x = (int)sampleElem->screenX;
				const int y = (int)sampleElem->screenY;

				SplatRadiance(sampleElem->radiance, x, y, 1.f);
			}
			break;
		}
		default:
			assert (false);
			break;
	}
	statsTotalSampleTime += WallClockTime() - t;
	statsTotalSamplesCount += sampleBuffer->GetSampleCount();

	// Free the sample buffer
	for (size_t i = 0; i < sampleBuffers.size(); ++i) {
		if (sampleBuffers[i] == sampleBuffer) {
			sampleBuffersUsed[i] = false;
			return;
		}
	}
}

void NativePixelDevice::UpdateFrameBuffer() {
	boost::mutex::scoped_lock lock(splatMutex);

	const SamplePixel *sp = sampleFrameBuffer->GetPixels();
	Pixel *p = frameBuffer->GetPixels();
	for (unsigned int i = 0; i < width * height; ++i) {
		const float weight = sp[i].weight;

		if (weight > 0.f) {
			const float invWeight = 1.f / weight;

			p[i].r = Radiance2PixelFloat(sp[i].radiance.r * invWeight);
			p[i].g = Radiance2PixelFloat(sp[i].radiance.g * invWeight);
			p[i].b = Radiance2PixelFloat(sp[i].radiance.b * invWeight);
		}
	}
}

void NativePixelDevice::Merge(const SampleFrameBuffer *sfb) {
	boost::mutex::scoped_lock lock(splatMutex);

	for (unsigned int i = 0; i < width * height; ++i) {
		SamplePixel *sp = sfb->GetPixel(i);
		SamplePixel *spbase = sampleFrameBuffer->GetPixel(i);

		spbase->radiance += sp->radiance;
		spbase->weight += sp->weight;
	}
}

const SampleFrameBuffer *NativePixelDevice::GetSampleFrameBuffer() const {
	return sampleFrameBuffer;
}
