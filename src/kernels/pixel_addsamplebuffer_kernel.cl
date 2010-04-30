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

// NOTE: this kernel assume samples do not overlap

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

static void AddSample(__global SamplePixel *sp, const float4 sample) {
    __global float4 *p = (__global float4 *)sp;
    *p += sample;
}

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
    const float4 sample = (float4)(sampleElem->radiance.r, sampleElem->radiance.g, sampleElem->radiance.b, 1.f);

    AddSample(&sampleFrameBuffer[x + y * width], sample);
}
