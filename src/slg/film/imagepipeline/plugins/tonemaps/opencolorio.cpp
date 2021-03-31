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

#include "luxrays/utils/serializationutils.h"
#include "slg/kernels/kernels.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/tonemaps/opencolorio.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// OpenColorIO tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::OpenColorIOToneMap)

OpenColorIOToneMap::OpenColorIOToneMap() {
}

OpenColorIOToneMap::~OpenColorIOToneMap() {
}

ToneMap *OpenColorIOToneMap::Copy() const {
	OpenColorIOToneMap *ociotm = new OpenColorIOToneMap();
	
	ociotm->conversionType = conversionType;
	
	ociotm->configFileName = configFileName;

	// COLORSPACE_CONVERSION
	ociotm->inputColorSpace = inputColorSpace;
	ociotm->outputColorSpace = outputColorSpace;

	// LUT_CONVERSION
	ociotm->lutFileName = lutFileName;
	
	// DISPLAY_CONVERSION
	ociotm->displayName = displayName;
	ociotm->viewName = viewName;
	
	return ociotm;
}

OpenColorIOToneMap *OpenColorIOToneMap::CreateColorSpaceConversion(const std::string &configFileName,
		const std::string &inputColorSpace, const std::string &outputColorSpace) {
	OpenColorIOToneMap *ociotm = new OpenColorIOToneMap();
	
	ociotm->conversionType = COLORSPACE_CONVERSION;
	
	ociotm->configFileName = configFileName;
	ociotm->inputColorSpace = inputColorSpace;
	ociotm->outputColorSpace = outputColorSpace;
	
	return ociotm;
}

OpenColorIOToneMap *OpenColorIOToneMap::CreateLUTConversion(const std::string &lutFileName) {
	OpenColorIOToneMap *ociotm = new OpenColorIOToneMap();
	
	ociotm->conversionType = LUT_CONVERSION;
	
	ociotm->lutFileName = lutFileName;
	
	return ociotm;
}

OpenColorIOToneMap *OpenColorIOToneMap::CreateDisplayConversion(const std::string &configFileName,
		const std::string &inputColorSpace, const std::string &displayName,
		const std::string &viewName) {
	OpenColorIOToneMap *ociotm = new OpenColorIOToneMap();
	
	ociotm->conversionType = DISPLAY_CONVERSION;
	
	ociotm->configFileName = configFileName;
	ociotm->inputColorSpace = inputColorSpace;
	ociotm->displayName = displayName;
	ociotm->viewName = viewName;
	
	return ociotm;
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void OpenColorIOToneMap::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

	try {
		switch (conversionType) {
			case COLORSPACE_CONVERSION: {
				OCIO::ConstConfigRcPtr config = (configFileName == "") ?
					OCIO::GetCurrentConfig() :
					OCIO::Config::CreateFromFile(configFileName.c_str());
				OCIO::ConstProcessorRcPtr processor = config->getProcessor(inputColorSpace.c_str(), outputColorSpace.c_str());

				OCIO::ConstCPUProcessorRcPtr cpu = processor->getDefaultCPUProcessor();

				// Apply the color transform with OpenColorIO
				OCIO::PackedImageDesc img(pixels, film.GetWidth(), film.GetHeight(), 3);
				cpu->apply(img);
				break;
			}
			default:
				throw runtime_error("Unknown mode in OpenColorIOToneMap::Apply(): " + ToString(conversionType));
		}
	} catch (OCIO::Exception & exception) {
		SLG_LOG("OpenColorIO Error in OpenColorIOToneMap::Apply(): " << exception.what());
	}
}
