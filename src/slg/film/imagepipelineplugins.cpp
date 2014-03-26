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

#include <boost/foreach.hpp>

#include "slg/film/imagepipelineplugins.h"
#include "slg/film/film.h"

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
