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

#include "slg/film/film.h"
#include "slg/film/filters/filter.h"

#include "slg/film/imagepipeline/plugins/tonemaps/autolinear.h"
#include "slg/film/imagepipeline/plugins/tonemaps/linear.h"
#include "slg/film/imagepipeline/plugins/tonemaps/luxlinear.h"
#include "slg/film/imagepipeline/plugins/tonemaps/reinhard02.h"

#include "slg/film/imagepipeline/plugins/cameraresponse.h"
#include "slg/film/imagepipeline/plugins/contourlines.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/gaussianblur3x3.h"
#include "slg/film/imagepipeline/plugins/nop.h"
#include "slg/film/imagepipeline/plugins/outputswitcher.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SetRadianceGroupsScale
//------------------------------------------------------------------------------

void Film::SetRadianceGroupsScale(const Properties &props) {
	set<string> radianceScaleIndices;
	vector<string> radianceScaleKeys = props.GetAllNames("film.radiancescales.");
	for (vector<string>::const_iterator radianceScaleKey = radianceScaleKeys.begin(); radianceScaleKey != radianceScaleKeys.end(); ++radianceScaleKey) {
		const string &key = *radianceScaleKey;
		const size_t dot1 = key.find(".", string("film.radiancescales.").length());
		if (dot1 == string::npos)
			continue;

		// Extract the radiance channel scale index
		const string indexStr = Property::ExtractField(key, 2);

		if (indexStr == "")
			throw runtime_error("Syntax error in film radiance scale index definition: " + indexStr);

		if (radianceScaleIndices.count(indexStr) > 0)
			continue;
		
		radianceScaleIndices.insert(indexStr);
		const u_int index = boost::lexical_cast<u_int>(indexStr);
		
		Film::RadianceChannelScale radianceChannelScale;
		radianceChannelScale.globalScale = props.Get(Property("film.radiancescales." + indexStr + ".globalscale")(1.f)).Get<float>();
		radianceChannelScale.temperature = props.Get(Property("film.radiancescales." + indexStr + ".temperature")(0.f)).Get<float>();
		radianceChannelScale.rgbScale = props.Get(Property("film.radiancescales." + indexStr + ".rgbscale")(1.f, 1.f, 1.f)).Get<Spectrum>();
		radianceChannelScale.enabled = props.Get(Property("film.radiancescales." + indexStr + ".enabled")(true)).Get<bool>();

		SetRadianceChannelScale(index, radianceChannelScale);
	}
}

//------------------------------------------------------------------------------
// AllocImagePipeline
//------------------------------------------------------------------------------

ImagePipeline *Film::AllocImagePipeline(const Properties &props) {
	//--------------------------------------------------------------------------
	// Create the image pipeline
	//--------------------------------------------------------------------------

	auto_ptr<ImagePipeline> imagePipeline(new ImagePipeline());
	vector<string> imagePipelineKeys = props.GetAllUniqueSubNames("film.imagepipeline");
	if (imagePipelineKeys.size() > 0) {
		// Sort the entries
		sort(imagePipelineKeys.begin(), imagePipelineKeys.end());

		for (vector<string>::const_iterator imagePipelineKey = imagePipelineKeys.begin(); imagePipelineKey != imagePipelineKeys.end(); ++imagePipelineKey) {
			// Extract the plugin priority name
			const string pluginPriority = Property::ExtractField(*imagePipelineKey, 2);
			if (pluginPriority == "")
				throw runtime_error("Syntax error in image pipeline plugin definition: " + *imagePipelineKey);
			const string prefix = "film.imagepipeline." + pluginPriority;

			const string type = props.Get(Property(prefix + ".type")("")).Get<string>();
			if (type == "")
				throw runtime_error("Syntax error in " + prefix + ".type");

			if (type == "TONEMAP_LINEAR") {
				imagePipeline->AddPlugin(new LinearToneMap(
					props.Get(Property(prefix + ".scale")(1.f)).Get<float>()));
			} else if (type == "TONEMAP_REINHARD02") {
				imagePipeline->AddPlugin(new Reinhard02ToneMap(
					props.Get(Property(prefix + ".prescale")(1.f)).Get<float>(),
					props.Get(Property(prefix + ".postscale")(1.2f)).Get<float>(),
					props.Get(Property(prefix + ".burn")(3.75f)).Get<float>()));
			} else if (type == "TONEMAP_AUTOLINEAR") {
				imagePipeline->AddPlugin(new AutoLinearToneMap());
			} else if (type == "TONEMAP_LUXLINEAR") {
				imagePipeline->AddPlugin(new LuxLinearToneMap(
					props.Get(Property(prefix + ".sensitivity")(100.f)).Get<float>(),
					props.Get(Property(prefix + ".exposure")(1.f / 1000.f)).Get<float>(),
					props.Get(Property(prefix + ".fstop")(2.8f)).Get<float>()));
			} else if (type == "NOP") {
				imagePipeline->AddPlugin(new NopPlugin());
			} else if (type == "GAMMA_CORRECTION") {
				imagePipeline->AddPlugin(new GammaCorrectionPlugin(
					props.Get(Property(prefix + ".value")(2.2f)).Get<float>(),
					// 4096 => 12bit resolution
					props.Get(Property(prefix + ".table.size")(4096u)).Get<u_int>()));
			} else if (type == "OUTPUT_SWITCHER") {
				imagePipeline->AddPlugin(new OutputSwitcherPlugin(
					Film::String2FilmChannelType(props.Get(Property(prefix + ".channel")("DEPTH")).Get<string>()),
					props.Get(Property(prefix + ".index")(0u)).Get<u_int>()));
			} else if (type == "GAUSSIANFILTER_3x3") {
				imagePipeline->AddPlugin(new GaussianBlur3x3FilterPlugin(
					props.Get(Property(prefix + ".weight")(.15f)).Get<float>()));
			} else if (type == "CAMERA_RESPONSE_FUNC") {
				imagePipeline->AddPlugin(new CameraResponsePlugin(
					props.Get(Property(prefix + ".name")("Advantix_100CD")).Get<string>()));
			} else if (type == "CONTOUR_LINES") {
				const float scale = props.Get(Property(prefix + ".scale")(179.f)).Get<float>();
				const float range = Max(0.f, props.Get(Property(prefix + ".range")(100.f)).Get<float>());
				const u_int steps = Max(2u, props.Get(Property(prefix + ".steps")(8)).Get<u_int>());
				const int zeroGridSize = props.Get(Property(prefix + ".zerogridsize")(8)).Get<int>();
				imagePipeline->AddPlugin(new ContourLinesPlugin(scale, range, steps, zeroGridSize));
			} else
				throw runtime_error("Unknown image pipeline plugin type: " + type);
		}
	} else {
		// The definition of image pipeline is missing, use the default
		imagePipeline->AddPlugin(new AutoLinearToneMap());
		imagePipeline->AddPlugin(new GammaCorrectionPlugin(2.2f));
	}

	if (props.IsDefined("film.gamma"))
		SLG_LOG("WARNING: ignored deprecated property film.gamma");
	
	return imagePipeline.release();
}

//------------------------------------------------------------------------------
// Film parser
//------------------------------------------------------------------------------

void Film::Parse(const Properties &props) {
	//--------------------------------------------------------------------------
	// Check if there is a new image pipeline definition
	//--------------------------------------------------------------------------

	if (props.HaveNames("film.imagepipeline")) {
		// Create the new image pipeline
		ImagePipeline *imagePipeline = AllocImagePipeline(props);

		// Use the new image pipeline
		SetImagePipeline(imagePipeline);
	}

	//--------------------------------------------------------------------------
	// Check if there are new radiance groups scale
	//--------------------------------------------------------------------------

	if (props.HaveNames("film.radiancescales"))
		SetRadianceGroupsScale(props);
}
