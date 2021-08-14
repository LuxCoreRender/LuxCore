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
#include "slg/utils/filenameresolver.h"

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
	
	// LOOK_CONVERSION
	ociotm->lookInputColorSpace = lookInputColorSpace;
	ociotm->lookName = lookName;
	
	return ociotm;
}

OpenColorIOToneMap *OpenColorIOToneMap::CreateColorSpaceConversion(const string &configFileName,
		const string &inputColorSpace, const string &outputColorSpace) {
	OpenColorIOToneMap *ociotm = new OpenColorIOToneMap();
	
	ociotm->conversionType = COLORSPACE_CONVERSION;
	
	ociotm->configFileName = configFileName;
	ociotm->inputColorSpace = inputColorSpace;
	ociotm->outputColorSpace = outputColorSpace;
	
	return ociotm;
}

OpenColorIOToneMap *OpenColorIOToneMap::CreateLUTConversion(const string &lutFileName) {
	OpenColorIOToneMap *ociotm = new OpenColorIOToneMap();
	
	ociotm->conversionType = LUT_CONVERSION;
	
	ociotm->lutFileName = lutFileName;
	
	return ociotm;
}

OpenColorIOToneMap *OpenColorIOToneMap::CreateDisplayConversion(const string &configFileName,
		const string &inputColorSpace, const string &displayName,
		const string &viewName) {
	OpenColorIOToneMap *ociotm = new OpenColorIOToneMap();
	
	ociotm->conversionType = DISPLAY_CONVERSION;
	
	ociotm->configFileName = configFileName;
	ociotm->inputColorSpace = inputColorSpace;
	ociotm->displayName = displayName;
	ociotm->viewName = viewName;
	
	return ociotm;
}

OpenColorIOToneMap *OpenColorIOToneMap::CreateLookConversion(const string &configFileName,
		const string &lookInputColorSpace, const string &lookName) {
	OpenColorIOToneMap *ociotm = new OpenColorIOToneMap();
	
	ociotm->conversionType = LOOK_CONVERSION;
	
	ociotm->configFileName = configFileName;
	ociotm->lookInputColorSpace = lookInputColorSpace;
	ociotm->lookName = lookName;
	
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
					OCIO::Config::CreateFromFile(SLG_FileNameResolver.ResolveFile(configFileName).c_str());

				OCIO::ConstProcessorRcPtr processor = config->getProcessor(inputColorSpace.c_str(), outputColorSpace.c_str());

				OCIO::ConstCPUProcessorRcPtr cpu = processor->getDefaultCPUProcessor();

				// Apply the color transform with OpenColorIO
				OCIO::PackedImageDesc img(pixels, film.GetWidth(), film.GetHeight(), 3);
				cpu->apply(img);
				break;
			}
			case LUT_CONVERSION: {
				OCIO::ConstConfigRcPtr config = OCIO::Config::CreateRaw();

				OCIO::FileTransformRcPtr transform = OCIO::FileTransform::Create();
				transform->setSrc(lutFileName.c_str());
				transform->setInterpolation(OCIO::INTERP_BEST);
				OCIO::ConstProcessorRcPtr processor = config->getProcessor(transform);

				OCIO::ConstCPUProcessorRcPtr cpu = processor->getDefaultCPUProcessor();

				// Apply the color transform with OpenColorIO
				OCIO::PackedImageDesc img(pixels, film.GetWidth(), film.GetHeight(), 3);
				cpu->apply(img);
				break;
			}
			case DISPLAY_CONVERSION: {
				OCIO::ConstConfigRcPtr config = (configFileName == "") ?
					OCIO::GetCurrentConfig() :
					OCIO::Config::CreateFromFile(configFileName.c_str());

				OCIO::DisplayViewTransformRcPtr transform = OCIO::DisplayViewTransform::Create();
				transform->setSrc(inputColorSpace.c_str());
				transform->setDisplay(displayName.c_str());
				transform->setView(viewName.c_str());
				OCIO::ConstProcessorRcPtr processor = config->getProcessor(transform);

				OCIO::ConstCPUProcessorRcPtr cpu = processor->getDefaultCPUProcessor();

				// Apply the color transform with OpenColorIO
				OCIO::PackedImageDesc img(pixels, film.GetWidth(), film.GetHeight(), 3);
				cpu->apply(img);
				break;
			}
			case LOOK_CONVERSION: {
				OCIO::ConstConfigRcPtr config = (configFileName == "") ?
					OCIO::GetCurrentConfig() :
					OCIO::Config::CreateFromFile(SLG_FileNameResolver.ResolveFile(configFileName).c_str());

				const char *lookOutputColorSpace = OCIO::LookTransform::GetLooksResultColorSpace(config,
						config->getCurrentContext(), lookName.c_str());
				if (lookOutputColorSpace && lookOutputColorSpace[0] != 0) {
					OCIO::LookTransformRcPtr transform = OCIO::LookTransform::Create();
					transform->setSrc(lookInputColorSpace.c_str());
					transform->setDst(lookOutputColorSpace);
					transform->setLooks(lookName.c_str());

					OCIO::ConstProcessorRcPtr processor = config->getProcessor(transform);

					OCIO::ConstCPUProcessorRcPtr cpu = processor->getDefaultCPUProcessor();

					// Apply the color transform with OpenColorIO
					OCIO::PackedImageDesc img(pixels, film.GetWidth(), film.GetHeight(), 3);
					cpu->apply(img);
				} else
					throw runtime_error("Unknown look destination color space in OpenColorIOToneMap::Apply()");
				break;
			}
			default:
				throw runtime_error("Unknown mode in OpenColorIOToneMap::Apply(): " + ToString(conversionType));
		}
	} catch (OCIO::Exception &exception) {
		throw runtime_error("OpenColorIO Error in OpenColorIOToneMap::Apply(): " + string(exception.what()));
	}
}
