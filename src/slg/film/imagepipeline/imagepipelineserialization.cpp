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

#include "slg/film/imagepipeline/imagepipeline.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImagePipelinePlugin serialization
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImagePipelinePlugin)

template<class Archive> void ImagePipelinePlugin::serialize(Archive &ar, const u_int version) {
}

namespace slg {
// Explicit instantiations for portable archives
template void ImagePipelinePlugin::serialize(LuxOutputBinArchive &ar, const u_int version);
template void ImagePipelinePlugin::serialize(LuxInputBinArchive &ar, const u_int version);
template void ImagePipelinePlugin::serialize(LuxOutputTextArchive &ar, const u_int version);
template void ImagePipelinePlugin::serialize(LuxInputTextArchive &ar, const u_int version);
}

//------------------------------------------------------------------------------
// ImagePipeline serialization
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImagePipeline)

template<class Archive> void ImagePipeline::serialize(Archive &ar, const u_int version) {
	ar & radianceChannelScales;

	ar & pipeline;
	ar & canUseHW;
}

namespace slg {
// Explicit instantiations for portable archives
template void ImagePipeline::serialize(LuxOutputBinArchive &ar, const u_int version);
template void ImagePipeline::serialize(LuxInputBinArchive &ar, const u_int version);
template void ImagePipeline::serialize(LuxOutputTextArchive &ar, const u_int version);
template void ImagePipeline::serialize(LuxInputTextArchive &ar, const u_int version);
}
