/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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
#include <boost/serialization/export.hpp>

#include "slg/film/imagepipeline/imagepipeline.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImagePipeline
//------------------------------------------------------------------------------

ImagePipeline::ImagePipeline() {
	canUseOpenCL = false;
}

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

	canUseOpenCL |= plugin->CanUseOpenCL();
}

void ImagePipeline::Apply(const Film &film, luxrays::Spectrum *pixels) const {
	//const double t1 = WallClockTime();

	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline) {
		plugin->Apply(film, pixels);
	}

	//const double t2 = WallClockTime();
	//SLG_LOG("ImagePipeline time: " << int((t2 - t1) * 1000.0) << "ms");
}

const ImagePipelinePlugin *ImagePipeline::GetPlugin(const std::type_info &type) const {
	BOOST_FOREACH(const ImagePipelinePlugin *plugin, pipeline) {
		if (typeid(*plugin) == type)
			return plugin;
	}

	return NULL;
}
