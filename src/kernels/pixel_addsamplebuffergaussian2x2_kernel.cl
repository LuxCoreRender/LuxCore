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

#define Gaussian2x2_xWidth 2.f
#define Gaussian2x2_yWidth 2.f
#define Gaussian2x2_invXWidth (1.f / 2.f)
#define Gaussian2x2_invYWidth (1.f / 2.f)

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

static int Ceil2Int(const float val) {
	return (int)ceil(val);
}

static int Floor2Int(const float val) {
	return (int)floor(val);
}

static void AddSample(__global SamplePixel *sp, const float4 sample) {
    float4 weight = (float4)(sample.w, sample.w, sample.w, 1.f);
    __global float4 *p = (__global float4 *)sp;
    *p += weight * sample;
}

__kernel void PixelAddSampleBufferGaussian2x2(
	const unsigned int width,
	const unsigned int height,
	__global SamplePixel *sampleFrameBuffer,
	const unsigned int sampleCount,
	__global SampleBufferElem *sampleBuff,
	const unsigned int FilterTableSize,
    __global float *Gaussian2x2_filterTable,
	__local int *ifxBuff, __local int *ifyBuff) {
	const unsigned int index = get_global_id(0);
	if (index >= sampleCount)
		return;

	__global SampleBufferElem *sampleElem = &sampleBuff[index];
    const float4 sample = (float4)(sampleElem->radiance.r, sampleElem->radiance.g, sampleElem->radiance.b, 1.f);

	const float dImageX = sampleElem->screenX - 0.5f;
	const float dImageY = sampleElem->screenY - 0.5f;
	const int x0 = Ceil2Int(dImageX - Gaussian2x2_xWidth);
	const int x1 = Floor2Int(dImageX + Gaussian2x2_xWidth);
	const int y0 = Ceil2Int(dImageY - Gaussian2x2_yWidth);
	const int y1 = Floor2Int(dImageY + Gaussian2x2_yWidth);
	if (x1 < x0 || y1 < y0 || x1 < 0 || y1 < 0)
		return;

	// Loop over filter support and add sample to pixel arrays
	__local int *ifx = &(ifxBuff[16 * get_local_id(0)]);
	for (int x = x0; x <= x1; ++x) {
		const float fx = fabs((x - dImageX) *
				Gaussian2x2_invXWidth * FilterTableSize);
		ifx[x - x0] = min(Floor2Int(fx), (int)FilterTableSize - 1);
	}

	__local int *ify = &(ifyBuff[16 * get_local_id(0)]);
	for (int y = y0; y <= y1; ++y) {
		const float fy = fabs((y - dImageY) *
				Gaussian2x2_invYWidth * FilterTableSize);
		ify[y - y0] = min(Floor2Int(fy), (int)FilterTableSize - 1);
	}

	float filterNorm = 0.f;
	for (int y = y0; y <= y1; ++y) {
		for (int x = x0; x <= x1; ++x) {
			const int offset = ify[y - y0] * FilterTableSize + ifx[x - x0];
			filterNorm += Gaussian2x2_filterTable[offset];
		}
	}
	filterNorm = 1.f / filterNorm;

	const int fx0 = max(x0, 0);
	const int fx1 = min(x1, (int)width - 1);
	const int fy0 = max(y0, 0);
	const int fy1 = min(y1, (int)height - 1);


    for (int y = fy0; y <= fy1; ++y) {
        const unsigned int offset = y * width;

		for (int x = fx0; x <= fx1; ++x) {
            const int tabOffset = ify[y - y0] * FilterTableSize + ifx[x - x0];
			sample.w = Gaussian2x2_filterTable[tabOffset] * filterNorm;

            AddSample(&sampleFrameBuffer[offset + x], sample);
        }
	}
}
