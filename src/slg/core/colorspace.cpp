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

#include "slg/core/colorspace.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ColorSpaceConfig
//------------------------------------------------------------------------------

ColorSpaceConfig::ColorSpaceConfig() {
	colorSpaceType = NOP_COLORSPACE;
}

ColorSpaceConfig::ColorSpaceConfig(const float gamma) {
	colorSpaceType = LUXCORE_COLORSPACE;
	luxcore.gamma = gamma;
}

ColorSpaceConfig::ColorSpaceConfig(const string &configName, const string &colorSpaceName) {
	colorSpaceType = OPENCOLORIO_COLORSPACE;
	ocio.configName = configName;
	ocio.colorSpaceName = colorSpaceName;
}

ColorSpaceConfig::~ColorSpaceConfig() {
}

ColorSpaceConfig::ColorSpaceConfig(const Properties &props, const string &prefix) {
	FromProperties(props, prefix, *this);
}

void ColorSpaceConfig::FromProperties(const Properties &props, const string &prefix, ColorSpaceConfig &colorSpaceCfg) {		
	const string colorSpace = props.Get(Property(prefix + ".colorspace")("luxcore")).Get<string>();

	if (colorSpace == "nop") {
		colorSpaceCfg.colorSpaceType = NOP_COLORSPACE;
	} else if (colorSpace == "luxcore") {
		colorSpaceCfg.colorSpaceType = LUXCORE_COLORSPACE;
		// For compatibility with the past
		const float oldGamma = props.Get(Property(prefix + ".gamma")(2.2f)).Get<float>();
		colorSpaceCfg.luxcore.gamma = props.Get(Property(prefix + ".colorspace.gamma")(oldGamma)).Get<float>();
	} else if (colorSpace == "opencolorio") {
		colorSpaceCfg.colorSpaceType = OPENCOLORIO_COLORSPACE;
		colorSpaceCfg.ocio.configName = props.Get(Property(prefix + ".colorspace.config")("")).Get<string>();
		colorSpaceCfg.ocio.colorSpaceName = props.Get(Property(prefix + ".colorspace.name")(OCIO::ROLE_TEXTURE_PAINT)).Get<string>();
	} else
		throw runtime_error("Unknown color space in ColorSpaceConfig::FromProperties(): " + colorSpace);
}
