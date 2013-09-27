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

#include "luxrays/core/utils.h"

namespace slg {

template<u_int CHANNELS, class T> class GenericFrameBuffer {
public:
	GenericFrameBuffer(const u_int w, const u_int h)
		: width(w), height(h) {
		pixels = new T[width * height * CHANNELS];

		Clear();
	}
	~GenericFrameBuffer() {
		delete[] pixels;
	}

	void Clear(const T value = 0) {
		std::fill(pixels, pixels + width * height * CHANNELS, value);
	};

	T *GetPixels() const { return pixels; }

	bool MinPixel(const u_int x, const u_int y, const T *v) {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		T *pixel = &pixels[(x + y * width) * CHANNELS];
		bool write = false;
		for (u_int i = 0; i < CHANNELS; ++i) {
			if (v[i] < pixel[i]) {
				pixel[i] = v[i];
				write = true;
			}
		}

		return write;
	}

	void AddPixel(const u_int x, const u_int y, const T *v) {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		T *pixel = &pixels[(x + y * width) * CHANNELS];
		for (u_int i = 0; i < CHANNELS; ++i)
			pixel[i] += v[i];
	}

	void AddWeightedPixel(const u_int x, const u_int y, const T *v, const float weight) {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		T *pixel = &pixels[(x + y * width) * CHANNELS];
		for (u_int i = 0; i < CHANNELS - 1; ++i)
			pixel[i] += v[i] * weight;
		pixel[CHANNELS - 1] += weight;
	}

	void SetPixel(const u_int x, const u_int y, const T *v) {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		T *pixel = &pixels[(x + y * width) * CHANNELS];
		for (u_int i = 0; i < CHANNELS; ++i)
			pixel[i] = v[i];
	}

	T *GetPixel(const u_int x, const u_int y) const {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		return &pixels[(x + y * width) * CHANNELS];
	}

	T *GetPixel(const u_int index) const {
		assert (index >= 0);
		assert (index < width * height);

		return &pixels[index * CHANNELS];
	}

	u_int GetWidth() const { return width; }
	u_int GetHeight() const { return height; }
	u_int GetChannels() const { return CHANNELS; }

private:
	const u_int width, height;

	T *pixels;
};

}

#endif	/* _SLG_FRAMEBUFFER_H */

