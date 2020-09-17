/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_COLOR_H
#define _LUXRAYS_COLOR_H

#include <cmath>
#include <ostream>

#include "luxrays/utils/utils.h"
#include "luxrays/utils/serializationutils.h"


namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/color/color_types.cl"
}

class SPD;
class SpectrumWavelengths;
class SWCSpectrum;

// Color Declarations
class Color {
public:
#define COLOR_SAMPLES 3

	// Color Public Methods
	Color() { };

	Color &operator+=(const Color &s2) {
		c[0] += s2.c[0]; c[1] += s2.c[1]; c[2] += s2.c[2];
		return *this;
	}
	Color &operator+=(float f) {
		c[0] += f; c[1] += f; c[2] += f;
		return *this;
	}
	Color operator+(const Color &s2) const {
		Color ret = *this;
		ret.c[0] += s2.c[0]; ret.c[1] += s2.c[1]; ret.c[2] += s2.c[2];
		return ret;
	}
	Color &operator-=(const Color &s2) {
		c[0] -= s2.c[0]; c[1] -= s2.c[1]; c[2] -= s2.c[2];
		return *this;
	}
	Color &operator-=(float f) {
			c[0] -= f; c[1] -= f; c[2] -= f;
		return *this;
	}
	Color operator-(const Color &s2) const {
		Color ret = *this;
		ret.c[0] -= s2.c[0]; ret.c[1] -= s2.c[1]; ret.c[2] -= s2.c[2];
		return ret;
	}
	Color operator/(const Color &s2) const {
		Color ret = *this;
		ret.c[0] *= 1.f / s2.c[0]; ret.c[1] *= 1.f / s2.c[1]; ret.c[2] *= 1.f / s2.c[2];
		return ret;
	}
	Color &operator/=(const Color &sp) {
		c[0] *= 1.f / sp.c[0]; c[1] *= 1.f / sp.c[1]; c[2] *= 1.f / sp.c[2];
		return *this;
	}
	Color operator*(const Color &sp) const {
		Color ret = *this;
		ret.c[0] *= sp.c[0]; ret.c[1] *= sp.c[1]; ret.c[2] *= sp.c[2];
		return ret;
	}
	Color &operator*=(const Color &sp) {
		c[0] *= sp.c[0]; c[1] *= sp.c[1]; c[2] *= sp.c[2];
		return *this;
	}
	Color operator*(float a) const {
		Color ret = *this;
		ret.c[0] *= a; ret.c[1] *= a; ret.c[2] *= a;
		return ret;
	}
	Color &operator*=(float a) {
		c[0] *= a; c[1] *= a; c[2] *= a;
		return *this;
	}
	friend inline Color operator*(float a, const Color &s) {
		return s * a;
	}
	Color operator/(float a) const {
		return *this * (1.f / a);
	}
	Color &operator/=(float a) {
		float inv = 1.f / a;
		c[0] *= inv; c[1] *= inv; c[2] *= inv;
		return *this;
	}
	void AddWeighted(float w, const Color &s) {
		c[0] += w * s.c[0]; c[1] += w * s.c[1]; c[2] += w * s.c[2];
	}
	bool operator==(const Color &sp) const {
		if (c[0] != sp.c[0]) return false;
		if (c[1] != sp.c[1]) return false;
		if (c[2] != sp.c[2]) return false;
		return true;
	}
	bool operator!=(const Color &sp) const {
		return !(*this == sp);
	}
	Color Abs() const {
		Color ret;
		ret.c[0] = fabsf(c[0]);
		ret.c[1] = fabsf(c[1]);
		ret.c[2] = fabsf(c[2]);
		return ret;
	}
	bool Black() const {
		if (c[0] != 0.) return false;
		if (c[1] != 0.) return false;
		if (c[2] != 0.) return false;
		return true;
	}
	Color Sqrt() const {
		Color ret;
		ret.c[0] = sqrtf(c[0]);
		ret.c[1] = sqrtf(c[1]);
		ret.c[2] = sqrtf(c[2]);
		return ret;
	}
	Color Pow(const Color &e) const {
		Color ret;
		ret.c[0] = c[0] > 0 ? powf(c[0], e.c[0]) : 0.f;
		ret.c[1] = c[1] > 0 ? powf(c[1], e.c[1]) : 0.f;
		ret.c[2] = c[2] > 0 ? powf(c[2], e.c[2]) : 0.f;
		return ret;
	}
	Color Pow(float f) const {
		Color ret;
		ret.c[0] = c[0] > 0 ? powf(c[0], f) : 0.f;
		ret.c[1] = c[1] > 0 ? powf(c[1], f) : 0.f;
		ret.c[2] = c[2] > 0 ? powf(c[2], f) : 0.f;
		return ret;
	}
	Color operator-() const {
		Color ret;
		ret.c[0] = -c[0];
		ret.c[1] = -c[1];
		ret.c[2] = -c[2];
		return ret;
	}
	float Max() const {
		return luxrays::Max(c[0], luxrays::Max(c[1], c[2]));
	}
	float Min() const {
		return luxrays::Min(c[0], luxrays::Min(c[1], c[2]));
	}
	friend Color Sqrt(const Color &s) {
		Color ret;
		ret.c[0] = sqrtf(s.c[0]);
		ret.c[1] = sqrtf(s.c[1]);
		ret.c[2] = sqrtf(s.c[2]);
		return ret;
	}
	friend Color Exp(const Color &s) {
		Color ret;
		ret.c[0] = expf(s.c[0]);
		ret.c[1] = expf(s.c[1]);
		ret.c[2] = expf(s.c[2]);
		return ret;
	}
	friend Color Ln(const Color &s) {
		Color ret;
		ret.c[0] = logf(s.c[0]);
		ret.c[1] = logf(s.c[1]);
		ret.c[2] = logf(s.c[2]);
		return ret;
	}
	friend Color Pow(const Color &s, const Color &f) {
		Color ret;
		ret.c[0] = s.c[0] > 0 ? powf(s.c[0], f.c[0]) : 0.f;
		ret.c[1] = s.c[1] > 0 ? powf(s.c[1], f.c[1]) : 0.f;
		ret.c[2] = s.c[2] > 0 ? powf(s.c[2], f.c[2]) : 0.f;
		return ret;
	}
	friend Color Pow(const Color &s, const float f) {
		Color ret;
		ret.c[0] = s.c[0] > 0 ? powf(s.c[0], f) : 0.f;
		ret.c[1] = s.c[1] > 0 ? powf(s.c[1], f) : 0.f;
		ret.c[2] = s.c[2] > 0 ? powf(s.c[2], f) : 0.f;
		return ret;
	}
	Color Clamp(float low = 0.f, float high = INFINITY) const {
		Color ret;
		ret.c[0] = luxrays::Clamp(c[0], low, high);
		ret.c[1] = luxrays::Clamp(c[1], low, high);
		ret.c[2] = luxrays::Clamp(c[2], low, high);
		return ret;
	}
	Color ScaledClamp(float low = 0.f, float high = INFINITY) const {
		Color ret = *this;

		const float maxValue = Max();
		if (maxValue > 0.f) {
			if (maxValue > high) {
				const float scale = high / maxValue;

				ret.c[0] *= scale;
				ret.c[1] *= scale;
				ret.c[2] *= scale;
			}

			if (maxValue < low) {
				const float scale = low / maxValue;

				ret.c[0] *= scale;
				ret.c[1] *= scale;
				ret.c[2] *= scale;
			}
		}

		return ret;
	}
	bool IsNaN() const {
		if (isnan(c[0])) return true;
		if (isnan(c[1])) return true;
		if (isnan(c[2])) return true;
		return false;
	}
	bool IsInf() const {
		if (isinf(c[0])) return true;
		if (isinf(c[1])) return true;
		if (isinf(c[2])) return true;
		return false;
	}
	bool IsNeg() const {
		if (c[0] < 0.f) return true;
		if (c[1] < 0.f) return true;
		if (c[2] < 0.f) return true;
		return false;
	}
	bool IsValid() const {
		return !IsNaN() && !IsInf() && !IsNeg();
	}

	friend class boost::serialization::access;

	// Color Public Data
	float c[3];
	
private:
	template<class Archive> void serialize(Archive & ar, const unsigned int version) {
		for (int i = 0; i < 3; ++i)
			ar & c[i];
	}
};

// RGBColor Declarations
class RGBColor : public Color {
public:
	// RGBColor Public Methods
	RGBColor() { c[0] = 0.f; c[1] = 0.f; c[2] = 0.f; }
	RGBColor(float v) { c[0] = v; c[1] = v; c[2] = v; }
	RGBColor(float r, float g, float b) { c[0] = r; c[1] = g; c[2] = b; }
	RGBColor(const float cs[3]) {
		c[0] = cs[0]; c[1] = cs[1]; c[2] = cs[2];
	}
	RGBColor(const Color &color) { // so that operators work
		c[0] = color.c[0]; c[1] = color.c[1]; c[2] = color.c[2];
	}

	float Y() const {
		return 0.212671f * c[0] + 0.715160f * c[1] + 0.072169f * c[2];
	}
	float Filter() const { return (c[0] + c[1] + c[2]) * (1.f / 3.f); }

	//--------------------------------------------------------------------------
	// Required by OpenSubdiv interface
	//--------------------------------------------------------------------------

	void Clear(void * = 0) {
        c[0] = 0.f;
		c[1] = 0.f;
		c[2] = 0.f;
    }
	
	void AddWithWeight(RGBColor const &src, float weight) {
        c[0] += weight * src.c[0];
        c[1] += weight * src.c[1];
        c[2] += weight * src.c[2];
    }

	//--------------------------------------------------------------------------
};

// RGBAColor Declarations
class RGBAColor : public Color {
public:
	// RGBAColor Public Methods
	RGBAColor() { c[0] = 0.f; c[1] = 0.f; c[2] = 0.f; alpha = 0.f; }
	RGBAColor(float v) { c[0] = v; c[1] = v; c[2] = v; alpha = v; }
	RGBAColor(float r, float g, float b) { c[0] = r; c[1] = g; c[2] = b; alpha = 0.f; }
	RGBAColor(float r, float g, float b, float a) { c[0] = r; c[1] = g; c[2] = b; alpha = a; }
	RGBAColor(const float cs[3]) {
		c[0] = cs[0]; c[1] = cs[1]; c[2] = cs[2]; alpha = 0.f;
	}
	RGBAColor(const Color &color) { // so that operators work
		c[0] = color.c[0]; c[1] = color.c[1]; c[2] = color.c[2]; alpha = 0.f;
	}

	float Y() const {
		return 0.212671f * c[0] + 0.715160f * c[1] + 0.072169f * c[2];
	}
	float Filter() const { return (c[0] + c[1] + c[2]) * (1.f / 3.f); }

	float alpha;
};

// XYZColor Declarations
class XYZColor : public Color {
    // Dade - serialization here is required by network rendering
    friend class boost::serialization::access;

public:
	// XYZColor Public Methods
	XYZColor() {
			c[0] = 0.f; c[1] = 0.f; c[2] = 0.f;
	}
	XYZColor(float v) {
			c[0] = v; c[1] = v; c[2] = v;
	}
	XYZColor(float x, float y, float z) {
		c[0] = x; c[1] = y; c[2] = z;
	}
	XYZColor(float cs[3]) {
			c[0] = cs[0]; c[1] = cs[1]; c[2] = cs[2];
	}
	XYZColor(const Color &color) { // so that operators work
		c[0] = color.c[0]; c[1] = color.c[1]; c[2] = color.c[2];
	}
	XYZColor(const SpectrumWavelengths &sw, const SWCSpectrum &s);
	XYZColor(const SPD &s);

	float Y() const {
		return c[1];
	}
};

//!
//! A colour system is defined by the CIE x and y coordinates of its
//! three primary illuminants and the x and y coordinates of the white
//! point.
//! The additional definition of the white point intensity allow for
//! intensity adaptation
//!
class ColorSystem {
public:
	// Default is SMPTE
	ColorSystem(float xR = .63f, float yR = .34f,
			float xG = .31f, float yG = .595f,
			float xB = .155f, float yB = .07f,
			float xW = .314275f, float yW = .329411f, float lum = 1.);

	//!
	//! \param[in] color A color in XYZ space
	//! \return The color converted in RGB space
	//!
	//! Determine the contribution of each primary in a linear combination
	//! which sums to the desired chromaticity.
	//!
	RGBColor ToRGB(const XYZColor &color) const {
		float c[3];
		c[0] = XYZToRGB[0][0] * color.c[0] + XYZToRGB[0][1] * color.c[1] + XYZToRGB[0][2] * color.c[2];
		c[1] = XYZToRGB[1][0] * color.c[0] + XYZToRGB[1][1] * color.c[1] + XYZToRGB[1][2] * color.c[2];
		c[2] = XYZToRGB[2][0] * color.c[0] + XYZToRGB[2][1] * color.c[1] + XYZToRGB[2][2] * color.c[2];
		return RGBColor(c);
	}

	//!
	//! \param[in] color A color in XYZ space
	//! \return The color converted in RGB space
	//!
	//! Determine the contribution of each primary in a linear combination
	//! which sums to the desired chromaticity. If the requested
	//! chromaticity falls outside the Maxwell triangle (colour gamut) formed
	//! by the three primaries, one of the R, G, or B weights will be
	//! negative. Use Constrain() to desaturate an outside-gamut colour to
	//! the closest representation within the available gamut.
	//! \sa Constrain
	//!
	RGBColor ToRGBConstrained(const XYZColor &color) const {
		RGBColor rgb(ToRGB(color));
		Constrain(color, rgb);
		return rgb;
	}

	//!
	//! \param[in] color A color in RGB space
	//! \return The color converted in XYZ space
	//!
	XYZColor ToXYZ(const RGBColor &color) const {
		float c[3];
		c[0] = RGBToXYZ[0][0] * color.c[0] + RGBToXYZ[0][1] * color.c[1] + RGBToXYZ[0][2] * color.c[2];
		c[1] = RGBToXYZ[1][0] * color.c[0] + RGBToXYZ[1][1] * color.c[1] + RGBToXYZ[1][2] * color.c[2];
		c[2] = RGBToXYZ[2][0] * color.c[0] + RGBToXYZ[2][1] * color.c[1] + RGBToXYZ[2][2] * color.c[2];
		return XYZColor(c);
	}

//protected:
	bool Constrain(const XYZColor &xyz, RGBColor &rgb) const;
	RGBColor Limit(const RGBColor &rgb, int method) const;

	static const ColorSystem DefaultColorSystem;

	float xRed, yRed; //!<Red coordinates
	float xGreen, yGreen; //!<Green coordinates
	float xBlue, yBlue; //!<Blue coordinates
	float xWhite, yWhite; //!<White coordinates
	float luminance; //!<White intensity
	float XYZToRGB[3][3]; //!<Corresponding conversion matrix from XYZ to RGB
	float RGBToXYZ[3][3]; //!<Corresponding conversion matrix from RGB to XYZ
};

//!
//! Color space white point conversion using Bradford matrices
//!
class ColorAdaptator {
public:
	//!
	//! \param[in] from initial color
	//! \param[in] to final color
	//!
	//! Construct the conversion matrix to convert corresponding
	//! to the provided initial and final colors with Bradford matrices
	//!
	ColorAdaptator(const XYZColor &from, const XYZColor &to);
	//!
	//! \param[in] color a color to convert
	//! \return the converted color in the new colorspace
	//!
	//! Converts a color in the new colorspace
	//!
	XYZColor Adapt(const XYZColor &color) const;
	//!
	//! \param[in] ca a color adaptator
	//! \return the composition of the 2 color adaptators
	//!
	//! Compose 2 color adaptators
	//!
	ColorAdaptator operator*(const ColorAdaptator &ca) const;
	//!
	//! \param[in] s a scaling factor
	//! \return a reference to self once scaled
	//!
	//! Scale the color adaptator
	//!
	ColorAdaptator &operator*=(float s);

private:
	float conv[3][3]; //!<Conversion matrix
};

// RGBColor Method Definitions
inline std::ostream &operator<<(std::ostream &os, const RGBColor &s) {
	os << "RGBColor[" << s.c[0] << ", " << s.c[1] << ", " << s.c[2] << "]";
	return os;
}

// RGBAColor Method Definitions
inline std::ostream &operator<<(std::ostream &os, const RGBAColor &s) {
	os << "RGBAColor[" << s.c[0] << ", " << s.c[1] << ", " << s.c[2] << ", " << s.alpha << "]";
	return os;
}

// XYZColor Method Definitions
inline std::ostream &operator<<(std::ostream &os, const XYZColor &s) {
	os << "XYZColor[" << s.c[0] << ", " << s.c[1] << ", " << s.c[2] << "]";
	return os;
}

typedef RGBColor Spectrum;

extern Spectrum TemperatureToWhitePoint(const float temperature, const bool normalize);

}

// Eliminate serialization overhead at the cost of
// never being able to increase the version.
BOOST_CLASS_IMPLEMENTATION(luxrays::Spectrum, boost::serialization::object_serializable)
BOOST_CLASS_EXPORT_KEY(luxrays::Spectrum)

#endif // _LUXRAYS_COLOR_H
