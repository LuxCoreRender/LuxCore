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

size_t NativePixelDevice::SampleBufferSize = 512;

NativePixelDevice::NativePixelDevice(const Context *context,
		const size_t threadIndex, const unsigned int devIndex) :
			PixelDevice(context, DEVICE_TYPE_NATIVE_THREAD, devIndex) {
	char buf[64];
	sprintf(buf, "NativePixelThread-%03d", (int)threadIndex);
	deviceName = std::string(buf);

	sampleFrameBuffer = NULL;
	frameBuffer = NULL;

	SetGamma();
}

NativePixelDevice::~NativePixelDevice() {
	if (started)
		PixelDevice::Stop();

	delete sampleFrameBuffer;
	delete frameBuffer;
}

void NativePixelDevice::Init(const unsigned int w, const unsigned int h) {
	PixelDevice::Init(w, h);

	delete sampleFrameBuffer;
	delete frameBuffer;

	sampleFrameBuffer = new SampleFrameBuffer(width, height);
	sampleFrameBuffer->Reset();

	frameBuffer = new FrameBuffer(width, height);
}

void NativePixelDevice::Reset() {
	sampleFrameBuffer->Reset();
}

void NativePixelDevice::SetGamma(const float gamma ) {
	float x = 0.f;
	const float dx = 1.f / GammaTableSize;
	for (unsigned int i = 0; i < GammaTableSize; ++i, x += dx)
		gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / gamma);
}

void NativePixelDevice::Start() {
	PixelDevice::Start();
}

void NativePixelDevice::Interrupt() {
	assert (started);
}

void NativePixelDevice::Stop() {
	PixelDevice::Stop();
}

SampleBuffer *NativePixelDevice::NewSampleBuffer() {
	return new SampleBuffer(SampleBufferSize);
}

void NativePixelDevice::PushSampleBuffer(const SampleBuffer *sampleBuffer) {
	assert (started);

	const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
	SamplePixel *sp = sampleFrameBuffer->GetPixels();
	for (unsigned int i = 0; i < sampleBuffer->GetSampleCount(); ++i) {
		const SampleBufferElem *sampleElem = &sbe[i];
		int x = (int)sampleElem->screenX;
		int y = (int)sampleElem->screenY;

		unsigned int offset = x + y * width;
		AtomicAdd(&sp[offset].radiance.r, sampleElem->radiance.r);
		AtomicAdd(&sp[offset].radiance.g, sampleElem->radiance.g);
		AtomicAdd(&sp[offset].radiance.b, sampleElem->radiance.b);
		AtomicAdd(&sp[offset].weight, 1.f);
	}
}

void NativePixelDevice::UpdateFrameBuffer() {
	const SamplePixel *sp = sampleFrameBuffer->GetPixels();
	Pixel *p = frameBuffer->GetPixels();
	for (unsigned int i = 0, j = 0; i < width * height; ++i) {
		const float weight = sp[i].weight;

		if (weight == 0.f)
			j++;
		else {
			const float invWeight = 1.f / weight;

			p[j].r = Radiance2PixelFloat(sp[i].radiance.r * invWeight);
			p[j].g = Radiance2PixelFloat(sp[i].radiance.g * invWeight);
			p[j++].b = Radiance2PixelFloat(sp[i].radiance.b * invWeight);
		}
	}
}
