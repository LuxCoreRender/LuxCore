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
#include "slg/film/film.h"

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

void ImagePipeline::Apply(Film &film) {
	const double t1 = WallClockTime();

#if !defined(LUXRAYS_DISABLE_OPENCL)
	bool imageInCPURam = true;
	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline) {
		//const double p1 = WallClockTime();

		const bool useOpenCLApply = film.oclEnable && plugin->CanUseOpenCL();

		if (useOpenCLApply) {
			if (imageInCPURam) {
				// Transfer the buffer to OpenCL device ram
				//SLG_LOG("Transferring RGB_TONEMAPPED buffer to OpenCL device");
				film.WriteOCLBuffer_RGB_TONEMAPPED();
			} else {
				// The buffer is already in the OpenCL device ram
			}
		} else {
			if (!imageInCPURam) {
				// Transfer the buffer from OpenCL device ram
				//SLG_LOG("Transferring RGB_TONEMAPPED buffer from OpenCL device");
				film.ReadOCLBuffer_RGB_TONEMAPPED();
				film.oclIntersectionDevice->GetOpenCLQueue().finish();
			} else {
				// The buffer is already in CPU ram
			}
		}

		if (useOpenCLApply) {
			plugin->ApplyOCL(film);
			imageInCPURam = false;
		} else {
			plugin->Apply(film);
			imageInCPURam = true;			
		}
		
		//const double p2 = WallClockTime();
		//SLG_LOG("ImagePipeline plugin time: " << int((p2 - p1) * 1000.0) << "ms");
	}

	if (!imageInCPURam)
		film.ReadOCLBuffer_RGB_TONEMAPPED();
	if (film.oclEnable && canUseOpenCL)
		film.oclIntersectionDevice->GetOpenCLQueue().finish();

#else
	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline) {
		//const double p1 = WallClockTime();

		plugin->Apply(film);
		
		//const double p2 = WallClockTime();
		//SLG_LOG("ImagePipeline plugin time: " << int((p2 - p1) * 1000.0) << "ms");
	}
#endif

	const double t2 = WallClockTime();
	SLG_LOG("ImagePipeline time: " << int((t2 - t1) * 1000.0) << "ms");
}

const ImagePipelinePlugin *ImagePipeline::GetPlugin(const std::type_info &type) const {
	BOOST_FOREACH(const ImagePipelinePlugin *plugin, pipeline) {
		if (typeid(*plugin) == type)
			return plugin;
	}

	return NULL;
}
