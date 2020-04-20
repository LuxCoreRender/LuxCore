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

#include "luxrays/core/color/spds/irregular.h"
#include "slg/textures/fresnel/fresnelpreset.h"
#include "slg/textures/fresnel/fresnelconst.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Fresnel preset texture
//------------------------------------------------------------------------------

static const u_int AmorphousCarbonSamples = 15;
static const float AmorphousCarbonWavelengths[] = {
	247.96, 309.95, 326.263, 344.389, 364.647, 387.438, 413.267, 442.786,
	476.846, 516.583, 563.545, 619.9, 688.778, 774.875, 885.571
};
static const float AmorphousCarbonN[] = {
	1.73, 1.84, 1.9, 1.94, 2, 2.06, 2.11, 2.17, 2.24, 2.3, 2.38, 2.43,
	2.43, 2.33, 2.24
};
static const float AmorphousCarbonK[] = {
	0.712, 0.808, 0.91, 0.92, 0.92, 0.91, 0.9, 0.89, 0.88, 0.87, 0.82,
	0.75, 0.7, 0.71, 0.7
}; // jeanphi - 1 value was missing, added 0.7 at the end

static const u_int SopraSamples = 56;
static const float SopraWavelengths[] = {
	298.7570554, 302.4004341, 306.1337728, 309.960445, 313.8839949,
	317.9081487, 322.036826, 326.2741526, 330.6244747, 335.092373,
	339.6826795, 344.4004944, 349.2512056, 354.2405086, 359.374429,
	364.6593471, 370.1020239, 375.7096303, 381.4897785, 387.4505563,
	393.6005651, 399.9489613, 406.5055016, 413.2805933, 420.2853492,
	427.5316483, 435.0322035, 442.8006357, 450.8515564, 459.2006593,
	467.8648226, 476.8622231, 486.2124627, 495.936712, 506.0578694,
	516.6007417, 527.5922468, 539.0616435, 551.0407911, 563.5644455,
	576.6705953, 590.4008476, 604.8008683, 619.92089, 635.8162974,
	652.5483053, 670.1847459, 688.8009889, 708.4810171, 729.3186941,
	751.4192606, 774.9011125, 799.8979226, 826.5611867, 855.0632966,
	885.6012714
};

static const float SilverN[] = {
	1.519, 1.496, 1.4325, 1.323, 1.142062, 0.932, 0.719062, 0.526,
	0.388125, 0.294, 0.253313, 0.238, 0.221438, 0.209, 0.194813, 0.186,
	0.192063, 0.2, 0.198063, 0.192, 0.182, 0.173, 0.172625, 0.173,
	0.166688, 0.16, 0.1585, 0.157, 0.151063, 0.144, 0.137313, 0.132,
	0.13025, 0.13, 0.129938, 0.13, 0.130063, 0.129, 0.124375, 0.12,
	0.119313, 0.121, 0.1255, 0.131, 0.136125, 0.14, 0.140063, 0.14,
	0.144313, 0.148, 0.145875, 0.143, 0.142563, 0.145, 0.151938, 0.163
};
static const float SilverK[] = {
	1.08, 0.882, 0.761063, 0.647, 0.550875, 0.504, 0.554375, 0.663,
	0.818563, 0.986, 1.120687, 1.24, 1.34525, 1.44, 1.53375, 1.61,
	1.641875, 1.67, 1.735, 1.81, 1.87875, 1.95, 2.029375, 2.11, 2.18625,
	2.26, 2.329375, 2.4, 2.47875, 2.56, 2.64, 2.72, 2.798125, 2.88,
	2.97375, 3.07, 3.159375, 3.25, 3.348125, 3.45, 3.55375, 3.66, 3.76625,
	3.88, 4.010625, 4.15, 4.293125, 4.44, 4.58625, 4.74, 4.908125, 5.09,
	5.28875, 5.5, 5.720624, 5.95
};

static const float GoldN[] = {
	1.795, 1.812, 1.822625, 1.83, 1.837125, 1.84, 1.83425, 1.824, 1.812,
	1.798, 1.782, 1.766, 1.7525, 1.74, 1.727625, 1.716, 1.705875, 1.696,
	1.68475, 1.674, 1.666, 1.658, 1.64725, 1.636, 1.628, 1.616, 1.59625,
	1.562, 1.502125, 1.426, 1.345875, 1.242, 1.08675, 0.916, 0.7545, 0.608,
	0.49175, 0.402, 0.3455, 0.306, 0.267625, 0.236, 0.212375, 0.194,
	0.17775, 0.166, 0.161, 0.16, 0.160875, 0.164, 0.1695, 0.176, 0.181375,
	0.188, 0.198125, 0.21
};
static const float GoldK[] = {
	1.920375, 1.92, 1.918875, 1.916, 1.911375, 1.904, 1.891375, 1.878,
	1.86825, 1.86, 1.85175, 1.846, 1.84525, 1.848, 1.852375, 1.862, 1.883,
	1.906, 1.9225, 1.936, 1.94775, 1.956, 1.959375, 1.958, 1.951375, 1.94,
	1.9245, 1.904, 1.875875, 1.846, 1.814625, 1.796, 1.797375, 1.84,
	1.9565, 2.12, 2.32625, 2.54, 2.730625, 2.88, 2.940625, 2.97, 3.015,
	3.06, 3.07, 3.15, 3.445812, 3.8, 4.087687, 4.357, 4.610188, 4.86,
	5.125813, 5.39, 5.63125, 5.88
};

static const float CopperN[] = {
	1.400313, 1.38, 1.358438, 1.34, 1.329063, 1.325, 1.3325, 1.34,
	1.334375, 1.325, 1.317812, 1.31, 1.300313, 1.29, 1.281563, 1.27,
	1.249062, 1.225, 1.2, 1.18, 1.174375, 1.175, 1.1775, 1.18, 1.178125,
	1.175, 1.172812, 1.17, 1.165312, 1.16, 1.155312, 1.15, 1.142812, 1.135,
	1.131562, 1.12, 1.092437, 1.04, 0.950375, 0.826, 0.645875, 0.468,
	0.35125, 0.272, 0.230813, 0.214, 0.20925, 0.213, 0.21625, 0.223,
	0.2365, 0.25, 0.254188, 0.26, 0.28, 0.3
};
static const float CopperK[] = {
	1.662125, 1.687, 1.703313, 1.72, 1.744563, 1.77, 1.791625, 1.81,
	1.822125, 1.834, 1.85175, 1.872, 1.89425, 1.916, 1.931688, 1.95,
	1.972438, 2.015, 2.121562, 2.21, 2.177188, 2.13, 2.160063, 2.21,
	2.249938, 2.289, 2.326, 2.362, 2.397625, 2.433, 2.469187, 2.504,
	2.535875, 2.564, 2.589625, 2.605, 2.595562, 2.583, 2.5765, 2.599,
	2.678062, 2.809, 3.01075, 3.24, 3.458187, 3.67, 3.863125, 4.05,
	4.239563, 4.43, 4.619563, 4.817, 5.034125, 5.26, 5.485625, 5.717
};

static const float AluminiumN[] = {
	0.273375, 0.28, 0.286813, 0.294, 0.301875, 0.31, 0.317875, 0.326,
	0.33475, 0.344, 0.353813, 0.364, 0.374375, 0.385, 0.39575, 0.407,
	0.419125, 0.432, 0.445688, 0.46, 0.474688, 0.49, 0.506188, 0.523,
	0.540063, 0.558, 0.577313, 0.598, 0.620313, 0.644, 0.668625, 0.695,
	0.72375, 0.755, 0.789, 0.826, 0.867, 0.912, 0.963, 1.02, 1.08, 1.15,
	1.22, 1.3, 1.39, 1.49, 1.6, 1.74, 1.91, 2.14, 2.41, 2.63, 2.8, 2.74,
	2.58, 2.24
};
static const float AluminiumK[] = {
	3.59375, 3.64, 3.689375, 3.74, 3.789375, 3.84, 3.894375, 3.95, 4.005,
	4.06, 4.11375, 4.17, 4.23375, 4.3, 4.365, 4.43, 4.49375, 4.56, 4.63375,
	4.71, 4.784375, 4.86, 4.938125, 5.02, 5.10875, 5.2, 5.29, 5.38, 5.48,
	5.58, 5.69, 5.8, 5.915, 6.03, 6.15, 6.28, 6.42, 6.55, 6.7, 6.85, 7,
	7.15, 7.31, 7.48, 7.65, 7.82, 8.01, 8.21, 8.39, 8.57, 8.62, 8.6, 8.45,
	8.31, 8.21, 8.21
};

FresnelTexture *slg::AllocFresnelPresetTex(const Properties &props, const string &propName) {
	const string presetName = props.Get(Property(propName + ".preset")(
			props.Get(Property(propName + ".name")("aluminium")).Get<string>())).Get<string>();

	vector<float> wl;
	vector<float> n;
	vector<float> k;

	if (presetName == "amorphous carbon") {
		wl.assign(AmorphousCarbonWavelengths,
				AmorphousCarbonWavelengths + AmorphousCarbonSamples);
		n.assign(AmorphousCarbonN,
				AmorphousCarbonN + AmorphousCarbonSamples);
		k.assign(AmorphousCarbonK,
				AmorphousCarbonK + AmorphousCarbonSamples);
	} else if (presetName == "silver") {
		wl.assign(SopraWavelengths, SopraWavelengths + SopraSamples);
		n.assign(SilverN, SilverN + SopraSamples);
		k.assign(SilverK, SilverK + SopraSamples);
	} else if (presetName == "gold") {
		wl.assign(SopraWavelengths, SopraWavelengths + SopraSamples);
		n.assign(GoldN, GoldN + SopraSamples);
		k.assign(GoldK, GoldK + SopraSamples);
	} else if (presetName == "copper") {
		wl.assign(SopraWavelengths, SopraWavelengths + SopraSamples);
		n.assign(CopperN, CopperN + SopraSamples);
		k.assign(CopperK, CopperK + SopraSamples);
	} else if (presetName == "aluminium") {
		wl.assign(SopraWavelengths, SopraWavelengths + SopraSamples);
		n.assign(AluminiumN, AluminiumN + SopraSamples);
		k.assign(AluminiumK, AluminiumK + SopraSamples);
	} else
		throw runtime_error("Unknown Fresnel preset name: " + presetName);

	IrregularSPD N(&wl[0], &n[0], wl.size());
	IrregularSPD K(&wl[0], &k[0], wl.size());

	ColorSystem colorSpace(.63f, .34f, .31f, .595f, .155f, .07f,
		1.f / 3.f, 1.f / 3.f, 1.f);
	const RGBColor Nrgb = colorSpace.ToRGBConstrained(N.ToNormalizedXYZ());
	const RGBColor Krgb = colorSpace.ToRGBConstrained(K.ToNormalizedXYZ());

	return new FresnelConstTexture(Nrgb, Krgb);
}
