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

#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

void AtomicAdd(__global float *val, const float delta) {
	union {
		float f;
		unsigned int i;
	} oldVal;
	union {
		float f;
		unsigned int i;
	} newVal;

	do {
		oldVal.f = *val;
		newVal.f = oldVal.f + delta;
	} while (atom_cmpxchg((__global unsigned int *)val, oldVal.i, newVal.i) != oldVal.i);
}

typedef struct {
	float r, g, b;
} Spectrum;

typedef struct {
	Spectrum radiance;
	float weight;
} SamplePixel;

typedef struct {
	float screenX, screenY;
	Spectrum radiance;
} SampleBufferElem;

__kernel void PixelAddSampleBuffer(
	const unsigned int width,
	const unsigned int height,
	__global SamplePixel *sampleFrameBuffer,
	const unsigned int sampleCount,
	__global SampleBufferElem *sampleBuff) {
	const unsigned int index = get_global_id(0);
	if (index >= sampleCount)
		return;

	__global SampleBufferElem *sampleElem = &sampleBuff[index];
	const unsigned int x = (unsigned int)sampleElem->screenX;
	const unsigned int y = (unsigned int)sampleElem->screenY;
	const unsigned int offset = x + y * width;

	AtomicAdd(&sampleFrameBuffer[offset].radiance.r, sampleElem->radiance.r);
	AtomicAdd(&sampleFrameBuffer[offset].radiance.g, sampleElem->radiance.g);
	AtomicAdd(&sampleFrameBuffer[offset].radiance.b, sampleElem->radiance.b);
	AtomicAdd(&sampleFrameBuffer[offset].weight, 1.f);
}
