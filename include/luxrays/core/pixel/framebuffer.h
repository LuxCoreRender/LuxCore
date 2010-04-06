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

#ifndef _FRAMEBUFFER_H
#define	_FRAMEBUFFER_H

#include "luxrays/core/pixel/spectrum.h"

namespace luxrays {

typedef struct {
	Spectrum radiance;
	float weight;
} SamplePixel;

class SampleFrameBuffer {
public:
	SampleFrameBuffer(const unsigned int w, const unsigned int h) {
		width = w;
		height = h;
		pixels = new SamplePixel[width * height];

		Reset();
	}
	~SampleFrameBuffer() {
		delete[] pixels;
	}

	void Reset() {
		for (unsigned int i = 0; i < width * height; ++i) {
			pixels[i].radiance.r = 0.f;
			pixels[i].radiance.g = 0.f;
			pixels[i].radiance.b = 0.f;
			pixels[i].weight = 0.f;
		}
	};

private:
	const unsigned int width, height;

	SamplePixel *pixels;
};

typedef Spectrum Pixel;

class FrameBuffer {
public:
	FrameBuffer(const unsigned int w, const unsigned int h) {
		width = w;
		height = h;
		pixels = new SamplePixel[width * height];

		Reset();
	}
	~FrameBuffer() {
		delete[] pixels;
	}

	void Reset() {
		for (unsigned int i = 0; i < width * height; ++i) {
			pixels[i].r = 0.f;
			pixels[i].g = 0.f;
			pixels[i].b = 0.f;
		}
	};

private:
	const unsigned int width, height;

	Pixel *pixels;
};

}

#endif	/* _FRAMEBUFFER_H */

