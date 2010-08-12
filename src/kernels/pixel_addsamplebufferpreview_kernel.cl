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

int Ceil2Int(const float val) {
	return (int)ceil(val);
}

int Floor2Int(const float val) {
	return (int)floor(val);
}

void AddSample(__global SamplePixel *sp, const float4 sample) {
    float4 weight = (float4)(sample.w, sample.w, sample.w, 1.f);
    __global float4 *p = (__global float4 *)sp;
    *p += weight * sample;
}

__kernel __attribute__((reqd_work_group_size(64, 1, 1))) void PixelAddSampleBufferPreview(
	const unsigned int width,
	const unsigned int height,
	__global SamplePixel *sampleFrameBuffer,
	const unsigned int sampleCount,
	__global SampleBufferElem *sampleBuff) {
	const unsigned int index = get_global_id(0);
	if (index >= sampleCount)
		return;

	const float splatSize = 2.0f;
	__global SampleBufferElem *sampleElem = &sampleBuff[index];

	const float dImageX = sampleElem->screenX - 0.5f;
	const float dImageY = sampleElem->screenY - 0.5f;
    const float4 sample = (float4)(sampleElem->radiance.r, sampleElem->radiance.g, sampleElem->radiance.b, 0.01f);

	int x0 = Ceil2Int(dImageX - splatSize);
	int x1 = Floor2Int(dImageX + splatSize);
	int y0 = Ceil2Int(dImageY - splatSize);
	int y1 = Floor2Int(dImageY + splatSize);

	x0 = max(x0, 0);
	x1 = min(x1, (int)width - 1);
	y0 = max(y0, 0);
	y1 = min(y1, (int)height - 1);

	for (int y = y0; y <= y1; ++y) {
        const unsigned int offset = y * width;

		for (int x = x0; x <= x1; ++x)
            AddSample(&sampleFrameBuffer[offset + x], sample);
	}
}
