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

#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#include "slg/film/imagepipelineplugins.h"
#include "slg/film/film.h"
#include "slg/film/cameraresponsefunctions.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Gamma correction plugin
//------------------------------------------------------------------------------

GammaCorrectionPlugin::GammaCorrectionPlugin(const float g, const u_int tableSize) {
	gamma = g;

	gammaTable.resize(tableSize, 0.f);
	float x = 0.f;
	const float dx = 1.f / tableSize;
	for (u_int i = 0; i < tableSize; ++i, x += dx)
		gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / g);
}

ImagePipelinePlugin *GammaCorrectionPlugin::Copy() const {
	return new GammaCorrectionPlugin(gamma, gammaTable.size());
}

float GammaCorrectionPlugin::Radiance2PixelFloat(const float x) const {
	// Very slow !
	//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

	const u_int tableSize = gammaTable.size();
	const int index = Clamp<int>(Floor2UInt(tableSize * Clamp(x, 0.f, 1.f)), 0, tableSize - 1);
	return gammaTable[index];
}

void GammaCorrectionPlugin::Apply(const Film &film, Spectrum *pixels, std::vector<bool> &pixelsMask) const {
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i]) {
			pixels[i].c[0] = Radiance2PixelFloat(pixels[i].c[0]);
			pixels[i].c[1] = Radiance2PixelFloat(pixels[i].c[1]);
			pixels[i].c[2] = Radiance2PixelFloat(pixels[i].c[2]);
		}
	}
}

//------------------------------------------------------------------------------
// Nop plugin
//------------------------------------------------------------------------------

ImagePipelinePlugin *NopPlugin::Copy() const {
	return new NopPlugin();
}

void NopPlugin::Apply(const Film &film, Spectrum *pixels, std::vector<bool> &pixelsMask) const {
}

//------------------------------------------------------------------------------
// OutputSwitcher plugin
//------------------------------------------------------------------------------

ImagePipelinePlugin *OutputSwitcherPlugin::Copy() const {
	return new OutputSwitcherPlugin(type, index);
}

void OutputSwitcherPlugin::Apply(const Film &film, Spectrum *pixels, std::vector<bool> &pixelsMask) const {
	// Copy the data from another Film output channel

	// Do nothing if the Film is missing this particular channel
	if (!film.HasChannel(type))
		return;

	const u_int pixelCount = film.GetWidth() * film.GetHeight();
	switch (type) {
		case Film::RADIANCE_PER_PIXEL_NORMALIZED: {
			if (index >= film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size())
				return;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float v[3];
					film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[index]->GetWeightedPixel(i, v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::RADIANCE_PER_SCREEN_NORMALIZED: {
			if (index >= film.channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size())
				return;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float v[3];
					film.channel_RADIANCE_PER_SCREEN_NORMALIZEDs[index]->GetWeightedPixel(i, v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::ALPHA: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float a;
					film.channel_ALPHA->GetWeightedPixel(i, &a);
					pixels[i] = Spectrum(a);
				}
			}
			break;
		}
		case Film::DEPTH: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float d;
					film.channel_DEPTH->GetWeightedPixel(i, &d);
					pixels[i] = Spectrum(d);
				}
			}
			break;
		}
		case Film::POSITION: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float *v = film.channel_POSITION->GetPixel(i);
					pixels[i].c[0] = fabs(v[0]);
					pixels[i].c[1] = fabs(v[1]);
					pixels[i].c[2] = fabs(v[2]);
				}
			}
			break;
		}
		case Film::GEOMETRY_NORMAL: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float *v = film.channel_GEOMETRY_NORMAL->GetPixel(i);
					pixels[i].c[0] = fabs(v[0]);
					pixels[i].c[1] = fabs(v[1]);
					pixels[i].c[2] = fabs(v[2]);
				}
			}
			break;
		}
		case Film::SHADING_NORMAL: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float *v = film.channel_SHADING_NORMAL->GetPixel(i);
					pixels[i].c[0] = fabs(v[0]);
					pixels[i].c[1] = fabs(v[1]);
					pixels[i].c[2] = fabs(v[2]);
				}
			}
			break;
		}
		case Film::MATERIAL_ID: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					u_int *v = film.channel_MATERIAL_ID->GetPixel(i);
					pixels[i].c[0] = (*v) & 0xff;
					pixels[i].c[1] = ((*v) & 0xff00) >> 8;
					pixels[i].c[2] = ((*v) & 0xff0000) >> 16;
				}
			}
			break;
		}
		case Film::DIRECT_DIFFUSE: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i])
					film.channel_DIRECT_DIFFUSE->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::DIRECT_GLOSSY: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i])
					film.channel_DIRECT_GLOSSY->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::EMISSION: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i])
					film.channel_EMISSION->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_DIFFUSE: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i])
					film.channel_INDIRECT_DIFFUSE->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_GLOSSY: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i])
					film.channel_INDIRECT_GLOSSY->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_SPECULAR: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i])
					film.channel_INDIRECT_SPECULAR->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::MATERIAL_ID_MASK: {
			if (index >= film.channel_MATERIAL_ID_MASKs.size())
				return;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float v;
					film.channel_MATERIAL_ID_MASKs[index]->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::DIRECT_SHADOW_MASK: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float v;
					film.channel_DIRECT_SHADOW_MASK->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::INDIRECT_SHADOW_MASK: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float v;
					film.channel_INDIRECT_SHADOW_MASK->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::UV: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float v[2];
					film.channel_UV->GetWeightedPixel(i, v);
					pixels[i].c[0] = v[0];
					pixels[i].c[1] = v[1];
					pixels[i].c[2] = 0.f;
				}
			}
			break;
		}
		case Film::RAYCOUNT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i]) {
					float v;
					film.channel_RAYCOUNT->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::BY_MATERIAL_ID: {
			if (index >= film.channel_BY_MATERIAL_IDs.size())
				return;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (pixelsMask[i])
					film.channel_BY_MATERIAL_IDs[index]->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		default:
			throw runtime_error("Unknown film output type in OutputSwitcherPlugin::Apply(): " + ToString(type));
	}
}

//------------------------------------------------------------------------------
// GaussianBlur filter plugin
//------------------------------------------------------------------------------

ImagePipelinePlugin *GaussianBlur3x3FilterPlugin::Copy() const {
	return new GaussianBlur3x3FilterPlugin(weight);
}

void GaussianBlur3x3FilterPlugin::ApplyBlurFilterXR1(
		const u_int filmWidth, const u_int filmHeight,
		const Spectrum *src,
		Spectrum *dst,
		const float aF,
		const float bF,
		const float cF) const {
	// Do left edge
	Spectrum a;
	Spectrum b = src[0];
	Spectrum c = src[1];

	const float leftTotF = bF + cF;
	const Spectrum bLeftK = bF / leftTotF;
	const Spectrum cLeftK = cF / leftTotF;
	dst[0] = bLeftK  * b + cLeftK * c;

    // Main loop
	const float totF = aF + bF + cF;
	const Spectrum aK = aF / totF;
	const Spectrum bK = bF / totF;
	const Spectrum cK = cF / totF;

	for (unsigned int x = 1; x < filmWidth - 1; ++x) {
		a = b;
		b = c;
		c = src[x + 1];

		dst[x] = aK * a + bK  * b + cK * c;
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const Spectrum aRightK = aF / rightTotF;
	const Spectrum bRightK = bF / rightTotF;
	a = b;
	b = c;
	dst[filmWidth - 1] = aRightK  * a + bRightK * b;
}

void GaussianBlur3x3FilterPlugin::ApplyBlurFilterYR1(
		const u_int filmWidth, const u_int filmHeight,
		const Spectrum *src,
		Spectrum *dst,
		const float aF,
		const float bF,
		const float cF) const {
	// Do left edge
	Spectrum a;
	Spectrum b = src[0];
	Spectrum c = src[filmWidth];

	const float leftTotF = bF + cF;
	const Spectrum bLeftK = bF / leftTotF;
	const Spectrum cLeftK = cF / leftTotF;
	dst[0] = bLeftK  * b + cLeftK * c;

    // Main loop
	const float totF = aF + bF + cF;
	const Spectrum aK = aF / totF;
	const Spectrum bK = bF / totF;
	const Spectrum cK = cF / totF;

    for (unsigned int y = 1; y < filmHeight - 1; ++y) {
		const unsigned index = y * filmWidth;

		a = b;
		b = c;
		c = src[index + filmWidth];

		dst[index] = aK * a + bK  * b + cK * c;
    }

    // Do right edge
	const float rightTotF = aF + bF;
	const Spectrum aRightK = aF / rightTotF;
	const Spectrum bRightK = bF / rightTotF;
	a = b;
	b = c;
	dst[(filmHeight - 1) * filmWidth] = aRightK  * a + bRightK * b;
}

void GaussianBlur3x3FilterPlugin::ApplyGaussianBlurFilterXR1(
		const u_int filmWidth, const u_int filmHeight,
		const Spectrum *src, Spectrum *dst) const {
	const float aF = weight;
	const float bF = 1.f;
	const float cF = weight;

	ApplyBlurFilterXR1(filmWidth, filmHeight, src, dst, aF, bF, cF);
}

void GaussianBlur3x3FilterPlugin::ApplyGaussianBlurFilterYR1(
		const u_int filmWidth, const u_int filmHeight,
		const Spectrum *src, Spectrum *dst) const {
	const float aF = weight;
	const float bF = 1.f;
	const float cF = weight;

	ApplyBlurFilterYR1(filmWidth, filmHeight, src, dst, aF, bF, cF);
}

void GaussianBlur3x3FilterPlugin::Apply(const Film &film, Spectrum *pixels, vector<bool> &pixelsMask) const {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	// Allocate the temporary buffer if required
	if ((!tmpBuffer) || (width * height != tmpBufferSize)) {
		delete tmpBuffer;

		tmpBufferSize = width * height;
		tmpBuffer = new Spectrum[tmpBufferSize];
	}

	for (u_int i = 0; i < 3; ++i) {
		for (u_int y = 0; y < height; ++y) {
			const u_int index = y * width;
			ApplyGaussianBlurFilterXR1(width, height, &pixels[index], &tmpBuffer[index]);
		}

		for (u_int x = 0; x < width; ++x)
			ApplyGaussianBlurFilterYR1(width, height, &tmpBuffer[x], &pixels[x]);
	}
}

//------------------------------------------------------------------------------
// CameraResponse filter plugin
//------------------------------------------------------------------------------

ImagePipelinePlugin *CameraResponsePlugin::Copy() const {
	CameraResponsePlugin *crp = new CameraResponsePlugin();
	crp->color = color;
	crp->RedI = RedI;
	crp->RedB = RedB;
	crp->GreenI = GreenI;
	crp->GreenB = GreenB;
	crp->BlueI = BlueI;
	crp->BlueB = BlueB;

	return crp;
}

// Estimates gamma in y = x^gamma using Gauss-Newton iterations
static float EstimateGamma(const vector<float> &x, const vector<float> &y, float *rmse) {
	const size_t n = x.size();

	double gamma = 1.0; // Initial guess
	int iter = 0;

	double last_ssr = 1e30;

	while (iter < 100) {
		double ssr = 0;
		double JTr = 0.0;
		double JTJ = 0.0;

		size_t i;

		for (i = 0; i < n; i++) {
			// log() doesn't behave nice near zero
			if (x[i] < 1e-12)
				continue;

			// pow() has issues with small but negative values of gamma
			// this is more stable
			const double lx = log(x[i]);
			const double xg = exp(gamma * lx);
			
			// residual
			double r = y[i] - xg;
			ssr += r*r;
			
			// jacobian
			double J = lx * xg;

			JTr += J * r;
			JTJ += J * J;
		}

		if (rmse)
			*rmse = static_cast<float>(sqrt(ssr/i));

		if (fabs(ssr - last_ssr) < 1e-6)
			break;

		const double delta = JTr / JTJ;

		if (fabs(delta) < 1e-9)
			break;

		gamma = gamma + delta;

		last_ssr = ssr;

		iter++;
	}

	return static_cast<float>(gamma);
}

static void AdjustGamma(const vector<float> &from, vector<float> &to, float gamma = 2.2f) {
	for (size_t i = 0; i < from.size(); i++)
		to[i] = powf(to[i], gamma);
}

CameraResponsePlugin::CameraResponsePlugin(const string &name) {
	if (!LoadPreset(name))
		LoadFile(name);

	// Compensate for built-in gamma. Use Y for gamma estimation.
	vector<float> YI, YB;

	// use all unique input values	
	YI.insert(YI.end(), RedI.begin(), RedI.end());
	YI.insert(YI.end(), GreenI.begin(), GreenI.end());
	YI.insert(YI.end(), BlueI.begin(), BlueI.end());

	sort(YI.begin(), YI.end());
	const size_t n = unique(YI.begin(), YI.end()) - YI.begin();

	YI.resize(n);
	YB.resize(n);

	for (size_t i = 0; i < n; i++) {
		RGBColor c(YI[i]);

		// Map handles color / monochrome and interpolation
		Map(c);		

		YB[i] = c.Y();
	}

	float rmse;
	const float sourceGamma = EstimateGamma(YI, YB, &rmse);

	// Compensate for built-in gamma
	AdjustGamma(RedI, RedB, 1.f / sourceGamma);
	AdjustGamma(GreenI, GreenB, 1.f / sourceGamma);
	AdjustGamma(BlueI, BlueB, 1.f / sourceGamma);
}

void CameraResponsePlugin::Apply(const Film &film, Spectrum *pixels, vector<bool> &pixelsMask) const {
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i])
			Map(pixels[i]);
	}
}

void CameraResponsePlugin::Map(RGBColor &rgb) const {
	if (color) {
		rgb.c[0] = ApplyCrf(rgb.c[0], RedI, RedB);
		rgb.c[1] = ApplyCrf(rgb.c[1], GreenI, GreenB);
		rgb.c[2] = ApplyCrf(rgb.c[2], BlueI, BlueB);
	} else {
		const float y = rgb.Y();
		rgb.c[0] = rgb.c[1] = rgb.c[2] = ApplyCrf(y, RedI, RedB);
	}
}

float CameraResponsePlugin::ApplyCrf(float point, const vector<float> &from, const vector<float> &to) const {
	if (point <= from.front())
		return to.front();
	if (point >= from.back())
		return to.back();

	int index = std::upper_bound(from.begin(), from.end(), point) -
		from.begin();
	float x1 = from[index - 1];
	float x2 = from[index];
	float y1 = to[index - 1];
	float y2 = to[index];
	return Lerp((point - x1) / (x2 - x1), y1, y2);
}

void CameraResponsePlugin::LoadFile(const string &filmName) {
	ifstream file(filmName.c_str());
	if (!file)
		throw runtime_error("Unable to open Camera Response Function file: " + filmName);

	string crfdata;
	{
		// Read file into memory
		stringstream ss("");
		ss << file.rdbuf();
		crfdata = ss.str();
	}

	boost::regex func_expr("(?-s)(?<funcname>\\S+)\\s+graph.+\\s+I\\s*=\\s*(?<I>.+$)\\s+B\\s*=\\s*(?<B>.+$)");
	boost::regex channel_expr("^(?<name>.*)(?<channel>Red|Green|Blue)$");
	boost::regex float_expr("-?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?");

	boost::sregex_iterator rit(crfdata.begin(), crfdata.end(), func_expr);
	boost::sregex_iterator rend;
	
	bool red, green, blue;
	red = green = blue = false;

	string crfname("");

	for (; rit != rend; ++rit) {
		const boost::smatch &m = *rit;
		boost::smatch cm;

		// Smatch references input string, keep copy
		const string funcname = m["funcname"].str();
		string name = funcname;
		string channel = "Red";

		if (boost::regex_match(funcname, cm, channel_expr)) {
			name = cm["name"].str();
			channel = cm["channel"].str();
		}
		const string Istr = m["I"].str();
		const string Bstr = m["B"].str();

		if (crfname == "")
			crfname = name;
		else if (crfname != name)
			throw runtime_error("Expected Camera Response Function name '" + crfname + "' but found '" + name + "'");

		vector<float> *I = NULL;
		vector<float> *B = NULL;
		bool *channel_flag = NULL;

		if (channel == "Red") {
			I = &RedI;
			B = &RedB;
			channel_flag = &red;
		} else if (channel == "Green") {
			I = &GreenI;
			B = &GreenB;
			channel_flag = &green;
		} else if (channel == "Blue") {
			I = &BlueI;
			B = &BlueB;
			channel_flag = &blue;
		} else
			throw runtime_error("Error parsing Camera Response file");

		if (*channel_flag) {
			SLG_LOG("WARNING: " << channel << " channel already specified in '" << filmName << "', ignoring");
			continue;
		}

		// Parse functions
		for (boost::sregex_iterator rit(Istr.begin(), Istr.end(), float_expr); rit != rend; ++rit) {
			I->push_back(boost::lexical_cast<float>(rit->str(0)));
		}

		for (boost::sregex_iterator rit(Bstr.begin(), Bstr.end(), float_expr); rit != rend; ++rit) {
			B->push_back(boost::lexical_cast<float>(rit->str(0)));
		}

		*channel_flag = I->size() == B->size();

		if (!(*channel_flag) || I->empty()) {
			SLG_LOG("WARNING: Inconsistent " << channel << " data for '" << filmName << "', ignoring");
			I->clear();
			B->clear();
			*channel_flag = false;
		}

		color = red && green && blue;
		if (color)
			break;
	}

	// Need at least red for either monochrome or color
	if (!red)
		throw runtime_error("No valid Camera Response Functions in: " + filmName);
}

bool CameraResponsePlugin::LoadPreset(const std::string &filmName) {
/*  
	Valid preset names (case sensitive):

	Advantix_100CD
	Advantix_200CD
	Advantix_400CD
	Agfachrome_ctpecisa_200CD
	Agfachrome_ctprecisa_100CD
	Agfachrome_rsx2_050CD
	Agfachrome_rsx2_100CD
	Agfachrome_rsx2_200CD
	Agfacolor_futura_100CD
	Agfacolor_futura_200CD
	Agfacolor_futura_400CD
	Agfacolor_futuraII_100CD
	Agfacolor_futuraII_200CD
	Agfacolor_futuraII_400CD
	Agfacolor_hdc_100_plusCD
	Agfacolor_hdc_200_plusCD
	Agfacolor_hdc_400_plusCD
	Agfacolor_optimaII_100CD
	Agfacolor_optimaII_200CD
	Agfacolor_ultra_050_CD
	Agfacolor_vista_100CD
	Agfacolor_vista_200CD
	Agfacolor_vista_400CD
	Agfacolor_vista_800CD
	Agfapan_apx_025CD
	Agfapan_apx_100CD
	Agfapan_apx_400CD
	Ektachrome_100_plusCD
	Ektachrome_100CD
	Ektachrome_320TCD
	Ektachrome_400XCD
	Ektachrome_64CD
	Ektachrome_64TCD
	Ektachrome_E100SCD
	F125CD
	F250CD
	F400CD
	FCICD
	Gold_100CD
	Gold_200CD
	Kodachrome_200CD
	Kodachrome_25CD
	Kodachrome_64CD
	Max_Zoom_800CD
	Portra_100TCD
	Portra_160NCCD
	Portra_160VCCD
	Portra_400NCCD
	Portra_400VCCD
	Portra_800CD
*/

	// Generated by make_embedded_crfs.py
	if (filmName == "Advantix_100CD") {
		const size_t nRed = sizeof(Advantix_100CDRed_I) / sizeof(Advantix_100CDRed_I[0]);
		RedI.assign(Advantix_100CDRed_I, Advantix_100CDRed_I + nRed);
		RedB.assign(Advantix_100CDRed_B, Advantix_100CDRed_B + nRed);
		const size_t nGreen = sizeof(Advantix_100CDGreen_I) / sizeof(Advantix_100CDGreen_I[0]);
		GreenI.assign(Advantix_100CDGreen_I, Advantix_100CDGreen_I + nGreen);
		GreenB.assign(Advantix_100CDGreen_B, Advantix_100CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Advantix_100CDBlue_I) / sizeof(Advantix_100CDBlue_I[0]);
		BlueI.assign(Advantix_100CDBlue_I, Advantix_100CDBlue_I + nBlue);
		BlueB.assign(Advantix_100CDBlue_B, Advantix_100CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Advantix_200CD") {
		const size_t nRed = sizeof(Advantix_200CDRed_I) / sizeof(Advantix_200CDRed_I[0]);
		RedI.assign(Advantix_200CDRed_I, Advantix_200CDRed_I + nRed);
		RedB.assign(Advantix_200CDRed_B, Advantix_200CDRed_B + nRed);
		const size_t nGreen = sizeof(Advantix_200CDGreen_I) / sizeof(Advantix_200CDGreen_I[0]);
		GreenI.assign(Advantix_200CDGreen_I, Advantix_200CDGreen_I + nGreen);
		GreenB.assign(Advantix_200CDGreen_B, Advantix_200CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Advantix_200CDBlue_I) / sizeof(Advantix_200CDBlue_I[0]);
		BlueI.assign(Advantix_200CDBlue_I, Advantix_200CDBlue_I + nBlue);
		BlueB.assign(Advantix_200CDBlue_B, Advantix_200CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Advantix_400CD") {
		const size_t nRed = sizeof(Advantix_400CDRed_I) / sizeof(Advantix_400CDRed_I[0]);
		RedI.assign(Advantix_400CDRed_I, Advantix_400CDRed_I + nRed);
		RedB.assign(Advantix_400CDRed_B, Advantix_400CDRed_B + nRed);
		const size_t nGreen = sizeof(Advantix_400CDGreen_I) / sizeof(Advantix_400CDGreen_I[0]);
		GreenI.assign(Advantix_400CDGreen_I, Advantix_400CDGreen_I + nGreen);
		GreenB.assign(Advantix_400CDGreen_B, Advantix_400CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Advantix_400CDBlue_I) / sizeof(Advantix_400CDBlue_I[0]);
		BlueI.assign(Advantix_400CDBlue_I, Advantix_400CDBlue_I + nBlue);
		BlueB.assign(Advantix_400CDBlue_B, Advantix_400CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfachrome_ctpecisa_200CD") {
		const size_t nRed = sizeof(Agfachrome_ctpecisa_200CDRed_I) / sizeof(Agfachrome_ctpecisa_200CDRed_I[0]);
		RedI.assign(Agfachrome_ctpecisa_200CDRed_I, Agfachrome_ctpecisa_200CDRed_I + nRed);
		RedB.assign(Agfachrome_ctpecisa_200CDRed_B, Agfachrome_ctpecisa_200CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfachrome_ctpecisa_200CDGreen_I) / sizeof(Agfachrome_ctpecisa_200CDGreen_I[0]);
		GreenI.assign(Agfachrome_ctpecisa_200CDGreen_I, Agfachrome_ctpecisa_200CDGreen_I + nGreen);
		GreenB.assign(Agfachrome_ctpecisa_200CDGreen_B, Agfachrome_ctpecisa_200CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfachrome_ctpecisa_200CDBlue_I) / sizeof(Agfachrome_ctpecisa_200CDBlue_I[0]);
		BlueI.assign(Agfachrome_ctpecisa_200CDBlue_I, Agfachrome_ctpecisa_200CDBlue_I + nBlue);
		BlueB.assign(Agfachrome_ctpecisa_200CDBlue_B, Agfachrome_ctpecisa_200CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfachrome_ctprecisa_100CD") {
		const size_t nRed = sizeof(Agfachrome_ctprecisa_100CDRed_I) / sizeof(Agfachrome_ctprecisa_100CDRed_I[0]);
		RedI.assign(Agfachrome_ctprecisa_100CDRed_I, Agfachrome_ctprecisa_100CDRed_I + nRed);
		RedB.assign(Agfachrome_ctprecisa_100CDRed_B, Agfachrome_ctprecisa_100CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfachrome_ctprecisa_100CDGreen_I) / sizeof(Agfachrome_ctprecisa_100CDGreen_I[0]);
		GreenI.assign(Agfachrome_ctprecisa_100CDGreen_I, Agfachrome_ctprecisa_100CDGreen_I + nGreen);
		GreenB.assign(Agfachrome_ctprecisa_100CDGreen_B, Agfachrome_ctprecisa_100CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfachrome_ctprecisa_100CDBlue_I) / sizeof(Agfachrome_ctprecisa_100CDBlue_I[0]);
		BlueI.assign(Agfachrome_ctprecisa_100CDBlue_I, Agfachrome_ctprecisa_100CDBlue_I + nBlue);
		BlueB.assign(Agfachrome_ctprecisa_100CDBlue_B, Agfachrome_ctprecisa_100CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfachrome_rsx2_050CD") {
		const size_t nRed = sizeof(Agfachrome_rsx2_050CDRed_I) / sizeof(Agfachrome_rsx2_050CDRed_I[0]);
		RedI.assign(Agfachrome_rsx2_050CDRed_I, Agfachrome_rsx2_050CDRed_I + nRed);
		RedB.assign(Agfachrome_rsx2_050CDRed_B, Agfachrome_rsx2_050CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfachrome_rsx2_050CDGreen_I) / sizeof(Agfachrome_rsx2_050CDGreen_I[0]);
		GreenI.assign(Agfachrome_rsx2_050CDGreen_I, Agfachrome_rsx2_050CDGreen_I + nGreen);
		GreenB.assign(Agfachrome_rsx2_050CDGreen_B, Agfachrome_rsx2_050CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfachrome_rsx2_050CDBlue_I) / sizeof(Agfachrome_rsx2_050CDBlue_I[0]);
		BlueI.assign(Agfachrome_rsx2_050CDBlue_I, Agfachrome_rsx2_050CDBlue_I + nBlue);
		BlueB.assign(Agfachrome_rsx2_050CDBlue_B, Agfachrome_rsx2_050CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfachrome_rsx2_100CD") {
		const size_t nRed = sizeof(Agfachrome_rsx2_100CDRed_I) / sizeof(Agfachrome_rsx2_100CDRed_I[0]);
		RedI.assign(Agfachrome_rsx2_100CDRed_I, Agfachrome_rsx2_100CDRed_I + nRed);
		RedB.assign(Agfachrome_rsx2_100CDRed_B, Agfachrome_rsx2_100CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfachrome_rsx2_100CDGreen_I) / sizeof(Agfachrome_rsx2_100CDGreen_I[0]);
		GreenI.assign(Agfachrome_rsx2_100CDGreen_I, Agfachrome_rsx2_100CDGreen_I + nGreen);
		GreenB.assign(Agfachrome_rsx2_100CDGreen_B, Agfachrome_rsx2_100CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfachrome_rsx2_100CDBlue_I) / sizeof(Agfachrome_rsx2_100CDBlue_I[0]);
		BlueI.assign(Agfachrome_rsx2_100CDBlue_I, Agfachrome_rsx2_100CDBlue_I + nBlue);
		BlueB.assign(Agfachrome_rsx2_100CDBlue_B, Agfachrome_rsx2_100CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfachrome_rsx2_200CD") {
		const size_t nRed = sizeof(Agfachrome_rsx2_200CDRed_I) / sizeof(Agfachrome_rsx2_200CDRed_I[0]);
		RedI.assign(Agfachrome_rsx2_200CDRed_I, Agfachrome_rsx2_200CDRed_I + nRed);
		RedB.assign(Agfachrome_rsx2_200CDRed_B, Agfachrome_rsx2_200CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfachrome_rsx2_200CDGreen_I) / sizeof(Agfachrome_rsx2_200CDGreen_I[0]);
		GreenI.assign(Agfachrome_rsx2_200CDGreen_I, Agfachrome_rsx2_200CDGreen_I + nGreen);
		GreenB.assign(Agfachrome_rsx2_200CDGreen_B, Agfachrome_rsx2_200CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfachrome_rsx2_200CDBlue_I) / sizeof(Agfachrome_rsx2_200CDBlue_I[0]);
		BlueI.assign(Agfachrome_rsx2_200CDBlue_I, Agfachrome_rsx2_200CDBlue_I + nBlue);
		BlueB.assign(Agfachrome_rsx2_200CDBlue_B, Agfachrome_rsx2_200CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_futura_100CD") {
		const size_t nRed = sizeof(Agfacolor_futura_100CDRed_I) / sizeof(Agfacolor_futura_100CDRed_I[0]);
		RedI.assign(Agfacolor_futura_100CDRed_I, Agfacolor_futura_100CDRed_I + nRed);
		RedB.assign(Agfacolor_futura_100CDRed_B, Agfacolor_futura_100CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_futura_100CDGreen_I) / sizeof(Agfacolor_futura_100CDGreen_I[0]);
		GreenI.assign(Agfacolor_futura_100CDGreen_I, Agfacolor_futura_100CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_futura_100CDGreen_B, Agfacolor_futura_100CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_futura_100CDBlue_I) / sizeof(Agfacolor_futura_100CDBlue_I[0]);
		BlueI.assign(Agfacolor_futura_100CDBlue_I, Agfacolor_futura_100CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_futura_100CDBlue_B, Agfacolor_futura_100CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_futura_200CD") {
		const size_t nRed = sizeof(Agfacolor_futura_200CDRed_I) / sizeof(Agfacolor_futura_200CDRed_I[0]);
		RedI.assign(Agfacolor_futura_200CDRed_I, Agfacolor_futura_200CDRed_I + nRed);
		RedB.assign(Agfacolor_futura_200CDRed_B, Agfacolor_futura_200CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_futura_200CDGreen_I) / sizeof(Agfacolor_futura_200CDGreen_I[0]);
		GreenI.assign(Agfacolor_futura_200CDGreen_I, Agfacolor_futura_200CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_futura_200CDGreen_B, Agfacolor_futura_200CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_futura_200CDBlue_I) / sizeof(Agfacolor_futura_200CDBlue_I[0]);
		BlueI.assign(Agfacolor_futura_200CDBlue_I, Agfacolor_futura_200CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_futura_200CDBlue_B, Agfacolor_futura_200CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_futura_400CD") {
		const size_t nRed = sizeof(Agfacolor_futura_400CDRed_I) / sizeof(Agfacolor_futura_400CDRed_I[0]);
		RedI.assign(Agfacolor_futura_400CDRed_I, Agfacolor_futura_400CDRed_I + nRed);
		RedB.assign(Agfacolor_futura_400CDRed_B, Agfacolor_futura_400CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_futura_400CDGreen_I) / sizeof(Agfacolor_futura_400CDGreen_I[0]);
		GreenI.assign(Agfacolor_futura_400CDGreen_I, Agfacolor_futura_400CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_futura_400CDGreen_B, Agfacolor_futura_400CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_futura_400CDBlue_I) / sizeof(Agfacolor_futura_400CDBlue_I[0]);
		BlueI.assign(Agfacolor_futura_400CDBlue_I, Agfacolor_futura_400CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_futura_400CDBlue_B, Agfacolor_futura_400CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_futuraII_100CD") {
		const size_t nRed = sizeof(Agfacolor_futuraII_100CDRed_I) / sizeof(Agfacolor_futuraII_100CDRed_I[0]);
		RedI.assign(Agfacolor_futuraII_100CDRed_I, Agfacolor_futuraII_100CDRed_I + nRed);
		RedB.assign(Agfacolor_futuraII_100CDRed_B, Agfacolor_futuraII_100CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_futuraII_100CDGreen_I) / sizeof(Agfacolor_futuraII_100CDGreen_I[0]);
		GreenI.assign(Agfacolor_futuraII_100CDGreen_I, Agfacolor_futuraII_100CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_futuraII_100CDGreen_B, Agfacolor_futuraII_100CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_futuraII_100CDBlue_I) / sizeof(Agfacolor_futuraII_100CDBlue_I[0]);
		BlueI.assign(Agfacolor_futuraII_100CDBlue_I, Agfacolor_futuraII_100CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_futuraII_100CDBlue_B, Agfacolor_futuraII_100CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_futuraII_200CD") {
		const size_t nRed = sizeof(Agfacolor_futuraII_200CDRed_I) / sizeof(Agfacolor_futuraII_200CDRed_I[0]);
		RedI.assign(Agfacolor_futuraII_200CDRed_I, Agfacolor_futuraII_200CDRed_I + nRed);
		RedB.assign(Agfacolor_futuraII_200CDRed_B, Agfacolor_futuraII_200CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_futuraII_200CDGreen_I) / sizeof(Agfacolor_futuraII_200CDGreen_I[0]);
		GreenI.assign(Agfacolor_futuraII_200CDGreen_I, Agfacolor_futuraII_200CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_futuraII_200CDGreen_B, Agfacolor_futuraII_200CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_futuraII_200CDBlue_I) / sizeof(Agfacolor_futuraII_200CDBlue_I[0]);
		BlueI.assign(Agfacolor_futuraII_200CDBlue_I, Agfacolor_futuraII_200CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_futuraII_200CDBlue_B, Agfacolor_futuraII_200CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_futuraII_400CD") {
		const size_t nRed = sizeof(Agfacolor_futuraII_400CDRed_I) / sizeof(Agfacolor_futuraII_400CDRed_I[0]);
		RedI.assign(Agfacolor_futuraII_400CDRed_I, Agfacolor_futuraII_400CDRed_I + nRed);
		RedB.assign(Agfacolor_futuraII_400CDRed_B, Agfacolor_futuraII_400CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_futuraII_400CDGreen_I) / sizeof(Agfacolor_futuraII_400CDGreen_I[0]);
		GreenI.assign(Agfacolor_futuraII_400CDGreen_I, Agfacolor_futuraII_400CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_futuraII_400CDGreen_B, Agfacolor_futuraII_400CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_futuraII_400CDBlue_I) / sizeof(Agfacolor_futuraII_400CDBlue_I[0]);
		BlueI.assign(Agfacolor_futuraII_400CDBlue_I, Agfacolor_futuraII_400CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_futuraII_400CDBlue_B, Agfacolor_futuraII_400CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_hdc_100_plusCD") {
		const size_t nRed = sizeof(Agfacolor_hdc_100_plusCDRed_I) / sizeof(Agfacolor_hdc_100_plusCDRed_I[0]);
		RedI.assign(Agfacolor_hdc_100_plusCDRed_I, Agfacolor_hdc_100_plusCDRed_I + nRed);
		RedB.assign(Agfacolor_hdc_100_plusCDRed_B, Agfacolor_hdc_100_plusCDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_hdc_100_plusCDGreen_I) / sizeof(Agfacolor_hdc_100_plusCDGreen_I[0]);
		GreenI.assign(Agfacolor_hdc_100_plusCDGreen_I, Agfacolor_hdc_100_plusCDGreen_I + nGreen);
		GreenB.assign(Agfacolor_hdc_100_plusCDGreen_B, Agfacolor_hdc_100_plusCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_hdc_100_plusCDBlue_I) / sizeof(Agfacolor_hdc_100_plusCDBlue_I[0]);
		BlueI.assign(Agfacolor_hdc_100_plusCDBlue_I, Agfacolor_hdc_100_plusCDBlue_I + nBlue);
		BlueB.assign(Agfacolor_hdc_100_plusCDBlue_B, Agfacolor_hdc_100_plusCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_hdc_200_plusCD") {
		const size_t nRed = sizeof(Agfacolor_hdc_200_plusCDRed_I) / sizeof(Agfacolor_hdc_200_plusCDRed_I[0]);
		RedI.assign(Agfacolor_hdc_200_plusCDRed_I, Agfacolor_hdc_200_plusCDRed_I + nRed);
		RedB.assign(Agfacolor_hdc_200_plusCDRed_B, Agfacolor_hdc_200_plusCDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_hdc_200_plusCDGreen_I) / sizeof(Agfacolor_hdc_200_plusCDGreen_I[0]);
		GreenI.assign(Agfacolor_hdc_200_plusCDGreen_I, Agfacolor_hdc_200_plusCDGreen_I + nGreen);
		GreenB.assign(Agfacolor_hdc_200_plusCDGreen_B, Agfacolor_hdc_200_plusCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_hdc_200_plusCDBlue_I) / sizeof(Agfacolor_hdc_200_plusCDBlue_I[0]);
		BlueI.assign(Agfacolor_hdc_200_plusCDBlue_I, Agfacolor_hdc_200_plusCDBlue_I + nBlue);
		BlueB.assign(Agfacolor_hdc_200_plusCDBlue_B, Agfacolor_hdc_200_plusCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_hdc_400_plusCD") {
		const size_t nRed = sizeof(Agfacolor_hdc_400_plusCDRed_I) / sizeof(Agfacolor_hdc_400_plusCDRed_I[0]);
		RedI.assign(Agfacolor_hdc_400_plusCDRed_I, Agfacolor_hdc_400_plusCDRed_I + nRed);
		RedB.assign(Agfacolor_hdc_400_plusCDRed_B, Agfacolor_hdc_400_plusCDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_hdc_400_plusCDGreen_I) / sizeof(Agfacolor_hdc_400_plusCDGreen_I[0]);
		GreenI.assign(Agfacolor_hdc_400_plusCDGreen_I, Agfacolor_hdc_400_plusCDGreen_I + nGreen);
		GreenB.assign(Agfacolor_hdc_400_plusCDGreen_B, Agfacolor_hdc_400_plusCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_hdc_400_plusCDBlue_I) / sizeof(Agfacolor_hdc_400_plusCDBlue_I[0]);
		BlueI.assign(Agfacolor_hdc_400_plusCDBlue_I, Agfacolor_hdc_400_plusCDBlue_I + nBlue);
		BlueB.assign(Agfacolor_hdc_400_plusCDBlue_B, Agfacolor_hdc_400_plusCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_optimaII_100CD") {
		const size_t nRed = sizeof(Agfacolor_optimaII_100CDRed_I) / sizeof(Agfacolor_optimaII_100CDRed_I[0]);
		RedI.assign(Agfacolor_optimaII_100CDRed_I, Agfacolor_optimaII_100CDRed_I + nRed);
		RedB.assign(Agfacolor_optimaII_100CDRed_B, Agfacolor_optimaII_100CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_optimaII_100CDGreen_I) / sizeof(Agfacolor_optimaII_100CDGreen_I[0]);
		GreenI.assign(Agfacolor_optimaII_100CDGreen_I, Agfacolor_optimaII_100CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_optimaII_100CDGreen_B, Agfacolor_optimaII_100CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_optimaII_100CDBlue_I) / sizeof(Agfacolor_optimaII_100CDBlue_I[0]);
		BlueI.assign(Agfacolor_optimaII_100CDBlue_I, Agfacolor_optimaII_100CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_optimaII_100CDBlue_B, Agfacolor_optimaII_100CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_optimaII_200CD") {
		const size_t nRed = sizeof(Agfacolor_optimaII_200CDRed_I) / sizeof(Agfacolor_optimaII_200CDRed_I[0]);
		RedI.assign(Agfacolor_optimaII_200CDRed_I, Agfacolor_optimaII_200CDRed_I + nRed);
		RedB.assign(Agfacolor_optimaII_200CDRed_B, Agfacolor_optimaII_200CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_optimaII_200CDGreen_I) / sizeof(Agfacolor_optimaII_200CDGreen_I[0]);
		GreenI.assign(Agfacolor_optimaII_200CDGreen_I, Agfacolor_optimaII_200CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_optimaII_200CDGreen_B, Agfacolor_optimaII_200CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_optimaII_200CDBlue_I) / sizeof(Agfacolor_optimaII_200CDBlue_I[0]);
		BlueI.assign(Agfacolor_optimaII_200CDBlue_I, Agfacolor_optimaII_200CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_optimaII_200CDBlue_B, Agfacolor_optimaII_200CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_ultra_050_CD") {
		const size_t nRed = sizeof(Agfacolor_ultra_050_CDRed_I) / sizeof(Agfacolor_ultra_050_CDRed_I[0]);
		RedI.assign(Agfacolor_ultra_050_CDRed_I, Agfacolor_ultra_050_CDRed_I + nRed);
		RedB.assign(Agfacolor_ultra_050_CDRed_B, Agfacolor_ultra_050_CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_ultra_050_CDGreen_I) / sizeof(Agfacolor_ultra_050_CDGreen_I[0]);
		GreenI.assign(Agfacolor_ultra_050_CDGreen_I, Agfacolor_ultra_050_CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_ultra_050_CDGreen_B, Agfacolor_ultra_050_CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_ultra_050_CDBlue_I) / sizeof(Agfacolor_ultra_050_CDBlue_I[0]);
		BlueI.assign(Agfacolor_ultra_050_CDBlue_I, Agfacolor_ultra_050_CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_ultra_050_CDBlue_B, Agfacolor_ultra_050_CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_vista_100CD") {
		const size_t nRed = sizeof(Agfacolor_vista_100CDRed_I) / sizeof(Agfacolor_vista_100CDRed_I[0]);
		RedI.assign(Agfacolor_vista_100CDRed_I, Agfacolor_vista_100CDRed_I + nRed);
		RedB.assign(Agfacolor_vista_100CDRed_B, Agfacolor_vista_100CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_vista_100CDGreen_I) / sizeof(Agfacolor_vista_100CDGreen_I[0]);
		GreenI.assign(Agfacolor_vista_100CDGreen_I, Agfacolor_vista_100CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_vista_100CDGreen_B, Agfacolor_vista_100CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_vista_100CDBlue_I) / sizeof(Agfacolor_vista_100CDBlue_I[0]);
		BlueI.assign(Agfacolor_vista_100CDBlue_I, Agfacolor_vista_100CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_vista_100CDBlue_B, Agfacolor_vista_100CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_vista_200CD") {
		const size_t nRed = sizeof(Agfacolor_vista_200CDRed_I) / sizeof(Agfacolor_vista_200CDRed_I[0]);
		RedI.assign(Agfacolor_vista_200CDRed_I, Agfacolor_vista_200CDRed_I + nRed);
		RedB.assign(Agfacolor_vista_200CDRed_B, Agfacolor_vista_200CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_vista_200CDGreen_I) / sizeof(Agfacolor_vista_200CDGreen_I[0]);
		GreenI.assign(Agfacolor_vista_200CDGreen_I, Agfacolor_vista_200CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_vista_200CDGreen_B, Agfacolor_vista_200CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_vista_200CDBlue_I) / sizeof(Agfacolor_vista_200CDBlue_I[0]);
		BlueI.assign(Agfacolor_vista_200CDBlue_I, Agfacolor_vista_200CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_vista_200CDBlue_B, Agfacolor_vista_200CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_vista_400CD") {
		const size_t nRed = sizeof(Agfacolor_vista_400CDRed_I) / sizeof(Agfacolor_vista_400CDRed_I[0]);
		RedI.assign(Agfacolor_vista_400CDRed_I, Agfacolor_vista_400CDRed_I + nRed);
		RedB.assign(Agfacolor_vista_400CDRed_B, Agfacolor_vista_400CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_vista_400CDGreen_I) / sizeof(Agfacolor_vista_400CDGreen_I[0]);
		GreenI.assign(Agfacolor_vista_400CDGreen_I, Agfacolor_vista_400CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_vista_400CDGreen_B, Agfacolor_vista_400CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_vista_400CDBlue_I) / sizeof(Agfacolor_vista_400CDBlue_I[0]);
		BlueI.assign(Agfacolor_vista_400CDBlue_I, Agfacolor_vista_400CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_vista_400CDBlue_B, Agfacolor_vista_400CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfacolor_vista_800CD") {
		const size_t nRed = sizeof(Agfacolor_vista_800CDRed_I) / sizeof(Agfacolor_vista_800CDRed_I[0]);
		RedI.assign(Agfacolor_vista_800CDRed_I, Agfacolor_vista_800CDRed_I + nRed);
		RedB.assign(Agfacolor_vista_800CDRed_B, Agfacolor_vista_800CDRed_B + nRed);
		const size_t nGreen = sizeof(Agfacolor_vista_800CDGreen_I) / sizeof(Agfacolor_vista_800CDGreen_I[0]);
		GreenI.assign(Agfacolor_vista_800CDGreen_I, Agfacolor_vista_800CDGreen_I + nGreen);
		GreenB.assign(Agfacolor_vista_800CDGreen_B, Agfacolor_vista_800CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Agfacolor_vista_800CDBlue_I) / sizeof(Agfacolor_vista_800CDBlue_I[0]);
		BlueI.assign(Agfacolor_vista_800CDBlue_I, Agfacolor_vista_800CDBlue_I + nBlue);
		BlueB.assign(Agfacolor_vista_800CDBlue_B, Agfacolor_vista_800CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Agfapan_apx_025CD") {
		const size_t nRed = sizeof(Agfapan_apx_025CDRed_I) / sizeof(Agfapan_apx_025CDRed_I[0]);
		RedI.assign(Agfapan_apx_025CDRed_I, Agfapan_apx_025CDRed_I + nRed);
		RedB.assign(Agfapan_apx_025CDRed_B, Agfapan_apx_025CDRed_B + nRed);
		color = false;
	} else if (filmName == "Agfapan_apx_100CD") {
		const size_t nRed = sizeof(Agfapan_apx_100CDRed_I) / sizeof(Agfapan_apx_100CDRed_I[0]);
		RedI.assign(Agfapan_apx_100CDRed_I, Agfapan_apx_100CDRed_I + nRed);
		RedB.assign(Agfapan_apx_100CDRed_B, Agfapan_apx_100CDRed_B + nRed);
		color = false;
	} else if (filmName == "Agfapan_apx_400CD") {
		const size_t nRed = sizeof(Agfapan_apx_400CDRed_I) / sizeof(Agfapan_apx_400CDRed_I[0]);
		RedI.assign(Agfapan_apx_400CDRed_I, Agfapan_apx_400CDRed_I + nRed);
		RedB.assign(Agfapan_apx_400CDRed_B, Agfapan_apx_400CDRed_B + nRed);
		color = false;
	} else if (filmName == "Ektachrome_100_plusCD") {
		const size_t nRed = sizeof(Ektachrome_100_plusCDRed_I) / sizeof(Ektachrome_100_plusCDRed_I[0]);
		RedI.assign(Ektachrome_100_plusCDRed_I, Ektachrome_100_plusCDRed_I + nRed);
		RedB.assign(Ektachrome_100_plusCDRed_B, Ektachrome_100_plusCDRed_B + nRed);
		const size_t nGreen = sizeof(Ektachrome_100_plusCDGreen_I) / sizeof(Ektachrome_100_plusCDGreen_I[0]);
		GreenI.assign(Ektachrome_100_plusCDGreen_I, Ektachrome_100_plusCDGreen_I + nGreen);
		GreenB.assign(Ektachrome_100_plusCDGreen_B, Ektachrome_100_plusCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Ektachrome_100_plusCDBlue_I) / sizeof(Ektachrome_100_plusCDBlue_I[0]);
		BlueI.assign(Ektachrome_100_plusCDBlue_I, Ektachrome_100_plusCDBlue_I + nBlue);
		BlueB.assign(Ektachrome_100_plusCDBlue_B, Ektachrome_100_plusCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Ektachrome_100CD") {
		const size_t nRed = sizeof(Ektachrome_100CDRed_I) / sizeof(Ektachrome_100CDRed_I[0]);
		RedI.assign(Ektachrome_100CDRed_I, Ektachrome_100CDRed_I + nRed);
		RedB.assign(Ektachrome_100CDRed_B, Ektachrome_100CDRed_B + nRed);
		const size_t nGreen = sizeof(Ektachrome_100CDGreen_I) / sizeof(Ektachrome_100CDGreen_I[0]);
		GreenI.assign(Ektachrome_100CDGreen_I, Ektachrome_100CDGreen_I + nGreen);
		GreenB.assign(Ektachrome_100CDGreen_B, Ektachrome_100CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Ektachrome_100CDBlue_I) / sizeof(Ektachrome_100CDBlue_I[0]);
		BlueI.assign(Ektachrome_100CDBlue_I, Ektachrome_100CDBlue_I + nBlue);
		BlueB.assign(Ektachrome_100CDBlue_B, Ektachrome_100CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Ektachrome_320TCD") {
		const size_t nRed = sizeof(Ektachrome_320TCDRed_I) / sizeof(Ektachrome_320TCDRed_I[0]);
		RedI.assign(Ektachrome_320TCDRed_I, Ektachrome_320TCDRed_I + nRed);
		RedB.assign(Ektachrome_320TCDRed_B, Ektachrome_320TCDRed_B + nRed);
		const size_t nGreen = sizeof(Ektachrome_320TCDGreen_I) / sizeof(Ektachrome_320TCDGreen_I[0]);
		GreenI.assign(Ektachrome_320TCDGreen_I, Ektachrome_320TCDGreen_I + nGreen);
		GreenB.assign(Ektachrome_320TCDGreen_B, Ektachrome_320TCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Ektachrome_320TCDBlue_I) / sizeof(Ektachrome_320TCDBlue_I[0]);
		BlueI.assign(Ektachrome_320TCDBlue_I, Ektachrome_320TCDBlue_I + nBlue);
		BlueB.assign(Ektachrome_320TCDBlue_B, Ektachrome_320TCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Ektachrome_400XCD") {
		const size_t nRed = sizeof(Ektachrome_400XCDRed_I) / sizeof(Ektachrome_400XCDRed_I[0]);
		RedI.assign(Ektachrome_400XCDRed_I, Ektachrome_400XCDRed_I + nRed);
		RedB.assign(Ektachrome_400XCDRed_B, Ektachrome_400XCDRed_B + nRed);
		const size_t nGreen = sizeof(Ektachrome_400XCDGreen_I) / sizeof(Ektachrome_400XCDGreen_I[0]);
		GreenI.assign(Ektachrome_400XCDGreen_I, Ektachrome_400XCDGreen_I + nGreen);
		GreenB.assign(Ektachrome_400XCDGreen_B, Ektachrome_400XCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Ektachrome_400XCDBlue_I) / sizeof(Ektachrome_400XCDBlue_I[0]);
		BlueI.assign(Ektachrome_400XCDBlue_I, Ektachrome_400XCDBlue_I + nBlue);
		BlueB.assign(Ektachrome_400XCDBlue_B, Ektachrome_400XCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Ektachrome_64CD") {
		const size_t nRed = sizeof(Ektachrome_64CDRed_I) / sizeof(Ektachrome_64CDRed_I[0]);
		RedI.assign(Ektachrome_64CDRed_I, Ektachrome_64CDRed_I + nRed);
		RedB.assign(Ektachrome_64CDRed_B, Ektachrome_64CDRed_B + nRed);
		const size_t nGreen = sizeof(Ektachrome_64CDGreen_I) / sizeof(Ektachrome_64CDGreen_I[0]);
		GreenI.assign(Ektachrome_64CDGreen_I, Ektachrome_64CDGreen_I + nGreen);
		GreenB.assign(Ektachrome_64CDGreen_B, Ektachrome_64CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Ektachrome_64CDBlue_I) / sizeof(Ektachrome_64CDBlue_I[0]);
		BlueI.assign(Ektachrome_64CDBlue_I, Ektachrome_64CDBlue_I + nBlue);
		BlueB.assign(Ektachrome_64CDBlue_B, Ektachrome_64CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Ektachrome_64TCD") {
		const size_t nRed = sizeof(Ektachrome_64TCDRed_I) / sizeof(Ektachrome_64TCDRed_I[0]);
		RedI.assign(Ektachrome_64TCDRed_I, Ektachrome_64TCDRed_I + nRed);
		RedB.assign(Ektachrome_64TCDRed_B, Ektachrome_64TCDRed_B + nRed);
		const size_t nGreen = sizeof(Ektachrome_64TCDGreen_I) / sizeof(Ektachrome_64TCDGreen_I[0]);
		GreenI.assign(Ektachrome_64TCDGreen_I, Ektachrome_64TCDGreen_I + nGreen);
		GreenB.assign(Ektachrome_64TCDGreen_B, Ektachrome_64TCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Ektachrome_64TCDBlue_I) / sizeof(Ektachrome_64TCDBlue_I[0]);
		BlueI.assign(Ektachrome_64TCDBlue_I, Ektachrome_64TCDBlue_I + nBlue);
		BlueB.assign(Ektachrome_64TCDBlue_B, Ektachrome_64TCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Ektachrome_E100SCD") {
		const size_t nRed = sizeof(Ektachrome_E100SCDRed_I) / sizeof(Ektachrome_E100SCDRed_I[0]);
		RedI.assign(Ektachrome_E100SCDRed_I, Ektachrome_E100SCDRed_I + nRed);
		RedB.assign(Ektachrome_E100SCDRed_B, Ektachrome_E100SCDRed_B + nRed);
		const size_t nGreen = sizeof(Ektachrome_E100SCDGreen_I) / sizeof(Ektachrome_E100SCDGreen_I[0]);
		GreenI.assign(Ektachrome_E100SCDGreen_I, Ektachrome_E100SCDGreen_I + nGreen);
		GreenB.assign(Ektachrome_E100SCDGreen_B, Ektachrome_E100SCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Ektachrome_E100SCDBlue_I) / sizeof(Ektachrome_E100SCDBlue_I[0]);
		BlueI.assign(Ektachrome_E100SCDBlue_I, Ektachrome_E100SCDBlue_I + nBlue);
		BlueB.assign(Ektachrome_E100SCDBlue_B, Ektachrome_E100SCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "F125CD") {
		const size_t nRed = sizeof(F125CDRed_I) / sizeof(F125CDRed_I[0]);
		RedI.assign(F125CDRed_I, F125CDRed_I + nRed);
		RedB.assign(F125CDRed_B, F125CDRed_B + nRed);
		const size_t nGreen = sizeof(F125CDGreen_I) / sizeof(F125CDGreen_I[0]);
		GreenI.assign(F125CDGreen_I, F125CDGreen_I + nGreen);
		GreenB.assign(F125CDGreen_B, F125CDGreen_B + nGreen);
		const size_t nBlue = sizeof(F125CDBlue_I) / sizeof(F125CDBlue_I[0]);
		BlueI.assign(F125CDBlue_I, F125CDBlue_I + nBlue);
		BlueB.assign(F125CDBlue_B, F125CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "F250CD") {
		const size_t nRed = sizeof(F250CDRed_I) / sizeof(F250CDRed_I[0]);
		RedI.assign(F250CDRed_I, F250CDRed_I + nRed);
		RedB.assign(F250CDRed_B, F250CDRed_B + nRed);
		const size_t nGreen = sizeof(F250CDGreen_I) / sizeof(F250CDGreen_I[0]);
		GreenI.assign(F250CDGreen_I, F250CDGreen_I + nGreen);
		GreenB.assign(F250CDGreen_B, F250CDGreen_B + nGreen);
		const size_t nBlue = sizeof(F250CDBlue_I) / sizeof(F250CDBlue_I[0]);
		BlueI.assign(F250CDBlue_I, F250CDBlue_I + nBlue);
		BlueB.assign(F250CDBlue_B, F250CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "F400CD") {
		const size_t nRed = sizeof(F400CDRed_I) / sizeof(F400CDRed_I[0]);
		RedI.assign(F400CDRed_I, F400CDRed_I + nRed);
		RedB.assign(F400CDRed_B, F400CDRed_B + nRed);
		const size_t nGreen = sizeof(F400CDGreen_I) / sizeof(F400CDGreen_I[0]);
		GreenI.assign(F400CDGreen_I, F400CDGreen_I + nGreen);
		GreenB.assign(F400CDGreen_B, F400CDGreen_B + nGreen);
		const size_t nBlue = sizeof(F400CDBlue_I) / sizeof(F400CDBlue_I[0]);
		BlueI.assign(F400CDBlue_I, F400CDBlue_I + nBlue);
		BlueB.assign(F400CDBlue_B, F400CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "FCICD") {
		const size_t nRed = sizeof(FCICDRed_I) / sizeof(FCICDRed_I[0]);
		RedI.assign(FCICDRed_I, FCICDRed_I + nRed);
		RedB.assign(FCICDRed_B, FCICDRed_B + nRed);
		const size_t nGreen = sizeof(FCICDGreen_I) / sizeof(FCICDGreen_I[0]);
		GreenI.assign(FCICDGreen_I, FCICDGreen_I + nGreen);
		GreenB.assign(FCICDGreen_B, FCICDGreen_B + nGreen);
		const size_t nBlue = sizeof(FCICDBlue_I) / sizeof(FCICDBlue_I[0]);
		BlueI.assign(FCICDBlue_I, FCICDBlue_I + nBlue);
		BlueB.assign(FCICDBlue_B, FCICDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Gold_100CD") {
		const size_t nRed = sizeof(Gold_100CDRed_I) / sizeof(Gold_100CDRed_I[0]);
		RedI.assign(Gold_100CDRed_I, Gold_100CDRed_I + nRed);
		RedB.assign(Gold_100CDRed_B, Gold_100CDRed_B + nRed);
		const size_t nGreen = sizeof(Gold_100CDGreen_I) / sizeof(Gold_100CDGreen_I[0]);
		GreenI.assign(Gold_100CDGreen_I, Gold_100CDGreen_I + nGreen);
		GreenB.assign(Gold_100CDGreen_B, Gold_100CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Gold_100CDBlue_I) / sizeof(Gold_100CDBlue_I[0]);
		BlueI.assign(Gold_100CDBlue_I, Gold_100CDBlue_I + nBlue);
		BlueB.assign(Gold_100CDBlue_B, Gold_100CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Gold_200CD") {
		const size_t nRed = sizeof(Gold_200CDRed_I) / sizeof(Gold_200CDRed_I[0]);
		RedI.assign(Gold_200CDRed_I, Gold_200CDRed_I + nRed);
		RedB.assign(Gold_200CDRed_B, Gold_200CDRed_B + nRed);
		const size_t nGreen = sizeof(Gold_200CDGreen_I) / sizeof(Gold_200CDGreen_I[0]);
		GreenI.assign(Gold_200CDGreen_I, Gold_200CDGreen_I + nGreen);
		GreenB.assign(Gold_200CDGreen_B, Gold_200CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Gold_200CDBlue_I) / sizeof(Gold_200CDBlue_I[0]);
		BlueI.assign(Gold_200CDBlue_I, Gold_200CDBlue_I + nBlue);
		BlueB.assign(Gold_200CDBlue_B, Gold_200CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Kodachrome_200CD") {
		const size_t nRed = sizeof(Kodachrome_200CDRed_I) / sizeof(Kodachrome_200CDRed_I[0]);
		RedI.assign(Kodachrome_200CDRed_I, Kodachrome_200CDRed_I + nRed);
		RedB.assign(Kodachrome_200CDRed_B, Kodachrome_200CDRed_B + nRed);
		const size_t nGreen = sizeof(Kodachrome_200CDGreen_I) / sizeof(Kodachrome_200CDGreen_I[0]);
		GreenI.assign(Kodachrome_200CDGreen_I, Kodachrome_200CDGreen_I + nGreen);
		GreenB.assign(Kodachrome_200CDGreen_B, Kodachrome_200CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Kodachrome_200CDBlue_I) / sizeof(Kodachrome_200CDBlue_I[0]);
		BlueI.assign(Kodachrome_200CDBlue_I, Kodachrome_200CDBlue_I + nBlue);
		BlueB.assign(Kodachrome_200CDBlue_B, Kodachrome_200CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Kodachrome_25CD") {
		const size_t nRed = sizeof(Kodachrome_25CDRed_I) / sizeof(Kodachrome_25CDRed_I[0]);
		RedI.assign(Kodachrome_25CDRed_I, Kodachrome_25CDRed_I + nRed);
		RedB.assign(Kodachrome_25CDRed_B, Kodachrome_25CDRed_B + nRed);
		const size_t nGreen = sizeof(Kodachrome_25CDGreen_I) / sizeof(Kodachrome_25CDGreen_I[0]);
		GreenI.assign(Kodachrome_25CDGreen_I, Kodachrome_25CDGreen_I + nGreen);
		GreenB.assign(Kodachrome_25CDGreen_B, Kodachrome_25CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Kodachrome_25CDBlue_I) / sizeof(Kodachrome_25CDBlue_I[0]);
		BlueI.assign(Kodachrome_25CDBlue_I, Kodachrome_25CDBlue_I + nBlue);
		BlueB.assign(Kodachrome_25CDBlue_B, Kodachrome_25CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Kodachrome_64CD") {
		const size_t nRed = sizeof(Kodachrome_64CDRed_I) / sizeof(Kodachrome_64CDRed_I[0]);
		RedI.assign(Kodachrome_64CDRed_I, Kodachrome_64CDRed_I + nRed);
		RedB.assign(Kodachrome_64CDRed_B, Kodachrome_64CDRed_B + nRed);
		const size_t nGreen = sizeof(Kodachrome_64CDGreen_I) / sizeof(Kodachrome_64CDGreen_I[0]);
		GreenI.assign(Kodachrome_64CDGreen_I, Kodachrome_64CDGreen_I + nGreen);
		GreenB.assign(Kodachrome_64CDGreen_B, Kodachrome_64CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Kodachrome_64CDBlue_I) / sizeof(Kodachrome_64CDBlue_I[0]);
		BlueI.assign(Kodachrome_64CDBlue_I, Kodachrome_64CDBlue_I + nBlue);
		BlueB.assign(Kodachrome_64CDBlue_B, Kodachrome_64CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Max_Zoom_800CD") {
		const size_t nRed = sizeof(Max_Zoom_800CDRed_I) / sizeof(Max_Zoom_800CDRed_I[0]);
		RedI.assign(Max_Zoom_800CDRed_I, Max_Zoom_800CDRed_I + nRed);
		RedB.assign(Max_Zoom_800CDRed_B, Max_Zoom_800CDRed_B + nRed);
		const size_t nGreen = sizeof(Max_Zoom_800CDGreen_I) / sizeof(Max_Zoom_800CDGreen_I[0]);
		GreenI.assign(Max_Zoom_800CDGreen_I, Max_Zoom_800CDGreen_I + nGreen);
		GreenB.assign(Max_Zoom_800CDGreen_B, Max_Zoom_800CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Max_Zoom_800CDBlue_I) / sizeof(Max_Zoom_800CDBlue_I[0]);
		BlueI.assign(Max_Zoom_800CDBlue_I, Max_Zoom_800CDBlue_I + nBlue);
		BlueB.assign(Max_Zoom_800CDBlue_B, Max_Zoom_800CDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Portra_100TCD") {
		const size_t nRed = sizeof(Portra_100TCDRed_I) / sizeof(Portra_100TCDRed_I[0]);
		RedI.assign(Portra_100TCDRed_I, Portra_100TCDRed_I + nRed);
		RedB.assign(Portra_100TCDRed_B, Portra_100TCDRed_B + nRed);
		const size_t nGreen = sizeof(Portra_100TCDGreen_I) / sizeof(Portra_100TCDGreen_I[0]);
		GreenI.assign(Portra_100TCDGreen_I, Portra_100TCDGreen_I + nGreen);
		GreenB.assign(Portra_100TCDGreen_B, Portra_100TCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Portra_100TCDBlue_I) / sizeof(Portra_100TCDBlue_I[0]);
		BlueI.assign(Portra_100TCDBlue_I, Portra_100TCDBlue_I + nBlue);
		BlueB.assign(Portra_100TCDBlue_B, Portra_100TCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Portra_160NCCD") {
		const size_t nRed = sizeof(Portra_160NCCDRed_I) / sizeof(Portra_160NCCDRed_I[0]);
		RedI.assign(Portra_160NCCDRed_I, Portra_160NCCDRed_I + nRed);
		RedB.assign(Portra_160NCCDRed_B, Portra_160NCCDRed_B + nRed);
		const size_t nGreen = sizeof(Portra_160NCCDGreen_I) / sizeof(Portra_160NCCDGreen_I[0]);
		GreenI.assign(Portra_160NCCDGreen_I, Portra_160NCCDGreen_I + nGreen);
		GreenB.assign(Portra_160NCCDGreen_B, Portra_160NCCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Portra_160NCCDBlue_I) / sizeof(Portra_160NCCDBlue_I[0]);
		BlueI.assign(Portra_160NCCDBlue_I, Portra_160NCCDBlue_I + nBlue);
		BlueB.assign(Portra_160NCCDBlue_B, Portra_160NCCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Portra_160VCCD") {
		const size_t nRed = sizeof(Portra_160VCCDRed_I) / sizeof(Portra_160VCCDRed_I[0]);
		RedI.assign(Portra_160VCCDRed_I, Portra_160VCCDRed_I + nRed);
		RedB.assign(Portra_160VCCDRed_B, Portra_160VCCDRed_B + nRed);
		const size_t nGreen = sizeof(Portra_160VCCDGreen_I) / sizeof(Portra_160VCCDGreen_I[0]);
		GreenI.assign(Portra_160VCCDGreen_I, Portra_160VCCDGreen_I + nGreen);
		GreenB.assign(Portra_160VCCDGreen_B, Portra_160VCCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Portra_160VCCDBlue_I) / sizeof(Portra_160VCCDBlue_I[0]);
		BlueI.assign(Portra_160VCCDBlue_I, Portra_160VCCDBlue_I + nBlue);
		BlueB.assign(Portra_160VCCDBlue_B, Portra_160VCCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Portra_400NCCD") {
		const size_t nRed = sizeof(Portra_400NCCDRed_I) / sizeof(Portra_400NCCDRed_I[0]);
		RedI.assign(Portra_400NCCDRed_I, Portra_400NCCDRed_I + nRed);
		RedB.assign(Portra_400NCCDRed_B, Portra_400NCCDRed_B + nRed);
		const size_t nGreen = sizeof(Portra_400NCCDGreen_I) / sizeof(Portra_400NCCDGreen_I[0]);
		GreenI.assign(Portra_400NCCDGreen_I, Portra_400NCCDGreen_I + nGreen);
		GreenB.assign(Portra_400NCCDGreen_B, Portra_400NCCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Portra_400NCCDBlue_I) / sizeof(Portra_400NCCDBlue_I[0]);
		BlueI.assign(Portra_400NCCDBlue_I, Portra_400NCCDBlue_I + nBlue);
		BlueB.assign(Portra_400NCCDBlue_B, Portra_400NCCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Portra_400VCCD") {
		const size_t nRed = sizeof(Portra_400VCCDRed_I) / sizeof(Portra_400VCCDRed_I[0]);
		RedI.assign(Portra_400VCCDRed_I, Portra_400VCCDRed_I + nRed);
		RedB.assign(Portra_400VCCDRed_B, Portra_400VCCDRed_B + nRed);
		const size_t nGreen = sizeof(Portra_400VCCDGreen_I) / sizeof(Portra_400VCCDGreen_I[0]);
		GreenI.assign(Portra_400VCCDGreen_I, Portra_400VCCDGreen_I + nGreen);
		GreenB.assign(Portra_400VCCDGreen_B, Portra_400VCCDGreen_B + nGreen);
		const size_t nBlue = sizeof(Portra_400VCCDBlue_I) / sizeof(Portra_400VCCDBlue_I[0]);
		BlueI.assign(Portra_400VCCDBlue_I, Portra_400VCCDBlue_I + nBlue);
		BlueB.assign(Portra_400VCCDBlue_B, Portra_400VCCDBlue_B + nBlue);
		color = true;
	} else if (filmName == "Portra_800CD") {
		const size_t nRed = sizeof(Portra_800CDRed_I) / sizeof(Portra_800CDRed_I[0]);
		RedI.assign(Portra_800CDRed_I, Portra_800CDRed_I + nRed);
		RedB.assign(Portra_800CDRed_B, Portra_800CDRed_B + nRed);
		const size_t nGreen = sizeof(Portra_800CDGreen_I) / sizeof(Portra_800CDGreen_I[0]);
		GreenI.assign(Portra_800CDGreen_I, Portra_800CDGreen_I + nGreen);
		GreenB.assign(Portra_800CDGreen_B, Portra_800CDGreen_B + nGreen);
		const size_t nBlue = sizeof(Portra_800CDBlue_I) / sizeof(Portra_800CDBlue_I[0]);
		BlueI.assign(Portra_800CDBlue_I, Portra_800CDBlue_I + nBlue);
		BlueB.assign(Portra_800CDBlue_B, Portra_800CDBlue_B + nBlue);
		color = true;
	} else
		return false;

	return true;
}
