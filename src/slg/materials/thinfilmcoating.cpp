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

#include "slg/materials/thinfilmcoating.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//---------------------------------------------------------------------------------
// Thin film coating (utility functions, to be used in materials where appropriate)
//---------------------------------------------------------------------------------

// A reduced set of the CIE tables from luxrays/core/color/spds/data/xyzbasis.h, 
// in the range specified by MIN_WAVELENGTH and MAX_WAVELENGTH, with WAVELENGTH_STEP 
// intervals instead of 1 nm intervals.
// Pre-computed with the function at the bottom of this file.
static const float CIE_X_reduced[] = {
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

static const float CIE_Y_reduced[] = {
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

static const float CIE_Z_reduced[] = {
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

static const float Y_sum = 10.683556556701660f;

// 24 wavelengths seem to do the job. Any less and artifacs begin to appear at thickness around 2000 nm
// Using 34 now so I can use integers for the wavelengths, and 720 - 380 = 340.
const u_int NUM_WAVELENGTHS = 34;
const u_int MIN_WAVELENGTH = 380;
const u_int MAX_WAVELENGTH = 720;
const u_int WAVELENGTH_STEP = (MAX_WAVELENGTH - MIN_WAVELENGTH) / (NUM_WAVELENGTHS - 1);

const ColorSystem colorSpace(.63f, .34f, .31f, .595f, .155f, .07f, 1.f / 3.f, 1.f / 3.f, 1.f);

Spectrum slg::CalcFilmColor(const Vector &localFixedDir, const float filmThickness, const float filmIOR) {
	// Prevent wrong values if the ratio between IOR and thickness is too high
	if (filmThickness * (filmIOR - .4f) > 2000.f)
		return Spectrum(.5f);
	
	const float sinTheta = SinTheta(localFixedDir);
	const float s = sqrtf(Max(0.f, Sqr(filmIOR) - Sqr(sinTheta)));

	XYZColor xyzColor(0.f);

	for (u_int i = 0; i < NUM_WAVELENGTHS; ++i) {
		const u_int waveLength = WAVELENGTH_STEP * i + MIN_WAVELENGTH;
		
		const float pd = (4.f * M_PI * filmThickness / float(waveLength)) * s + M_PI;
		const float intensity = Sqr(cosf(pd));
		
		xyzColor.c[0] += intensity * CIE_X_reduced[i];
		xyzColor.c[1] += intensity * CIE_Y_reduced[i];
		xyzColor.c[2] += intensity * CIE_Z_reduced[i];
	}
	
	const XYZColor normalizedXYZ = xyzColor / Y_sum;
	
	const RGBColor rgb = colorSpace.ToRGBConstrained(normalizedXYZ);
	return static_cast<Spectrum>(rgb);
}

// Original LuxRender code
// void PhaseDifference(const SpectrumWavelengths &sw, const Vector &wo,
// 	float film, float filmindex, SWCSpectrum *const Pd)
// {
// 	const float swo = SinTheta(wo);
// 	const float s = sqrtf(max(0.f, filmindex * filmindex - /*1.f * 1.f * */ swo * swo));
// 	for(int i = 0; i < WAVELENGTH_SAMPLES; ++i) {
// 		const float pd = (4.f * M_PI * film / sw.w[i]) * s + M_PI;
// 		const float cpd = cosf(pd);
// 		Pd->c[i] *= cpd*cpd;
// 	}
// }

//---------------------------------------------------------------------------------
// To precompute the reduced XYZ tables and XYZ to RGB conversion matrix for OpenCL
//---------------------------------------------------------------------------------

// #include "luxrays/core/color/spds/data/xyzbasis.h"

// Spectrum slg::CalcFilmColor(const Vector &localFixedDir, const float filmThickness, const float filmIOR, const float exteriorIOR) {
// 	printf("static const float CIE_X_reduced[] = {\n");
// 	for (u_int i = 0; i < NUM_WAVELENGTHS; ++i) {
// 		const u_int waveLength = WAVELENGTH_STEP * i + MIN_WAVELENGTH;
		
// 		int CIE_index = waveLength - CIEstart - 1;
		
// 		printf("\t%.15ff,\n", CIE_X[CIE_index]);
// 	}
// 	printf("};\n\n");
	
// 	printf("static const float CIE_Y_reduced[] = {\n");
// 	for (u_int i = 0; i < NUM_WAVELENGTHS; ++i) {
// 		const u_int waveLength = WAVELENGTH_STEP * i + MIN_WAVELENGTH;
		
// 		int CIE_index = waveLength - CIEstart - 1;
		
// 		printf("\t%.15ff,\n", CIE_Y[CIE_index]);
// 	}
// 	printf("};\n\n");
	
// 	printf("static const float CIE_Z_reduced[] = {\n");
// 	for (u_int i = 0; i < NUM_WAVELENGTHS; ++i) {
// 		const u_int waveLength = WAVELENGTH_STEP * i + MIN_WAVELENGTH;
		
// 		int CIE_index = waveLength - CIEstart - 1;
		
// 		printf("\t%.15ff,\n", CIE_Z[CIE_index]);
// 	}
// 	printf("};\n\n");
	
// 	float yint = 0.f;
	
// 	for (u_int i = 0; i < NUM_WAVELENGTHS; ++i) {
// 		const u_int waveLength = WAVELENGTH_STEP * i + MIN_WAVELENGTH;
		
// 		int CIE_index = waveLength - CIEstart - 1;
// 		yint += CIE_Y[CIE_index];
// 	}
	
// 	printf("static const float Y_sum = %.15ff;\n\n", yint);
	
// 	printf("ColorSystem:\n");
// 	printf("XYZToRGB[] = {\n");
// 	for (int y = 0; y < 3; ++y) {
// 		printf("\t{\n");
// 		for (int x = 0; x < 3; ++x) {
// 			printf("\t\t%.15ff,\n", colorSpace.XYZToRGB[y][x]);
// 		}
		
// 		printf("\t},\n");
// 	}
// 	printf("};\n");
// 	printf("----\n");
	
// 	return Spectrum(0.5f, 0.f, 0.f);
// }
