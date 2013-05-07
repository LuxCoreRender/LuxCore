/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#ifndef _SLG_FRAMEBUFFER_H
#define	_SLG_FRAMEBUFFER_H

#include "luxrays/core/spectrum.h"

namespace slg {

typedef struct {
	luxrays::Spectrum radiance;
	float weight;
} SamplePixel;

class SampleFrameBuffer {
public:
	SampleFrameBuffer(const unsigned int w, const unsigned int h)
		: width(w), height(h) {
		pixels = new SamplePixel[width * height];

		Clear();
	}
	~SampleFrameBuffer() {
		delete[] pixels;
	}

	void Clear() {
		for (unsigned int i = 0; i < width * height; ++i) {
			pixels[i].radiance.r = 0.f;
			pixels[i].radiance.g = 0.f;
			pixels[i].radiance.b = 0.f;
			pixels[i].weight = 0.f;
		}
	};

	SamplePixel *GetPixels() const { return pixels; }

	void AddPixel(const unsigned int x, const unsigned int y, const luxrays::Spectrum &r, const float w) {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		SamplePixel *pixel = &pixels[x + y * width];
		pixel->radiance += r;
		pixel->weight += w;
	}

	void AddPixel(const unsigned int index, const luxrays::Spectrum &r, const float w) {
		assert (index >= 0);
		assert (index < width * height);

		pixels[index].radiance += r;
		pixels[index].weight += w;
	}

	void SetPixel(const unsigned int x, const unsigned int y, const luxrays::Spectrum &r, const float w) {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		SamplePixel *pixel = &pixels[x + y * width];
		pixel->radiance = r;
		pixel->weight = w;
	}

	void SetPixel(const unsigned int index, const luxrays::Spectrum &r, const float w) {
		assert (index >= 0);
		assert (index < width * height);

		pixels[index].radiance = r;
		pixels[index].weight = w;
	}

	SamplePixel *GetPixel(const unsigned int x, const unsigned int y) const {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		return &pixels[x + y * width];
	}

	SamplePixel *GetPixel(const unsigned int index) const {
		assert (index >= 0);
		assert (index < width * height);

		return &pixels[index];
	}

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }

private:
	const unsigned int width, height;

	SamplePixel *pixels;
};

typedef luxrays::Spectrum Pixel;

class FrameBuffer {
public:
	FrameBuffer(const unsigned int w, const unsigned int h)
			: width(w), height(h) {
		pixels = new Pixel[width * height];

		Clear();
	}
	~FrameBuffer() {
		delete[] pixels;
	}

	void Clear() {
		for (unsigned int i = 0; i < width * height; ++i) {
			pixels[i].r = 0.f;
			pixels[i].g = 0.f;
			pixels[i].b = 0.f;
		}
	};

	Pixel *GetPixels() const { return pixels; }

	void SetPixel(const unsigned int x, const unsigned int y, const luxrays::Spectrum& r) {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		pixels[x + y * width] = r;
	}

	void SetPixel(const unsigned int index, const luxrays::Spectrum& r) {
		assert (index >= 0);
		assert (index < width * height);

		pixels[index] = r;
	}

	Pixel *GetPixel(const unsigned int x, const unsigned int y) const {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		return &pixels[x + y * width];
	}

	Pixel *GetPixel(const unsigned int index) const {
		assert (index >= 0);
		assert (index < width * height);

		return &pixels[index];
	}

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }

private:
	const unsigned int width, height;

	Pixel *pixels;
};

typedef struct {
	float alpha;
} AlphaPixel;

typedef struct {
	float priority;
} PriorityPixel;

class AlphaFrameBuffer {
public:
	AlphaFrameBuffer(const unsigned int w, const unsigned int h)
		: width(w), height(h) {
		pixels = new AlphaPixel[width * height];

		Clear();
	}
	~AlphaFrameBuffer() {
		delete[] pixels;
	}

	void Clear() {
		for (unsigned int i = 0; i < width * height; ++i)
			pixels[i].alpha = 0.f;
	};

	AlphaPixel *GetPixels() const { return pixels; }

	void AddPixel(const unsigned int x, const unsigned int y, const float a) {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		AlphaPixel *pixel = &pixels[x + y * width];
		pixel->alpha += a;
	}

	void AddPixel(const unsigned int index, const float a) {
		assert (index >= 0);
		assert (index < width * height);

		pixels[index].alpha += a;
	}

	void SetPixel(const unsigned int x, const unsigned int y, const float a) {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		AlphaPixel *pixel = &pixels[x + y * width];
		pixel->alpha = a;
	}

	void SetPixel(const unsigned int index, const float a) {
		assert (index >= 0);
		assert (index < width * height);

		pixels[index].alpha = a;
	}

	AlphaPixel *GetPixel(const unsigned int x, const unsigned int y) const {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		return &pixels[x + y * width];
	}

	AlphaPixel *GetPixel(const unsigned int index) const {
		assert (index >= 0);
		assert (index < width * height);

		return &pixels[index];
	}

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }

private:
	const unsigned int width, height;

	AlphaPixel *pixels;
};


class PriorityFrameBuffer {
public:
	PriorityFrameBuffer(const unsigned int w, const unsigned int h)
		: width(w), height(h) {
		pixels = new PriorityPixel[width * height];

		Clear();
	}
	~PriorityFrameBuffer() {
		delete[] pixels;
	}


	void Clear() {

		for (unsigned int i = 0; i < width * height; ++i)
			pixels[i].priority = 1.f;
	};

	PriorityPixel *GetPixels() const { return pixels; }

	void SetPixel(const unsigned int x, const unsigned int y, const float a) {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		PriorityPixel *pixel = &pixels[x + y * width];
		pixel->priority = a;
	}

	void SetPixel(const unsigned int index, const float a) {
		assert (index >= 0);
		assert (index < width * height);

		pixels[index].priority = a;
	}

	PriorityPixel *GetPixel(const unsigned int x, const unsigned int y) const {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		return &pixels[x + y * width];
	}

	PriorityPixel *GetPixel(const unsigned int index) const {
		assert (index >= 0);
		assert (index < width * height);

		return &pixels[index];
	}

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }

private:
	const unsigned int width, height;

	PriorityPixel *pixels;
};


}

#endif	/* _SLG_FRAMEBUFFER_H */

