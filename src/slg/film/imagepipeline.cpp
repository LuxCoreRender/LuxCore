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

#include "slg/film/imagepipeline.h"
#include "slg/film/film.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImagePipeline
//------------------------------------------------------------------------------

ImagePipeline::~ImagePipeline() {
	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline)
		delete plugin;
}

ImagePipeline *ImagePipeline::Copy() const {
	ImagePipeline *ip = new ImagePipeline();

	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline) {
		ip->AddPlugin(plugin->Copy());
	}

	return ip;
}

void ImagePipeline::AddPlugin(ImagePipelinePlugin *plugin) {
	pipeline.push_back(plugin);
}

void ImagePipeline::Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const {
	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline) {
		plugin->Apply(film, pixels, pixelsMask);
	}
}

const ImagePipelinePlugin *ImagePipeline::GetPlugin(const std::type_info &type) const {
	BOOST_FOREACH(const ImagePipelinePlugin *plugin, pipeline) {
		if (typeid(*plugin) == type)
			return plugin;
	}

	return NULL;
}

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
	const int index = Clamp<int>(luxrays::Floor2UInt(tableSize * x), 0, tableSize - 1);
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
