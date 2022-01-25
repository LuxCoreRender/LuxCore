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

#include "luxrays/core/color/color.h"
#include "luxrays/core/color/spectrumgroup.h"
#include "luxrays/core/geometry/matrix3x3.h"
#include "luxrays/core/color/spectrumwavelengths.h"
#include "luxrays/core/color/swcspectrum.h"
#include "luxrays/core/color/spds/data/xyzbasis.h"
#include "luxrays/core/color/spds/blackbodyspd.h"

using namespace luxrays;

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::Spectrum)
BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::SpectrumGroup)

XYZColor::XYZColor(const SpectrumWavelengths &sw, const SWCSpectrum &s) {
	if (sw.single) {
		const u_int j = sw.single_w;
		SpectrumWavelengths::spd_ciex.Sample(1, sw.binsXYZ + j,
			sw.offsetsXYZ + j, c);
		SpectrumWavelengths::spd_ciey.Sample(1, sw.binsXYZ + j,
			sw.offsetsXYZ + j, c + 1);
		SpectrumWavelengths::spd_ciez.Sample(1, sw.binsXYZ + j,
			sw.offsetsXYZ + j, c + 2);
		c[0] *= s.c[j] * WAVELENGTH_SAMPLES;
		c[1] *= s.c[j] * WAVELENGTH_SAMPLES;
		c[2] *= s.c[j] * WAVELENGTH_SAMPLES;
	} else {
		SWCSpectrum x, y, z;
		SpectrumWavelengths::spd_ciex.Sample(WAVELENGTH_SAMPLES,
			sw.binsXYZ, sw.offsetsXYZ, x.c);
		SpectrumWavelengths::spd_ciey.Sample(WAVELENGTH_SAMPLES,
			sw.binsXYZ, sw.offsetsXYZ, y.c);
		SpectrumWavelengths::spd_ciez.Sample(WAVELENGTH_SAMPLES,
			sw.binsXYZ, sw.offsetsXYZ, z.c);
		c[0] = c[1] = c[2] = 0.f;
		for (u_int j = 0; j < WAVELENGTH_SAMPLES; ++j) {
			c[0] += x.c[j] * s.c[j];
			c[1] += y.c[j] * s.c[j];
			c[2] += z.c[j] * s.c[j];
		}
	}
}

XYZColor::XYZColor(const SPD &s) {
	c[0] = c[1] = c[2] = 0.f;
	for (u_int i = 0; i < nCIE; ++i) {
		const float v = s.Sample(i + CIEstart);
		c[0] += v * CIE_X[i];
		c[1] += v * CIE_Y[i];
		c[2] += v * CIE_Z[i];
	}
	*this *= 683.f;
}

static float dot(const float a[3], const float b[3]) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

//!
//! \param[in] xR x value of red in xyY space
//! \param[in] yR y value of red in xyY space
//! \param[in] xG x value of green in xyY space
//! \param[in] yG y value of green in xyY space
//! \param[in] xB x value of blue in xyY space
//! \param[in] yB y value of blue in xyY space
//! \param[in] xW x value of white in xyY space
//! \param[in] yW y value of white in xyY space
//! \param[in] lum Y (intensity) value of white in xyY space
//!
//! Initialize a colorspace conversion instance by providing reference values
//! of red, green, blue and white point.
//! This functions computes the corresponding conversion matrix from XYZ
//! space to RGB space.
//!
ColorSystem::ColorSystem(float xR, float yR, float xG, float yG, float xB, float yB,
	float xW, float yW, float lum)
	: xRed(xR), yRed(yR), xGreen(xG), yGreen(yG), xBlue(xB), yBlue(yB),
	xWhite(xW), yWhite(yW), luminance(lum) {
	const float red[3] = { xRed / yRed, 1.f, (1.f - xRed - yRed) / yRed };
	const float green[3] = { xGreen / yGreen, 1.f, (1.f - xGreen - yGreen) / yGreen };
	const float blue[3] = { xBlue / yBlue, 1.f, (1.f - xBlue - yBlue) / yBlue };
	const float white[3] = { xWhite / yWhite, 1.f, (1.f - xWhite - yWhite) / yWhite };
	float rgb[3][3];
	rgb[0][0] = red[0]; rgb[1][0] = red[1]; rgb[2][0] = red[2];
	rgb[0][1] = green[0]; rgb[1][1] = green[1]; rgb[2][1] = green[2];
	rgb[0][2] = blue[0]; rgb[1][2] = blue[1]; rgb[2][2] = blue[2];
	Invert3x3(rgb, rgb);
	float y[3];
	Transform3x3(rgb, white, y);
	float x[3] = {y[0] * red[0], y[1] * green[0], y[2] * blue[0]};
	float z[3] = {y[0] * red[2], y[1] * green[2], y[2] * blue[2]};
	rgb[0][0] = x[0] + white[0]; rgb[1][0] = x[1] + white[0]; rgb[2][0] = x[2] + white[0];
	rgb[0][1] = y[0] + white[1]; rgb[1][1] = y[1] + white[1]; rgb[2][1] = y[2] + white[1];
	rgb[0][2] = z[0] + white[2]; rgb[1][2] = z[1] + white[2]; rgb[2][2] = z[2] + white[2];
	float matrix[3][3];
	matrix[0][0] = (dot(x, x) + white[0] * white[0]) * luminance;
	matrix[1][0] = (dot(x, y) + white[1] * white[0]) * luminance;
	matrix[2][0] = (dot(x, z) + white[2] * white[0]) * luminance;
	matrix[0][1] = (dot(y, x) + white[0] * white[1]) * luminance;
	matrix[1][1] = (dot(y, y) + white[1] * white[1]) * luminance;
	matrix[2][1] = (dot(y, z) + white[2] * white[1]) * luminance;
	matrix[0][2] = (dot(z, x) + white[0] * white[2]) * luminance;
	matrix[1][2] = (dot(z, y) + white[1] * white[2]) * luminance;
	matrix[2][2] = (dot(z, z) + white[2] * white[2]) * luminance;
	Invert3x3(matrix, matrix);
	//C=R*Tt*(T*Tt)^-1
	Multiply3x3(rgb, matrix, XYZToRGB);
	Invert3x3(XYZToRGB, RGBToXYZ);
}

//!
//! \param[in] color  A RGB color possibly unrepresentable
//!
//! Test whether a requested colour is within the gamut achievable with
//! the primaries of the current colour system. This amounts simply to
//! testing whether all the primary weights are non-negative.
//!
static inline bool LowGamut(const RGBColor &color) {
    return color.c[0] < 0.f || color.c[1] < 0.f || color.c[2] < 0.f;
}

//!
//! \param[in] color  A RGB color possibly unrepresentable
//!
//! Test whether a requested colour is within the gamut achievable with
//! the primaries of the current colour system. This amounts simply to
//! testing whether all the primary weights are at most 1.
//!
static inline bool HighGamut(const RGBColor &color) {
    return color.c[0] > 1.f || color.c[1] > 1.f || color.c[2] > 1.f;
}

//!
//! \param[in] xyz The color in XYZ space
//! \param[in,out] rgb The same color in RGB space
//! \return Whether the RGB representation was modified or not
//! \retval true The color has been modified
//! \retval false The color was inside the representable gamut:
//! no modification occurred
//!
//! If the requested RGB shade contains a negative weight for one of
//! the primaries, it lies outside the colour gamut accessible from the
//! given triple of primaries. Desaturate it by mixing with the white
//! point of the colour system so as to reduce the primary with the
//! negative weight to zero. This is equivalent to finding the
//! intersection on the CIE diagram of a line drawn between the white
//! point and the requested colour with the edge of the Maxwell
//! triangle formed by the three primaries.
//! This function tries not to change the overall intensity, only the tint
//! is shifted to be inside the representable gamut.
//!
bool ColorSystem::Constrain(const XYZColor &xyz, RGBColor &rgb) const {
	bool constrain = false;
	// Is the contribution of one of the primaries negative ?
	if (LowGamut(rgb)) {
		// Compute the xyY color coordinates
		const float YComp = xyz.Y();
		if (!(YComp > 0.f)) {
			rgb = RGBColor(0.f);
			return true;
		}
		float xComp = xyz.c[0] / (xyz.c[0] + xyz.c[1] + xyz.c[2]);
		float yComp = xyz.c[1] / (xyz.c[0] + xyz.c[1] + xyz.c[2]);

		// Define the Blue to Red (BR) line equation
		const float aBR = (yRed - yBlue) / (xRed - xBlue);
		const float bBR = yBlue - (aBR * xBlue);

		// Define the Blue to Green (BG) line equation
		const float aBG = (yGreen - yBlue) / (xGreen - xBlue);
		const float bBG = yBlue - (aBG * xBlue);

		// Define the Green to Red (GR) line equation
		const float aGR = (yRed - yGreen) / (xRed - xGreen);
		const float bGR = yGreen - (aGR * xGreen);

		// Color below the (BR) line
		if (yComp < (aBR * xComp + bBR)) {
			// Compute the orthogonal to (BR) through the color coordinates
			const float aOrtho = -1.f / aBR;
			const float bOrtho = -aOrtho * xComp + yComp;

			// Compute the intersection point with (BR)
			xComp = (bOrtho - bBR) / (aBR - aOrtho);
			yComp = aBR * xComp + bBR;

			// If the computed point is not on the gamut limits, clamp to a primary
			if (xComp < xBlue) {
				xComp = xBlue;
				yComp = yBlue;
			} else if (xComp > xRed) {
				xComp = xRed;
				yComp = yRed;
			}
		// Color over the (BG) line
		} else if (yComp > (aBG * xComp + bBG)) {
			// Compute the orthogonal to (BG) through the color coordinates
			const float aOrtho = -1.f / aBG;
			const float bOrtho = -aOrtho * xComp + yComp;

			// Compute the intersection point with (BG)
			xComp = (bOrtho - bBG) / (aBG - aOrtho);
			yComp = aBG * xComp + bBG;

			// If the computed point is not on the gamut limits, clamp to a primary
			if (xComp < xBlue) {
				xComp = xBlue;
				yComp = yBlue;
			} else if (xComp > xGreen) {
				xComp = xGreen;
				yComp = yGreen;
			}
		// Color over the (GR) line
		} else if (yComp > (aGR * xComp + bGR)) {
			// Compute the orthogonal to (GR) through the color coordinates
			const float aOrtho = -1.f / aGR;
			const float bOrtho = -aOrtho * xComp + yComp;

			// Compute the intersection point with (GR)
			xComp = (bOrtho - bGR) / (aGR - aOrtho);
			yComp = aGR * xComp + bGR;

			// If the computed point is not on the gamut limits, clamp to a primary
			if (xComp < xGreen) {
				xComp = xGreen;
				yComp = yGreen;
			} else if (xComp > xRed) {
				xComp = xRed;
				yComp = yRed;
			}
		}

		// Recompute the XYZ color from the xyY values
		XYZColor disp;
		disp.c[0] = (xComp * YComp) / yComp;
		disp.c[1] = YComp;
		disp.c[2] = (1.f - xComp - yComp) * YComp / yComp;

		// Convert to RGB
		rgb = ToRGB(disp);
		constrain = true;	// Colour modified to fit RGB gamut
	}

	return constrain;
}

RGBColor ColorSystem::Limit(const RGBColor &rgb, int method) const {
	if (HighGamut(rgb)) {
		switch (method) {
		case 2:
			return rgb.Clamp(0.f, 1.f);
		case 3:
			return Lerp(1.f / Max(rgb.c[0], Max(rgb.c[1], rgb.c[2])),
				RGBColor(0.f), rgb);
		default:
		{
			const float lum = (method == 0) ?
				(RGBToXYZ[1][0] * rgb.c[0] +
				RGBToXYZ[1][1] * rgb.c[1] +
				RGBToXYZ[1][2] * rgb.c[2]) :
				(luminance / 3.f);
			if (lum > luminance)
				return RGBColor(1.f);

			// Find the primary with greater weight and calculate
			// the parameter of the point on the vector from
			// the white point to the original requested colour
			// in RGB space.
			const float l = lum / luminance;
			float parameter;
			if (rgb.c[0] > rgb.c[1] && rgb.c[0] > rgb.c[2]) {
				parameter = (1.f - l) / (rgb.c[0] - l);
			} else if (rgb.c[1] > rgb.c[2]) {
				parameter = (1.f - l) / (rgb.c[1] - l);
			} else {
				parameter = (1.f - l) / (rgb.c[2] - l);
			}

			// Now finally compute the limited RGB weights.
			return Lerp(parameter, RGBColor(l), rgb);
		}
		}
	}
	return rgb;
}

const ColorSystem ColorSystem::DefaultColorSystem;

static const float bradford[3][3] = {
	{0.8951f, 0.2664f, -0.1614f},
	{-0.7502f, 1.7135f, 0.0367f},
	{0.0389f, -0.0685f, 1.0296f}};
static const float invBradford[3][3] = {
	{0.9869929f, -0.1470543f, 0.1599627f},
	{0.4323053f, 0.5183603f, 0.0492912f},
	{-0.0085287f, 0.0400428f, 0.9684867f}};
ColorAdaptator::ColorAdaptator(const XYZColor &from, const XYZColor &to) {
	const float mat[3][3] = {
		{to.c[0] / from.c[0], 0.f, 0.f},
		{0.f, to.c[1] / from.c[1], 0.f},
		{0.f, 0.f, to.c[2] / from.c[2]}};
	float temp[3][3];
	Multiply3x3(mat, bradford, temp);
	Multiply3x3(invBradford, temp, conv);
}

XYZColor ColorAdaptator::Adapt(const XYZColor &color) const {
	XYZColor result;
	Transform3x3(conv, color.c, result.c);
	return result;
}

ColorAdaptator ColorAdaptator::operator*(const ColorAdaptator &ca) const {
	ColorAdaptator result(XYZColor(1.f), XYZColor(1.f));
	Multiply3x3(conv, ca.conv, result.conv);
	return result;
}

ColorAdaptator &ColorAdaptator::operator*=(float s) {
	for(u_int i = 0; i < 3; ++i) {
		for(u_int j = 0; j < 3; ++j)
			conv[i][j] *= s;
	}
	return *this;
}

Spectrum luxrays::TemperatureToWhitePoint(const float temperature, const bool normalize) {
    BlackbodySPD spd(temperature);

    XYZColor colorTemp = spd.ToXYZ();

    ColorSystem colorSpace;
    Spectrum scale = colorSpace.ToRGBConstrained(colorTemp).Clamp(0.f);

	// To find the normalization factor below

	/*float maxValue = 0.f;
	for (u_int i = 0; i < 13000; i += 1) {
		BlackbodySPD spd(i);

		ColorSystem colorSpace;
		Spectrum s = colorSpace.ToRGBConstrained(spd.ToXYZ()).Clamp(0.f);
		maxValue = Max(maxValue, s.Max());
	}
	cout << maxValue << "\n";*/

	// To normalize scale, divide by maxValue
	if (normalize)
		scale /= 89159.6f;

    return scale;
}
