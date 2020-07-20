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

#include <boost/foreach.hpp>
#include <unordered_set>

#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/imagepipeline.h"
#include "slg/film/imagepipeline/radiancechannelscale.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/bcddenoiser.h"
#include "slg/film/film.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImagePipelinePlugin
//------------------------------------------------------------------------------

float ImagePipelinePlugin::GetGammaCorrectionValue(const Film &film, const u_int index) {
	float gamma = 1.f;
	const ImagePipeline *ip = film.GetImagePipeline(index);
	if (ip) {
		const GammaCorrectionPlugin *gcPlugin = (const GammaCorrectionPlugin *)ip->GetPlugin(typeid(GammaCorrectionPlugin));
		if (gcPlugin)
			gamma = gcPlugin->gamma;
	}

	return gamma;
}

u_int ImagePipelinePlugin::GetBCDPipelineIndex(const Film &film) {
	for (u_int i = 0; i < film.GetImagePipelineCount(); ++i) {
		const ImagePipeline *ip = film.GetImagePipeline(i);

		const BCDDenoiserPlugin *bcdPlugin = (const BCDDenoiserPlugin *)ip->GetPlugin(typeid(BCDDenoiserPlugin));
		if (bcdPlugin)
			return i;
	}

	// Something wrong here
	throw runtime_error("Error in ImagePipelinePlugin::GetBCDPipelineIndex(): BCDDenoiserPlugin is not used in any image pipeline");
}

const bcd::HistogramParameters &ImagePipelinePlugin::GetBCDHistogramParameters(const Film &film) {
	for (u_int i = 0; i < film.GetImagePipelineCount(); ++i) {
		const ImagePipeline *ip = film.GetImagePipeline(i);

		const BCDDenoiserPlugin *bcdPlugin = (const BCDDenoiserPlugin *)ip->GetPlugin(typeid(BCDDenoiserPlugin));
		if (bcdPlugin)
			return bcdPlugin->GetHistogramParameters();
	}

	// Something wrong here
	throw runtime_error("Error in ImagePipelinePlugin::GetBCDHistogramParameters(): BCDDenoiserPlugin is not used in any image pipeline");
}

float ImagePipelinePlugin::GetBCDWarmUpSPP(const Film &film) {
	for (u_int i = 0; i < film.GetImagePipelineCount(); ++i) {
		const ImagePipeline *ip = film.GetImagePipeline(i);

		const BCDDenoiserPlugin *bcdPlugin = (const BCDDenoiserPlugin *)ip->GetPlugin(typeid(BCDDenoiserPlugin));
		if (bcdPlugin)
			return bcdPlugin->GetWarmUpSPP();
	}

	// Something wrong here
	throw runtime_error("Error in ImagePipelinePlugin::GetBCDWarmUpSPP(): BCDDenoiserPlugin is not used in any image pipeline");
}

//------------------------------------------------------------------------------
// ImagePipeline
//------------------------------------------------------------------------------

ImagePipeline::ImagePipeline() {
	canUseHW = false;
}

ImagePipeline::~ImagePipeline() {
	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline)
		delete plugin;
}

void ImagePipeline::SetRadianceGroupCount(const u_int radianceGroupCount) {
	radianceChannelScales.resize(radianceGroupCount);
}

void ImagePipeline::SetRadianceChannelScale(const u_int index, const RadianceChannelScale &scale) {
	radianceChannelScales.resize(Max<size_t>(radianceChannelScales.size(), index + 1));

	radianceChannelScales[index] = scale;
	radianceChannelScales[index].Init();
}

void ImagePipeline::AddHWChannelsUsed(unordered_set<Film::FilmChannelType> &hwChannelsUsed) const {
	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline) {
		plugin->AddHWChannelsUsed(hwChannelsUsed);
	}
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

	canUseHW |= plugin->CanUseHW();
}

void ImagePipeline::Apply(Film &film, const u_int index) {
	//const double t1 = WallClockTime();

	bool imageInCPURam = true;
	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline) {
		//const double p1 = WallClockTime();

		const bool useHWApply = film.hwEnable && film.hardwareDevice &&
				plugin->CanUseHW();

		// Check if it is a valid imagepipeline
		if (!useHWApply && !plugin->CanUseNative())
			throw runtime_error("A imagepipeline plugin can only use hardware device but imagepipeline hardware execution is disabled");

		if (useHWApply) {
			if (imageInCPURam) {
				// Transfer the buffer to OpenCL device ram
				//SLG_LOG("Transferring IMAGEPIPELINE buffer to OpenCL device");
				film.WriteHWBuffer_IMAGEPIPELINE(index);
			} else {
				// The buffer is already in the OpenCL device ram
			}
		} else {
			if (!imageInCPURam) {
				// Transfer the buffer from OpenCL device ram
				//SLG_LOG("Transferring IMAGEPIPELINE buffer from OpenCL device");
				film.ReadHWBuffer_IMAGEPIPELINE(index);
				film.hardwareDevice->FinishQueue();
			} else {
				// The buffer is already in CPU ram
			}
		}

		if (useHWApply) {
			plugin->ApplyHW(film, index);
			imageInCPURam = false;
		} else {
			plugin->Apply(film, index);
			imageInCPURam = true;			
		}
		
		//const double p2 = WallClockTime();
		//SLG_LOG("ImagePipeline plugin time: " << int((p2 - p1) * 1000.0) << "ms");
	}

	if (film.hwEnable && film.hardwareDevice && canUseHW) {
		if (!imageInCPURam)
			film.ReadHWBuffer_IMAGEPIPELINE(index);

		film.hardwareDevice->FinishQueue();
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
