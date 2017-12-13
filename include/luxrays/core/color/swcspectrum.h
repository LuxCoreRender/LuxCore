/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#ifndef _LUXRAYS_SWCSPECTRUM_H
#define _LUXRAYS_SWCSPECTRUM_H

#include <boost/serialization/access.hpp>

#include "luxrays/core/color/color.h"
#include "luxrays/core/color/spd.h"
#include "luxrays/core/color/spectrumwavelengths.h"

namespace luxrays {

#define Scalar float

class SWCSpectrum {
	friend class boost::serialization::access;
public:
	// SWCSpectrum Public Methods
	SWCSpectrum(Scalar v = 0.f) {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			c[i] = v;
	}

	// VS2013 generates invalid code for the default copy constructor here
	// when the Inline Function Expansion optimization is enabled.
	//
	// Now the problem seems to be present with GCC 4.8 too.

	SWCSpectrum(const SWCSpectrum &s) {
#if defined (WIN32)
		std::copy(s.c, s.c + WAVELENGTH_SAMPLES, c);
#else
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			c[i] = s.c[i];
#endif
	}

	SWCSpectrum(const SpectrumWavelengths &sw, const RGBColor &s);

	SWCSpectrum(const SpectrumWavelengths &sw, const SPD &s);

	SWCSpectrum(const float cs[WAVELENGTH_SAMPLES]) {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			c[i] = cs[i];
	}

	SWCSpectrum operator+(const SWCSpectrum &s2) const {
		SWCSpectrum ret = *this;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] += s2.c[i];
		return ret;
	}
	SWCSpectrum &operator+=(const SWCSpectrum &s2) {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			c[i] += s2.c[i];
		return *this;
	}
	// Needed for addition of textures
	SWCSpectrum operator+(Scalar a) const {
		SWCSpectrum ret = *this;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] += a;
		return ret;
	}
	// Needed for addition of textures
	friend inline
	SWCSpectrum operator+(Scalar a, const SWCSpectrum &s) {
		return s + a;
	}
	SWCSpectrum operator-(const SWCSpectrum &s2) const {
		SWCSpectrum ret = *this;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] -= s2.c[i];
		return ret;
	}
	SWCSpectrum &operator-=(const SWCSpectrum &s2) {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			c[i] -= s2.c[i];
		return *this;
	}
	// Needed for subtraction of textures
	friend inline
	SWCSpectrum operator-(Scalar a, const SWCSpectrum &s) {
		return s - a;
	}
	// Needed for subtraction of textures
	SWCSpectrum operator-(Scalar a) const {
		SWCSpectrum ret = *this;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] -= a;
		return ret;
	}
	SWCSpectrum operator/(const SWCSpectrum &s2) const {
		SWCSpectrum ret = *this;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] /= s2.c[i];
		return ret;
	}
	SWCSpectrum &operator/=(const SWCSpectrum &sp) {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			c[i] /= sp.c[i];
		return *this;
	}
	SWCSpectrum operator*(const SWCSpectrum &sp) const {
		SWCSpectrum ret = *this;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] *= sp.c[i];
		return ret;
	}
	SWCSpectrum &operator*=(const SWCSpectrum &sp) {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			c[i] *= sp.c[i];
		return *this;
	}
	SWCSpectrum operator*(Scalar a) const {
		SWCSpectrum ret = *this;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] *= a;
		return ret;
	}
	SWCSpectrum &operator*=(Scalar a) {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			c[i] *= a;
		return *this;
	}
	friend inline
	SWCSpectrum operator*(Scalar a, const SWCSpectrum &s) {
		return s * a;
	}
	SWCSpectrum operator/(Scalar a) const {
		return *this * (1.f / a);
	}
	SWCSpectrum &operator/=(Scalar a) {
		return *this *= (1.f / a);
	}
	void AddWeighted(Scalar w, const SWCSpectrum &s) {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			c[i] += w * s.c[i];
	}
	bool operator==(const SWCSpectrum &sp) const {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			if (c[i] != sp.c[i]) return false;
		return true;
	}
	bool operator!=(const SWCSpectrum &sp) const {
		return !(*this == sp);
	}
	bool Black() const {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			if (c[i] != 0.f) return false;
		return true;
	}
    Scalar Max() const {
        Scalar result = c[0];
        for (int i = 1; i < WAVELENGTH_SAMPLES; i++)
            result = std::max(result, c[i]);
        return result;
    }
    Scalar Min() const {
        Scalar result = c[0];
        for (int i = 1; i < WAVELENGTH_SAMPLES; i++)
            result = std::min(result, c[i]);
        return result;
    }
	friend SWCSpectrum Sqrt(const SWCSpectrum &s) {
		SWCSpectrum ret;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] = sqrtf(s.c[i]);
		return ret;
	}
	friend SWCSpectrum Pow(const SWCSpectrum &s, const SWCSpectrum &e) {
		SWCSpectrum ret;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] = powf(s.c[i], e.c[i]);
		return ret;
	}
	friend SWCSpectrum Pow(const SWCSpectrum &s, float e) {
		SWCSpectrum ret;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] = powf(s.c[i], e);
		return ret;
	}
	SWCSpectrum operator-() const {
		SWCSpectrum ret;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] = -c[i];
		return ret;
	}
	friend SWCSpectrum Exp(const SWCSpectrum &s) {
		SWCSpectrum ret;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] = expf(s.c[i]);
		return ret;
	}
	friend SWCSpectrum Ln(const SWCSpectrum &s) {
		SWCSpectrum ret;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] = logf(s.c[i]);
		return ret;
	}
	SWCSpectrum Clamp(Scalar low = 0.f,
	               Scalar high = INFINITY) const {
		SWCSpectrum ret;
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ret.c[i] = luxrays::Clamp(c[i], low, high);
		return ret;
	}
	bool IsNaN() const {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			if (isnan(c[i])) return true;
		return false;
	}
	bool IsInf() const {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			if (isinf(c[i])) return true;
		return false;
	}
	Scalar Y(const SpectrumWavelengths &sw) const;
	inline Scalar Filter(const SpectrumWavelengths &sw) const;

	friend std::ostream &operator<<(std::ostream &, const SWCSpectrum &);

	// SWCSpectrum Public Data
	Scalar c[WAVELENGTH_SAMPLES];
	
private:
	template<class Archive> void serialize(Archive & ar, const unsigned int version) {
		for (int i = 0; i < WAVELENGTH_SAMPLES; ++i)
			ar & c[i];
	}
};

// This is one of the most used functions so make it an inline candidate
// However it requires SpectrumWavelengths to be fully defined
inline Scalar SWCSpectrum::Filter(const SpectrumWavelengths &sw) const {
	if (sw.single)
		return c[sw.single_w];
	Scalar result = 0.f;
	for (u_int i = 0; i < WAVELENGTH_SAMPLES; ++i)
		result += c[i];
	return result * inv_WAVELENGTH_SAMPLES;
}

}

#endif // _LUXRAYS_SWCSPECTRUM_H
