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

#ifndef _SLG_COLORSPACECONVERTERS_H
#define	_SLG_COLORSPACECONVERTERS_H

#include <string>
#include <boost/unordered_map.hpp>

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;

#include "luxrays/core/color/color.h"
#include "slg/slg.h"
#include "slg/core/colorspace.h"

namespace slg {

//------------------------------------------------------------------------------
// ColorSpaceConverters
//------------------------------------------------------------------------------

class ColorSpaceConverters {
public:
	ColorSpaceConverters();
	virtual ~ColorSpaceConverters();

	void ConvertFrom(const ColorSpaceConfig &cfg, float &v);
	void ConvertFrom(const ColorSpaceConfig &cfg, luxrays::Spectrum &c);

private:
	void ConvertFromLuxCore(const float gamma, float &v);
	void ConvertFromLuxCore(const float gamma, luxrays::Spectrum &c);

	void ConvertFromOpenColorIO(const std::string &configFileName,
		const std::string &inputColorSpace, float &v);
	void ConvertFromOpenColorIO(const std::string &configFileName,
		const std::string &inputColorSpace, luxrays::Spectrum &c);

	boost::unordered_map<std::string, OCIO::ConstCPUProcessorRcPtr> ocioProcessorCache;
};

}

#endif	/* _SLG_COLORSPACECONVERTERS_H */
