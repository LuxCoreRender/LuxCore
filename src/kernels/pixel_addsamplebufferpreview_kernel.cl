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

static int Ceil2Int(const float val) {
	return (int)ceil(val);
}

static int Floor2Int(const float val) {
	return (int)floor(val);
}

__kernel void PixelAddSampleBufferPreview(
	const unsigned int width,
	const unsigned int height,
	__global SamplePixel *sampleFrameBuffer,
	const unsigned int sampleCount,
	__global SampleBufferElem *sampleBuff) {
	const unsigned int index = get_global_id(0);
	if (index >= sampleCount)
		return;

	const int splatSize = 3;
	__global SampleBufferElem *sampleElem = &sampleBuff[index];
	const float dImageX = sampleElem->screenX - 0.5f;
	const float dImageY = sampleElem->screenY - 0.5f;
	int x0 = Ceil2Int(dImageX - splatSize);
	int x1 = Floor2Int(dImageX + splatSize);
	int y0 = Ceil2Int(dImageY - splatSize);
	int y1 = Floor2Int(dImageY + splatSize);
	if (x1 < x0 || y1 < y0 || x1 < 0 || y1 < 0)
		return;

	x0 = max(x0, 0);
	x1 = min(x1, (int)width - 1);
	y0 = max(y0, 0);
	y1 = min(y1, (int)height - 1);

	for (int y = y0; y <= y1; ++y) {
		for (int x = x0; x <= x1; ++x) {
			const unsigned int offset = x + y * width;
			__global SamplePixel *sp = &(sampleFrameBuffer[offset]);

			/*AtomicAdd(&(sp->radiance.r), 0.01f * sampleElem->radiance.r);
			AtomicAdd(&(sp->radiance.g), 0.01f * sampleElem->radiance.g);
			AtomicAdd(&(sp->radiance.b), 0.01f * sampleElem->radiance.b);

			AtomicAdd(&(sp->weight), 0.01f);*/

			sp->radiance.r += 0.01f * sampleElem->radiance.r;
			sp->radiance.g += 0.01f * sampleElem->radiance.g;
			sp->radiance.b += 0.01f * sampleElem->radiance.b;

			sp->weight += 0.01f;
		}
	}
}
