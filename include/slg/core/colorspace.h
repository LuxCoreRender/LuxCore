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

#ifndef _LUXRAYS_COLORSPACE_H
#define _LUXRAYS_COLORSPACE_H

#include <string>

#include "luxrays/utils/properties.h"

namespace slg {

//------------------------------------------------------------------------------
// ColorSpaceConfig
//------------------------------------------------------------------------------

class ColorSpaceConfig {
public:
	typedef enum {
		NOP_COLORSPACE,
		LUXCORE_COLORSPACE,
		OPENCOLORIO_COLORSPACE
	} ColorSpaceType;

	// NOP_COLORSPACE constructor
	ColorSpaceConfig();
	// LUXCORE_COLORSPACE constructor
	ColorSpaceConfig(const float gamma);
	// OPENCOLORIO_COLORSPACE constructor
	ColorSpaceConfig(const std::string &configName, const std::string &colorSpaceName);
	// From properties constructor
	ColorSpaceConfig(const luxrays::Properties &props, const std::string &prefix,
			const ColorSpaceConfig &defaultCfg);

	~ColorSpaceConfig();

	static void FromProperties(const luxrays::Properties &props, const std::string &prefix,
			ColorSpaceConfig &colorSpaceCfg, const ColorSpaceConfig &defaultCfg);
	static ColorSpaceType String2ColorSpaceType(const std::string &type);
	static std::string ColorSpaceType2String(const ColorSpaceConfig::ColorSpaceType type);

	static const ColorSpaceConfig defaultNopConfig;
	static const ColorSpaceConfig defaultLuxCoreConfig;
	static const ColorSpaceConfig defaultOpenColorIOConfig;

	ColorSpaceType colorSpaceType;

	// LUXCORE_COLORSPACE data
	struct {
		float gamma;
	} luxcore;

	// OPENCOLORIO_COLORSPACE data
	struct {
		std::string configName;
		std::string colorSpaceName;
	} ocio;
};

}

#endif // _LUXRAYS_COLORSPACE_H
