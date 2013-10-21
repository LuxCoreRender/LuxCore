/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
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

	void SetWeightedPixel(const u_int x, const u_int y, const T *v, const float weight) {
		assert (x >= 0);
		assert (x < width);
		assert (y >= 0);
		assert (y < height);

		T *pixel = &pixels[(x + y * width) * CHANNELS];
		for (u_int i = 0; i < CHANNELS - 1; ++i)
			pixel[i] = v[i];
		pixel[CHANNELS - 1] = weight;
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

