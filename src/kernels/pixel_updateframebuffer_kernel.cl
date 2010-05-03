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

typedef struct {
	float r, g, b;
} Spectrum;

typedef struct {
	Spectrum radiance;
	float weight;
} SamplePixel;

typedef Spectrum Pixel;

static float Clamp(float val, float low, float high) {
	return (val > low) ? ((val < high) ? val : high) : low;
}

static unsigned int Floor2UInt(float val) {
	return (val > 0.f) ? ((unsigned int)floor(val)) : 0;
}

static float Radiance2PixelFloat(
		const float x,
		const unsigned int gammaTableSize,
		__global float *gammaTable) {
	//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

	const unsigned int index = min(
		Floor2UInt(gammaTableSize * Clamp(x, 0.f, 1.f)),
			gammaTableSize - 1);
	return gammaTable[index];
}

__kernel __attribute__((reqd_work_group_size(8, 8, 1))) void PixelUpdateFrameBuffer(
	const unsigned int width,
	const unsigned int height,
	__global SamplePixel *sampleFrameBuffer,
	__global Pixel *frameBuffer,
	const unsigned int gammaTableSize,
	__global float *gammaTable) {
    const unsigned int px = get_global_id(0);
    if(px >= width)
        return;
    const unsigned int py = get_global_id(1);
    if(py >= height)
        return;
	const unsigned int offset = px + py * width;

	__global SamplePixel *sp = &sampleFrameBuffer[offset];
	__global Pixel *p = &frameBuffer[offset];

	const float weight = sp->weight;
	if (weight == 0.f)
		return;

	const float invWeight = 1.f / weight;
	p->r = Radiance2PixelFloat(sp->radiance.r * invWeight, gammaTableSize, gammaTable);
	p->g = Radiance2PixelFloat(sp->radiance.g * invWeight, gammaTableSize, gammaTable);
	p->b = Radiance2PixelFloat(sp->radiance.b * invWeight, gammaTableSize, gammaTable);
}
