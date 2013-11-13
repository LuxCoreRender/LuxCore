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
	const int index = Clamp<int>(luxrays::Floor2UInt(tableSize * Clamp(x, 0.f, 1.f)), 0, tableSize - 1);
	return gammaTable[index];
}

void GammaCorrectionPlugin::Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const {
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i]) {
			pixels[i].r = Radiance2PixelFloat(pixels[i].r);
			pixels[i].g = Radiance2PixelFloat(pixels[i].g);
			pixels[i].b = Radiance2PixelFloat(pixels[i].b);
		}
	}
}

//------------------------------------------------------------------------------
// Nop plugin
//------------------------------------------------------------------------------

ImagePipelinePlugin *NopPlugin::Copy() const {
	return new NopPlugin();
}

void NopPlugin::Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const {
}

//------------------------------------------------------------------------------
// OutputSwitcher plugin
//------------------------------------------------------------------------------

ImagePipelinePlugin *OutputSwitcherPlugin::Copy() const {
	return new OutputSwitcherPlugin(type, index);
}

void OutputSwitcherPlugin::Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const {
	// Copy the data from another Film output channel

	// Do nothing if the Film is missing this particular channel
	if (!film.HasChannel(type))
		return;

	const u_int pixelCount = film.GetWidth() * film.GetHeight();
	switch (type) {
		case Film::RADIANCE_PER_PIXEL_NORMALIZED:
			break;
		case Film::RADIANCE_PER_SCREEN_NORMALIZED:
			break;
		case Film::ALPHA: {
			for (u_int i = 0; i < pixelCount; ++i) {
				float a;
				film.channel_ALPHA->GetWeightedPixel(i, &a);
				pixels[i].r = a;
				pixels[i].g = a;
				pixels[i].b = a;
			}
			break;
		}
		case Film::DEPTH: {
			for (u_int i = 0; i < pixelCount; ++i) {
				float d;
				film.channel_DEPTH->GetWeightedPixel(i, &d);
				pixels[i].r = d;
				pixels[i].g = d;
				pixels[i].b = d;
			}
			break;
		}
		case Film::POSITION:
			break;
		case Film::GEOMETRY_NORMAL:
			break;
		case Film::SHADING_NORMAL:
			break;
		case Film::MATERIAL_ID:
			break;
		case Film::DIRECT_DIFFUSE:
			break;
		case Film::DIRECT_GLOSSY:
			break;
		case Film::EMISSION:
			break;
		case Film::INDIRECT_DIFFUSE:
			break;
		case Film::INDIRECT_GLOSSY:
			break;
		case Film::INDIRECT_SPECULAR:
			break;
		case Film::MATERIAL_ID_MASK:
			break;
		case Film::DIRECT_SHADOW_MASK:
			break;
		case Film::INDIRECT_SHADOW_MASK:
			break;
		case Film::UV:
			break;
		case Film::RAYCOUNT:
			break;
		default:
			throw runtime_error("Unknown film output type in OutputSwitcherPlugin::Apply(): " + ToString(type));
	}
}
