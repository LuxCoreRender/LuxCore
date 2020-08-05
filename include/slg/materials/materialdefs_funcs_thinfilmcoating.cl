#line 2 "materialdefs_funcs_thinfilmcoating.cl"

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

//---------------------------------------------------------------------------------
// Thin film coating (utility functions, to be used in materials where appropriate)
//---------------------------------------------------------------------------------

// A reduced set of the CIE tables from luxrays/core/color/spds/data/xyzbasis.h, 
// in the range specified by MIN_WAVELENGTH and MAX_WAVELENGTH, with WAVELENGTH_STEP 
// intervals instead of 1 nm intervals.
// Pre-computed with the function at the bottom of this file.
__constant float CIE_X_reduced[] = {
	0.001231153961271f,
	0.003753236029297f,
	0.012869009748101f,
	0.038468241691589f,
	0.120767198503017f,
	0.271807909011841f,
	0.346129596233368f,
	0.339094102382660f,
	0.297257900238037f,
	0.206811502575874f,
	0.104297898709774f,
	0.036412831395864f,
	0.006296345964074f,
	0.006982080172747f,
	0.055378608405590f,
	0.153854206204414f,
	0.277101695537567f,
	0.418311506509781f,
	0.577821493148804f,
	0.745518803596497f,
	0.902318120002747f,
	1.018006443977356f,
	1.062909603118896f,
	1.013047695159912f,
	0.872919976711273f,
	0.663810372352600f,
	0.466193914413452f,
	0.298001110553741f,
	0.174832299351692f,
	0.093330606818199f,
	0.049818608909845f,
	0.024452749639750f,
	0.012129199691117f,
	0.006195407826453f,
};

__constant float CIE_Y_reduced[] = {
	0.000035215209209f,
	0.000106136103568f,
	0.000357266690116f,
	0.001069879974239f,
	0.003526400076225f,
	0.010664430446923f,
	0.021718239411712f,
	0.036225710064173f,
	0.057458721101284f,
	0.087232798337936f,
	0.133452802896500f,
	0.199417993426323f,
	0.308578014373779f,
	0.482939511537552f,
	0.690842390060425f,
	0.849491596221924f,
	0.947225213050842f,
	0.992811620235443f,
	0.996898710727692f,
	0.958134889602661f,
	0.879781603813171f,
	0.769154727458954f,
	0.643844783306122f,
	0.515632271766663f,
	0.393032014369965f,
	0.275624513626099f,
	0.182974398136139f,
	0.112769097089767f,
	0.064769759774208f,
	0.034200478345156f,
	0.018120689317584f,
	0.008846157230437f,
	0.004380074795336f,
	0.002237275009975f,
};

__constant float CIE_Z_reduced[] = {
	0.005802907049656f,
	0.017730150371790f,
	0.060998030006886f,
	0.183256804943085f,
	0.579530298709869f,
	1.323929548263550f,
	1.730382084846497f,
	1.775867104530334f,
	1.688737154006958f,
	1.338736176490784f,
	0.856619298458099f,
	0.491967290639877f,
	0.286168605089188f,
	0.168560802936554f,
	0.083845309913158f,
	0.044898588210344f,
	0.021989250555634f,
	0.009529305621982f,
	0.004202399868518f,
	0.002196799963713f,
	0.001683066948317f,
	0.001146667054854f,
	0.000842560024466f,
	0.000383466685889f,
	0.000202186696697f,
	0.000056933331507f,
	0.000021813330022f,
	0.000001333332989f,
	0.000000000000000f,
	0.000000000000000f,
	0.000000000000000f,
	0.000000000000000f,
	0.000000000000000f,
	0.000000000000000f,
};

//---------------------------------------------------------------------------------
// ColorSystem
//---------------------------------------------------------------------------------

__constant float ThinFilmCoating_XYZToRGB[3][3] = {
	{
		2.868713378906250f,
		-1.423546671867371f,
		-0.445165514945984f,
	},
	{
		-1.132583618164062f,
		2.095324993133545f,
		0.037260532379150f,
	},
	{
		0.061925172805786f,
		-0.216627478599548f,
		1.154703855514526f,
	},
};

OPENCL_FORCE_INLINE float3 ThinFilmCoating_ConvertXYZToRGB(const float3 xyzColor) {
	return MAKE_FLOAT3(
		ThinFilmCoating_XYZToRGB[0][0] * xyzColor.x + ThinFilmCoating_XYZToRGB[0][1] * xyzColor.y + ThinFilmCoating_XYZToRGB[0][2] * xyzColor.z,
		ThinFilmCoating_XYZToRGB[1][0] * xyzColor.x + ThinFilmCoating_XYZToRGB[1][1] * xyzColor.y + ThinFilmCoating_XYZToRGB[1][2] * xyzColor.z,
		ThinFilmCoating_XYZToRGB[2][0] * xyzColor.x + ThinFilmCoating_XYZToRGB[2][1] * xyzColor.y + ThinFilmCoating_XYZToRGB[2][2] * xyzColor.z
	);
}

OPENCL_FORCE_INLINE bool LowGamut(const float3 color) {
    return color.x < 0.f || color.y < 0.f || color.z < 0.f;
}

OPENCL_FORCE_INLINE float3 ThinFilmCoating_Constrain(const float3 xyz, const float3 rgb) {
	const float xRed = 0.63f;
	const float yRed = 0.34f;
	const float xGreen = 0.31f;
	const float yGreen = 0.595f;
	const float xBlue = 0.155f;
	const float yBlue = 0.07f;
	// const float xWhite = 1.f / 3.f;
	// const float yWhite = 1.f / 3.f;
	// const float luminance = 1.f;
	
	// Is the contribution of one of the primaries negative?
	if (!LowGamut(rgb)) {
		return rgb;
	}
		
	// Compute the xyY color coordinates
	const float YComp = xyz.y;
	if (!(YComp > 0.f)) {
		return TO_FLOAT3(0.f);
	}
	float xComp = xyz.x / (xyz.x + xyz.y + xyz.z);
	float yComp = xyz.y / (xyz.x + xyz.y + xyz.z);

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
	float3 disp = MAKE_FLOAT3(
		(xComp * YComp) / yComp,
		YComp,
		(1.f - xComp - yComp) * YComp / yComp
	);

	// Convert to RGB
	return ThinFilmCoating_ConvertXYZToRGB(disp);
}

//---------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 CalcFilmColor(const float3 localFixedDir, const float filmThickness, const float filmIOR) {
	// Prevent wrong values if the ratio between IOR and thickness is too high
	if (filmThickness * (filmIOR - .4f) > 2000.f)
		return TO_FLOAT3(.5f);
		
	// 24 wavelengths seem to do the job. Any less and artifacs begin to appear at thickness around 2000 nm
	// Using 34 now so I can use integers for the wavelengths, and 720 - 380 = 340.
	const uint NUM_WAVELENGTHS = 34;
	const uint MIN_WAVELENGTH = 380;
	//const uint MAX_WAVELENGTH = 720;
	const uint WAVELENGTH_STEP = 10;  // (MAX_WAVELENGTH - MIN_WAVELENGTH) / (NUM_WAVELENGTHS - 1);
	
	const float sinTheta = SinTheta(localFixedDir);
	const float s = sqrt(fmax(0.f, Sqr(filmIOR) - Sqr(sinTheta)));

	float3 xyzColor = MAKE_FLOAT3(0.f, 0.f, 0.f);

	for (uint i = 0; i < NUM_WAVELENGTHS; ++i) {
		const uint waveLength = WAVELENGTH_STEP * i + MIN_WAVELENGTH;
		
		const float pd = (4.f * M_PI_F * filmThickness / (float)waveLength) * s + M_PI_F;
		const float intensity = Sqr(cos(pd));
		
		xyzColor.x += intensity * CIE_X_reduced[i];
		xyzColor.y += intensity * CIE_Y_reduced[i];
		xyzColor.z += intensity * CIE_Z_reduced[i];
	}
	
	const float Y_sum = 10.683556556701660f;
	const float3 normalizedXYZ = xyzColor / Y_sum;
	
	float3 rgb = ThinFilmCoating_ConvertXYZToRGB(normalizedXYZ);
	return ThinFilmCoating_Constrain(normalizedXYZ, rgb);
}
