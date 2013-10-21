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

#ifndef _LUXRAYS_SPECTRUM_H_H
#define _LUXRAYS_SPECTRUM_H_H

#include <ostream>

#include "luxrays/core/utils.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/spectrum_types.cl"
}

class Spectrum {
public:
	// Spectrum Public Methods
	Spectrum(const float rr, const float gg, const float bb)
		: r(rr), g(gg), b(bb) {
	}

	Spectrum(const float rgb[3])
		: r(rgb[0]), g(rgb[1]), b(rgb[2]) {
	}

	Spectrum(const float v)
		: r(v), g(v), b(v) {
	}

	Spectrum()
		: r(0.f), g(0.f), b(0.f) {
	}

	Spectrum operator+(const Spectrum &v) const {
		return Spectrum(r + v.r, g + v.g, b + v.b);
	}

	Spectrum operator*(const Spectrum &v) const {
		return Spectrum(r * v.r, g * v.g, b * v.b);
	}

	Spectrum & operator*=(const Spectrum &v) {
		r *= v.r;
		g *= v.g;
		b *= v.b;
		return *this;
	}

	Spectrum & operator+=(const Spectrum &v) {
		r += v.r;
		g += v.g;
		b += v.b;
		return *this;
	}

	Spectrum operator-(const Spectrum &v) const {
		return Spectrum(r - v.r, g - v.g, b - v.b);
	}

	Spectrum & operator-=(const Spectrum &v) {
		r -= v.r;
		g -= v.g;
		b -= v.b;
		return *this;
	}

	bool operator==(const Spectrum &v) const {
		return r == v.r && g == v.g && b == v.b;
	}

	Spectrum operator*(float f) const {
		return Spectrum(f * r, f * g, f * b);
	}

	Spectrum & operator*=(float f) {
		r *= f;
		g *= f;
		b *= f;
		return *this;
	}

	Spectrum operator/(float f) const {
		float inv = 1.f / f;
		return Spectrum(r * inv, g * inv, b * inv);
	}

	Spectrum & operator/=(float f) {
		float inv = 1.f / f;
		r *= inv;
		g *= inv;
		b *= inv;
		return *this;
	}

	Spectrum operator/(const Spectrum &s) const {
		return Spectrum(r / s.r, g / s.g, b / s.b);
	}

	Spectrum & operator/=(const Spectrum &s) {
		r /= s.r;
		g /= s.g;
		b /= s.b;
		return *this;
	}

	Spectrum operator-() const {
		return Spectrum(-r, -g, -b);
	}

	float operator[](int i) const {
		return (&r)[i];
	}

	float &operator[](int i) {
		return (&r)[i];
	}

	float Filter() const {
		return luxrays::Max<float>(r, luxrays::Max<float>(g, b));
	}

	bool Black() const {
		return (r == 0.f) && (g == 0.f) && (b == 0.f);
	}

	bool IsNaN() const {
		return isnan(r) || isnan(g) || isnan(b);
	}

	bool IsInf() const {
		return isinf(r) || isinf(g) || isinf(b);
	}

	float Y() const {
		return 0.212671f * r + 0.715160f * g + 0.072169f * b;
	}

	Spectrum Clamp(const float min = 0.f, const float max = 1.f) const {
		Spectrum s(
			luxrays::Clamp(r, min, max),
			luxrays::Clamp(g, min, max),
			luxrays::Clamp(b, min, max));

		return s;
	}

	float r, g, b;
};

inline std::ostream &operator<<(std::ostream &os, const Spectrum &v) {
	os << "Spectrum[" << v.r << ", " << v.g << ", " << v.b << "]";
	return os;
}

inline Spectrum operator*(float f, const Spectrum &v) {
	return v * f;
}

inline Spectrum Exp(const Spectrum &s) {
	return Spectrum(expf(s.r), expf(s.g), expf(s.b));
}

inline Spectrum Sqrt(const Spectrum &s) {
	return Spectrum(sqrtf(s.r), sqrtf(s.g), sqrtf(s.b));
}

}

#endif	/* _SLG_SPECTRUM_H */
