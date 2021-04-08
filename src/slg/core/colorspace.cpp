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

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;

#include "luxrays/utils/strutils.h"
#include "slg/core/colorspace.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ColorSpaceConfig
//------------------------------------------------------------------------------

namespace slg {
const ColorSpaceConfig ColorSpaceConfig::defaultNopConfig = ColorSpaceConfig();
const ColorSpaceConfig ColorSpaceConfig::defaultLuxCoreConfig = ColorSpaceConfig(2.2f);
const ColorSpaceConfig ColorSpaceConfig::defaultOpenColorIOConfig = ColorSpaceConfig("", OCIO::ROLE_TEXTURE_PAINT);
}

ColorSpaceConfig::ColorSpaceConfig() {
	colorSpaceType = NOP_COLORSPACE;
	luxcore.gamma = 2.2f;
	ocio.configName = "";
	ocio.colorSpaceName = OCIO::ROLE_TEXTURE_PAINT;
}

ColorSpaceConfig::ColorSpaceConfig(const float gamma) {
	colorSpaceType = LUXCORE_COLORSPACE;
	luxcore.gamma = gamma;

	ocio.configName = "";
	ocio.colorSpaceName = OCIO::ROLE_TEXTURE_PAINT;
}

ColorSpaceConfig::ColorSpaceConfig(const string &configName, const string &colorSpaceName) {
	colorSpaceType = OPENCOLORIO_COLORSPACE;
	ocio.configName = configName;
	ocio.colorSpaceName = colorSpaceName;

	luxcore.gamma = 2.2f;
}

ColorSpaceConfig::~ColorSpaceConfig() {
}

ColorSpaceConfig::ColorSpaceConfig(const Properties &props, const string &prefix, const ColorSpaceConfig &defaultCfg) {
	FromProperties(props, prefix, *this, defaultCfg);
}

void ColorSpaceConfig::FromProperties(const Properties &props, const string &prefix,
		ColorSpaceConfig &colorSpaceCfg, const ColorSpaceConfig &defaultCfg) {
	ColorSpaceType type = String2ColorSpaceType(props.Get(Property(prefix + ".colorspace")(
			ColorSpaceType2String(defaultCfg.colorSpaceType))).Get<string>());

	colorSpaceCfg = defaultCfg;
	
	switch (type) {
		case ColorSpaceConfig::NOP_COLORSPACE: {
			colorSpaceCfg.colorSpaceType = NOP_COLORSPACE;
			break;
		}
		case ColorSpaceConfig::LUXCORE_COLORSPACE: {
			colorSpaceCfg.colorSpaceType = LUXCORE_COLORSPACE;
			// For compatibility with the past
			const float oldGamma = props.Get(Property(prefix + ".gamma")(defaultCfg.luxcore.gamma)).Get<float>();
			colorSpaceCfg.luxcore.gamma = props.Get(Property(prefix + ".colorspace.gamma")(oldGamma)).Get<float>();
			break;
		}
		case ColorSpaceConfig::OPENCOLORIO_COLORSPACE: {
			colorSpaceCfg.colorSpaceType = OPENCOLORIO_COLORSPACE;
			colorSpaceCfg.ocio.configName = props.Get(Property(prefix + ".colorspace.config")(defaultCfg.ocio.configName)).Get<string>();
			colorSpaceCfg.ocio.colorSpaceName = props.Get(Property(prefix + ".colorspace.name")(defaultCfg.ocio.colorSpaceName)).Get<string>();
			break;
		}
		default:
			throw runtime_error("Unknown color space in ColorSpaceConfig::FromProperties(): " + ToString(type));
	}
}

ColorSpaceConfig::ColorSpaceType ColorSpaceConfig::String2ColorSpaceType(const string &type) {
	if (type == "nop")
		return ColorSpaceConfig::NOP_COLORSPACE;
	else if (type == "luxcore")
		return ColorSpaceConfig::LUXCORE_COLORSPACE;
	else if (type == "opencolorio")
		return ColorSpaceConfig::OPENCOLORIO_COLORSPACE;
	else
		throw runtime_error("Unknown color space config type: " + type);
}

string ColorSpaceConfig::ColorSpaceType2String(const ColorSpaceConfig::ColorSpaceType type) {
	switch (type) {
		case ColorSpaceConfig::NOP_COLORSPACE:
			return "nop";
		case ColorSpaceConfig::LUXCORE_COLORSPACE:
			return "luxcore";
		case ColorSpaceConfig::OPENCOLORIO_COLORSPACE:
			return "opencolorio";
		default:
			throw runtime_error("Unsupported wrap type in ColorSpaceConfig::ColorSpaceType2String(): " + ToString(type));
	}
}
