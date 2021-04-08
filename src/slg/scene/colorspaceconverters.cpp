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
	
#include "luxrays/utils/strutils.h"
#include "slg/scene/colorspaceconverters.h"
#include "slg/utils/filenameresolver.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ColorSpaceConverters
//------------------------------------------------------------------------------

ColorSpaceConverters::ColorSpaceConverters() {
}

ColorSpaceConverters::~ColorSpaceConverters() {
}

void ColorSpaceConverters::ConvertFrom(const ColorSpaceConfig &cfg, float &v) {
	switch (cfg.colorSpaceType) {
		case ColorSpaceConfig::NOP_COLORSPACE:
			// Nothing to do
			break;
		case ColorSpaceConfig::LUXCORE_COLORSPACE:
			ConvertFromLuxCore(cfg.luxcore.gamma, v);
			break;
		case ColorSpaceConfig::OPENCOLORIO_COLORSPACE:
			ConvertFromOpenColorIO(cfg.ocio.configName, cfg.ocio.colorSpaceName, v);
			break;
		default:
			throw runtime_error("Unknown color space type in ColorSpaceConverters::ConvertFrom(): " + ToString(cfg.colorSpaceType));
	}
}

void ColorSpaceConverters::ConvertFrom(const ColorSpaceConfig &cfg, luxrays::Spectrum &c) {
	switch (cfg.colorSpaceType) {
		case ColorSpaceConfig::NOP_COLORSPACE:
			// Nothing to do
			break;
		case ColorSpaceConfig::LUXCORE_COLORSPACE:
			ConvertFromLuxCore(cfg.luxcore.gamma, c);
			break;
		case ColorSpaceConfig::OPENCOLORIO_COLORSPACE:
			ConvertFromOpenColorIO(cfg.ocio.configName, cfg.ocio.colorSpaceName, c);
			break;
		default:
			throw runtime_error("Unknown color space type in ColorSpaceConverters::ConvertFrom(): " + ToString(cfg.colorSpaceType));
	}
}

//------------------------------------------------------------------------------
// LuxCore color space
//------------------------------------------------------------------------------

void ColorSpaceConverters::ConvertFromLuxCore(const float gamma, float &v) {
	v = powf(v, gamma);
}

void ColorSpaceConverters::ConvertFromLuxCore(const float gamma, Spectrum &c) {
	c.c[0] = powf(c.c[0], gamma);
	c.c[1] = powf(c.c[1], gamma);
	c.c[2] = powf(c.c[2], gamma);
}

//------------------------------------------------------------------------------
// OpenColorIO color space
//------------------------------------------------------------------------------

void ColorSpaceConverters::ConvertFromOpenColorIO(const string &configFileName,
		const string &inputColorSpace, float &v) {
	Spectrum c(v);

	ConvertFromOpenColorIO(configFileName, inputColorSpace, c);

	v = c.Y();
}

static string GetOCIOProcessorCacheKey(const string &configFileName,
		const string &inputColorSpace) {
	return configFileName + "_#_" + inputColorSpace;
}

void ColorSpaceConverters::ConvertFromOpenColorIO(const string &configFileName,
		const string &inputColorSpace, Spectrum &c) {
	try {
		OCIO::ConstCPUProcessorRcPtr cpu;

		// Look for the processor inside the cache
		const string key = GetOCIOProcessorCacheKey(configFileName, inputColorSpace);
		auto it = ocioProcessorCache.find(key);
		if (it == ocioProcessorCache.end()) {
			// Not found, insert a new one into the cache
			OCIO::ConstConfigRcPtr config = (configFileName == "") ?
				OCIO::GetCurrentConfig() :
				OCIO::Config::CreateFromFile(SLG_FileNameResolver.ResolveFile(configFileName).c_str());

			OCIO::ConstProcessorRcPtr processor = config->getProcessor(inputColorSpace.c_str(), OCIO::ROLE_SCENE_LINEAR);

			cpu = processor->getDefaultCPUProcessor();

			ocioProcessorCache[key] = cpu;
		} else
			cpu = it->second;

		/// Apply the color transform with OpenColorIO
		OCIO::PackedImageDesc img(c.c, 1, 1, 3);
		cpu->apply(img);
	} catch (OCIO::Exception &exception) {
		throw runtime_error("OpenColorIO Error in ColorSpaceConverters::ConvertFromOpenColorIO(): " + string(exception.what()));
	}
}
