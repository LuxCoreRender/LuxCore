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
// Linear tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::OpenColorIOToneMap)

OpenColorIOToneMap::OpenColorIOToneMap() {
}

OpenColorIOToneMap::OpenColorIOToneMap(const std::string &ics, const std::string &ocs) {
	inputColorSpace = ics;
	outputColorSpace = ocs;
}

OpenColorIOToneMap::~OpenColorIOToneMap() {
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void OpenColorIOToneMap::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

	try {
		OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
		OCIO::ConstProcessorRcPtr processor = config->getProcessor(inputColorSpace.c_str(), outputColorSpace.c_str());

		OCIO::ConstCPUProcessorRcPtr cpu = processor->getDefaultCPUProcessor();

		// Apply the color transform with OpenColorIO
		OCIO::PackedImageDesc img(pixels, film.GetWidth(), film.GetHeight(), 3);
		cpu->apply(img);
	} catch (OCIO::Exception & exception) {
		SLG_LOG("OpenColorIO Error in OpenColorIOToneMap::Apply(): " << exception.what());
	}
}
