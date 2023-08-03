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

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/unordered_set.hpp>
#include <boost/lexical_cast.hpp>

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;

#include "luxrays/utils/fileext.h"
#include "luxrays/utils/thread.h"

#include "slg/core/sdl.h"
#include "slg/film/film.h"
#include "slg/film/filters/filter.h"

#include "slg/film/imagepipeline/plugins/tonemaps/autolinear.h"
#include "slg/film/imagepipeline/plugins/tonemaps/linear.h"
#include "slg/film/imagepipeline/plugins/tonemaps/luxlinear.h"
#include "slg/film/imagepipeline/plugins/tonemaps/reinhard02.h"
#include "slg/film/imagepipeline/plugins/tonemaps/opencolorio.h"

#include "slg/film/imagepipeline/plugins/cameraresponse.h"
#include "slg/film/imagepipeline/plugins/contourlines.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/gaussianblur3x3.h"
#include "slg/film/imagepipeline/plugins/nop.h"
#include "slg/film/imagepipeline/plugins/patterns.h"
#include "slg/film/imagepipeline/plugins/outputswitcher.h"
#include "slg/film/imagepipeline/plugins/backgroundimg.h"
#include "slg/film/imagepipeline/plugins/bcddenoiser.h"
#include "slg/film/imagepipeline/plugins/bloom.h"
#include "slg/film/imagepipeline/plugins/objectidmask.h"
#include "slg/film/imagepipeline/plugins/vignetting.h"
#include "slg/film/imagepipeline/plugins/coloraberration.h"
#include "slg/film/imagepipeline/plugins/premultiplyalpha.h"
#include "slg/film/imagepipeline/plugins/mist.h"
#include "slg/film/imagepipeline/plugins/intel_oidn.h"
#include "slg/film/imagepipeline/plugins/whitebalance.h"
#include "slg/film/imagepipeline/plugins/bakemapmargin.h"
#include "slg/film/imagepipeline/plugins/colorlut.h"
#include "slg/film/imagepipeline/plugins/optixdenoiser.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ParseOutputs
//------------------------------------------------------------------------------

void Film::ParseOutputs(const Properties &props) {
	// Initialize the FilmOutputs

	filmOutputs.Reset();

	const bool safeSave = props.Get(Property("film.outputs.safesave")(true)).Get<bool>();
	filmOutputs.SetSafeSave(safeSave);
	
	boost::unordered_set<string> outputNames;
	vector<string> outputKeys = props.GetAllNames("film.outputs.");
	for (vector<string>::const_iterator outputKey = outputKeys.begin(); outputKey != outputKeys.end(); ++outputKey) {
		const string &key = *outputKey;
		const size_t dot1 = key.find(".", string("film.outputs.").length());
		if (dot1 == string::npos)
			continue;

		// Extract the output type name
		const string outputName = Property::ExtractField(key, 2);
		if (outputName == "")
			throw runtime_error("Syntax error in film output definition: " + outputName);

		if (outputNames.count(outputName) > 0)
			continue;

		outputNames.insert(outputName);
		const string type = props.Get(Property("film.outputs." + outputName + ".type")("RGB_IMAGEPIPELINE")).Get<string>();
		const string fileName = props.Get(Property("film.outputs." + outputName + ".filename")("image.png")).Get<string>();

		SDL_LOG("Film output definition: " << type << " [" << fileName << "]");

//		// Check if it is a supported file format
//		FREE_IMAGE_FORMAT fif = FREEIMAGE_GETFIFFROMFILENAME(FREEIMAGE_CONVFILENAME(fileName).c_str());
//		if (fif == FIF_UNKNOWN)
//			throw runtime_error("Unknown image format in film output: " + outputName);

		// HDR image or not
		bool hdrImage = false;
        const string fileExtension = GetFileNameExt(fileName);
		if (fileExtension == ".exr" || fileExtension == ".hdr")
			hdrImage = true;

		switch (FilmOutputs::String2FilmOutputType(type)) {
			case FilmOutputs::RGB: {
				if (hdrImage)
					filmOutputs.Add(FilmOutputs::RGB, fileName);
				else
					throw runtime_error("Not tonemapped image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::RGBA: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::ALPHA);
					filmOutputs.Add(FilmOutputs::RGBA, fileName);
				} else
					throw runtime_error("Not tonemapped image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::RGB_IMAGEPIPELINE: {
				const u_int imagePipelineIndex = props.Get(Property("film.outputs." + outputName + ".index")(0)).Get<u_int>();
				Properties prop;
				prop.Set(Property("index")(imagePipelineIndex));

				filmOutputs.Add(FilmOutputs::RGB_IMAGEPIPELINE, fileName, &prop);
				break;
			}
			case FilmOutputs::RGBA_IMAGEPIPELINE: {
				const u_int imagePipelineIndex = props.Get(Property("film.outputs." + outputName + ".index")(0)).Get<u_int>();
				Properties prop;
				prop.Set(Property("index")(imagePipelineIndex));

				if (!initialized)
					AddChannel(Film::ALPHA);
				filmOutputs.Add(FilmOutputs::RGBA_IMAGEPIPELINE, fileName, &prop);
				break;
			}
			case FilmOutputs::ALPHA: {
				if (!initialized)
					AddChannel(Film::ALPHA);
				filmOutputs.Add(FilmOutputs::ALPHA, fileName);
				break;
			}
			case FilmOutputs::DEPTH: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::DEPTH);
					filmOutputs.Add(FilmOutputs::DEPTH, fileName);
				} else
					throw runtime_error("Depth image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::POSITION: {
				if (hdrImage) {
					if (!initialized) {
						AddChannel(Film::DEPTH);
						AddChannel(Film::POSITION);
					}
					filmOutputs.Add(FilmOutputs::POSITION, fileName);
				} else
					throw runtime_error("Position image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::GEOMETRY_NORMAL: {
				if (hdrImage) {
					if (!initialized) {
						AddChannel(Film::DEPTH);
						AddChannel(Film::GEOMETRY_NORMAL);
					}
					filmOutputs.Add(FilmOutputs::GEOMETRY_NORMAL, fileName);
				} else
					throw runtime_error("Geometry normal image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::SHADING_NORMAL: {
				if (hdrImage) {
					if (!initialized) {
						AddChannel(Film::DEPTH);
						AddChannel(Film::SHADING_NORMAL);
					}
					filmOutputs.Add(FilmOutputs::SHADING_NORMAL, fileName);
				} else
					throw runtime_error("Shading normal image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::MATERIAL_ID: {
				if (!hdrImage) {
					if (!initialized) {
						AddChannel(Film::DEPTH);
						AddChannel(Film::MATERIAL_ID);
					}
					filmOutputs.Add(FilmOutputs::MATERIAL_ID, fileName);
				} else
					throw runtime_error("Material ID image can be saved only in non HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::DIRECT_DIFFUSE: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::DIRECT_DIFFUSE);
					filmOutputs.Add(FilmOutputs::DIRECT_DIFFUSE, fileName);
				} else
					throw runtime_error("Direct diffuse image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::DIRECT_DIFFUSE_REFLECT: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::DIRECT_DIFFUSE_REFLECT);
					filmOutputs.Add(FilmOutputs::DIRECT_DIFFUSE_REFLECT, fileName);
				} else
					throw runtime_error("Direct diffuse reflect image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::DIRECT_DIFFUSE_TRANSMIT: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::DIRECT_DIFFUSE_TRANSMIT);
					filmOutputs.Add(FilmOutputs::DIRECT_DIFFUSE_TRANSMIT, fileName);
				} else
					throw runtime_error("Direct diffuse transmit image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::DIRECT_GLOSSY: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::DIRECT_GLOSSY);
					filmOutputs.Add(FilmOutputs::DIRECT_GLOSSY, fileName);
				} else
					throw runtime_error("Direct glossy image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::DIRECT_GLOSSY_REFLECT: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::DIRECT_GLOSSY_REFLECT);
					filmOutputs.Add(FilmOutputs::DIRECT_GLOSSY_REFLECT, fileName);
				} else
					throw runtime_error("Direct glossy reflect image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::DIRECT_GLOSSY_TRANSMIT: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::DIRECT_GLOSSY_TRANSMIT);
					filmOutputs.Add(FilmOutputs::DIRECT_GLOSSY_TRANSMIT, fileName);
				} else
					throw runtime_error("Direct glossy transmit image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::EMISSION: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::EMISSION);
					filmOutputs.Add(FilmOutputs::EMISSION, fileName);
				} else
					throw runtime_error("Emission image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::INDIRECT_DIFFUSE: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::INDIRECT_DIFFUSE);
					filmOutputs.Add(FilmOutputs::INDIRECT_DIFFUSE, fileName);
				} else
					throw runtime_error("Indirect diffuse image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::INDIRECT_DIFFUSE_REFLECT: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::INDIRECT_DIFFUSE_REFLECT);
					filmOutputs.Add(FilmOutputs::INDIRECT_DIFFUSE_REFLECT, fileName);
				} else
					throw runtime_error("Indirect diffuse reflect image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::INDIRECT_DIFFUSE_TRANSMIT: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::INDIRECT_DIFFUSE_TRANSMIT);
					filmOutputs.Add(FilmOutputs::INDIRECT_DIFFUSE_TRANSMIT, fileName);
				} else
					throw runtime_error("Indirect diffuse transmit image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::INDIRECT_GLOSSY: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::INDIRECT_GLOSSY);
					filmOutputs.Add(FilmOutputs::INDIRECT_GLOSSY, fileName);
				} else
					throw runtime_error("Indirect glossy image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::INDIRECT_GLOSSY_REFLECT: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::INDIRECT_GLOSSY_REFLECT);
					filmOutputs.Add(FilmOutputs::INDIRECT_GLOSSY_REFLECT, fileName);
				} else
					throw runtime_error("Indirect glossy reflect image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::INDIRECT_GLOSSY_TRANSMIT: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::INDIRECT_GLOSSY_TRANSMIT);
					filmOutputs.Add(FilmOutputs::INDIRECT_GLOSSY_TRANSMIT, fileName);
				} else
					throw runtime_error("Indirect glossy transmit image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::INDIRECT_SPECULAR: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::INDIRECT_SPECULAR);
					filmOutputs.Add(FilmOutputs::INDIRECT_SPECULAR, fileName);
				} else
					throw runtime_error("Indirect specular image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::INDIRECT_SPECULAR_REFLECT: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::INDIRECT_SPECULAR_REFLECT);
					filmOutputs.Add(FilmOutputs::INDIRECT_SPECULAR_REFLECT, fileName);
				} else
					throw runtime_error("Indirect specular reflect image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::INDIRECT_SPECULAR_TRANSMIT: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::INDIRECT_SPECULAR_TRANSMIT);
					filmOutputs.Add(FilmOutputs::INDIRECT_SPECULAR_TRANSMIT, fileName);
				} else
					throw runtime_error("Indirect specular transmit image can be saved only in HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::MATERIAL_ID_MASK: {
				const u_int materialID = props.Get(Property("film.outputs." + outputName + ".id")(255)).Get<u_int>();
				Properties prop;
				prop.Set(Property("id")(materialID));

				if (!initialized) {
					AddChannel(Film::MATERIAL_ID);
					AddChannel(Film::MATERIAL_ID_MASK, &prop);
				}
				filmOutputs.Add(FilmOutputs::MATERIAL_ID_MASK, fileName, &prop);
				break;
			}
			case FilmOutputs::DIRECT_SHADOW_MASK: {
				if (!initialized)
					AddChannel(Film::DIRECT_SHADOW_MASK);
				filmOutputs.Add(FilmOutputs::DIRECT_SHADOW_MASK, fileName);
				break;
			}
			case FilmOutputs::INDIRECT_SHADOW_MASK: {
				if (!initialized)
					AddChannel(Film::INDIRECT_SHADOW_MASK);
				filmOutputs.Add(FilmOutputs::INDIRECT_SHADOW_MASK, fileName);
				break;
			}
			case FilmOutputs::RADIANCE_GROUP: {
				const u_int lightID = props.Get(Property("film.outputs." + outputName + ".id")(0)).Get<u_int>();
				Properties prop;
				prop.Set(Property("id")(lightID));

				filmOutputs.Add(FilmOutputs::RADIANCE_GROUP, fileName, &prop);
				break;
			}
			case FilmOutputs::UV: {
				if (!initialized) {
					AddChannel(Film::DEPTH);
					AddChannel(Film::UV);
				}
				filmOutputs.Add(FilmOutputs::UV, fileName);
				break;
			}
			case FilmOutputs::RAYCOUNT: {
				if (!initialized)
					AddChannel(Film::RAYCOUNT);
				filmOutputs.Add(FilmOutputs::RAYCOUNT, fileName);
				break;
			}
			case FilmOutputs::BY_MATERIAL_ID: {
				const u_int materialID = props.Get(Property("film.outputs." + outputName + ".id")(255)).Get<u_int>();
				Properties prop;
				prop.Set(Property("id")(materialID));

				if (!initialized) {
					AddChannel(Film::MATERIAL_ID);
					AddChannel(Film::BY_MATERIAL_ID, &prop);
				}
				filmOutputs.Add(FilmOutputs::BY_MATERIAL_ID, fileName, &prop);
				break;
			}
			case FilmOutputs::IRRADIANCE: {
				if (!initialized)
					AddChannel(Film::IRRADIANCE);
				filmOutputs.Add(FilmOutputs::IRRADIANCE, fileName);
				break;
			}
			case FilmOutputs::OBJECT_ID: {
				if (!hdrImage) {
					if (!initialized) {
						AddChannel(Film::DEPTH);
						AddChannel(Film::OBJECT_ID);
					}
					filmOutputs.Add(FilmOutputs::OBJECT_ID, fileName);
				} else
					throw runtime_error("Object ID image can be saved only in non HDR formats: " + outputName);
				break;
			}
			case FilmOutputs::OBJECT_ID_MASK: {
				const u_int materialID = props.Get(Property("film.outputs." + outputName + ".id")(255)).Get<u_int>();
				Properties prop;
				prop.Set(Property("id")(materialID));

				if (!initialized) {
					AddChannel(Film::OBJECT_ID);
					AddChannel(Film::OBJECT_ID_MASK, &prop);
				}
				filmOutputs.Add(FilmOutputs::OBJECT_ID_MASK, fileName, &prop);
				break;
			}
			case FilmOutputs::BY_OBJECT_ID: {
				const u_int materialID = props.Get(Property("film.outputs." + outputName + ".id")(255)).Get<u_int>();
				Properties prop;
				prop.Set(Property("id")(materialID));

				if (!initialized) {
					AddChannel(Film::OBJECT_ID);
					AddChannel(Film::BY_OBJECT_ID, &prop);
				}
				filmOutputs.Add(FilmOutputs::BY_OBJECT_ID, fileName, &prop);
				break;
			}
			case FilmOutputs::SAMPLECOUNT: {
				if (!initialized)
					AddChannel(Film::SAMPLECOUNT);
				filmOutputs.Add(FilmOutputs::SAMPLECOUNT, fileName);
				break;
			}
			case FilmOutputs::CONVERGENCE: {
				if (!initialized)
					AddChannel(Film::CONVERGENCE);
				filmOutputs.Add(FilmOutputs::CONVERGENCE, fileName);
				break;
			}
			case FilmOutputs::SERIALIZED_FILM: {
				filmOutputs.Add(FilmOutputs::SERIALIZED_FILM, fileName);
				break;
			}
			case FilmOutputs::MATERIAL_ID_COLOR: {
				if (!initialized)
					AddChannel(Film::MATERIAL_ID_COLOR);
				filmOutputs.Add(FilmOutputs::MATERIAL_ID_COLOR, fileName);
				break;
			}
			case FilmOutputs::ALBEDO: {
				if (!initialized)
					AddChannel(Film::ALBEDO);
				filmOutputs.Add(FilmOutputs::ALBEDO, fileName);
				break;
			}
			case FilmOutputs::AVG_SHADING_NORMAL: {
				if (!initialized)
					AddChannel(Film::AVG_SHADING_NORMAL);
				filmOutputs.Add(FilmOutputs::AVG_SHADING_NORMAL, fileName);
				break;
			}
			case FilmOutputs::NOISE: {
				if (!initialized)
					AddChannel(Film::NOISE);
				filmOutputs.Add(FilmOutputs::NOISE, fileName);
				break;
			}
			case FilmOutputs::USER_IMPORTANCE: {
				if (!initialized)
					AddChannel(Film::USER_IMPORTANCE);
				filmOutputs.Add(FilmOutputs::USER_IMPORTANCE, fileName);				
				break;
			}
			case FilmOutputs::CAUSTIC: {
				if (hdrImage)
					filmOutputs.Add(FilmOutputs::CAUSTIC, fileName);
				else
					throw runtime_error("Caustic image can be saved only in HDR formats: " + outputName);
				break;
			}
			default:
				throw runtime_error("Unknown type in film output: " + type);
		}
	}

	// For compatibility with the past
	if (props.IsDefined("image.filename")) {
		SLG_LOG("WARNING: deprecated property image.filename");
		filmOutputs.Add(HasChannel(Film::ALPHA) ? FilmOutputs::RGBA_IMAGEPIPELINE : FilmOutputs::RGB_IMAGEPIPELINE,
				props.Get(Property("image.filename")("image.png")).Get<string>());
	}
}
		
//------------------------------------------------------------------------------
// ParseRadianceGroupsScale(s)
//------------------------------------------------------------------------------

void Film::ParseRadianceGroupsScale(const Properties &props, const u_int imagePipelineIndex,
		const string &radianceGroupsScalePrefix) {
	if (imagePipelineIndex > imagePipelines.size())
		throw runtime_error("Image pipeline index out of bound in a radiance group scale definition: " + ToString(imagePipelineIndex));

	const u_int keyFieldCount = Property::CountFields(radianceGroupsScalePrefix);

	boost::unordered_set<string> radianceScaleIndices;
	vector<string> radianceScaleKeys = props.GetAllNames(radianceGroupsScalePrefix);
	for (vector<string>::const_iterator radianceScaleKey = radianceScaleKeys.begin(); radianceScaleKey != radianceScaleKeys.end(); ++radianceScaleKey) {
		const string &key = *radianceScaleKey;
		const size_t dot1 = key.find(".", radianceGroupsScalePrefix.length());
		if (dot1 == string::npos)
			continue;

		// Extract the radiance channel scale index
		const string indexStr = Property::ExtractField(key, keyFieldCount);

		if (indexStr == "")
			throw runtime_error("Syntax error in image pipeline radiance scale index definition: " + indexStr);

		if (radianceScaleIndices.count(indexStr) > 0)
			continue;

		radianceScaleIndices.insert(indexStr);
		const u_int index = boost::lexical_cast<u_int>(indexStr);
		const string prefix = radianceGroupsScalePrefix + "." + indexStr;

		RadianceChannelScale radianceChannelScale;
		radianceChannelScale.globalScale = props.Get(Property(prefix + ".globalscale")(1.f)).Get<float>();
		radianceChannelScale.temperature = props.Get(Property(prefix + ".temperature")(0.f)).Get<float>();
		radianceChannelScale.rgbScale = props.Get(Property(prefix + ".rgbscale")(1.f, 1.f, 1.f)).Get<Spectrum>();
		radianceChannelScale.reverse = props.Get(Property(prefix + ".reverse")(true)).Get<bool>();
		radianceChannelScale.normalize = props.Get(Property(prefix + ".normalize")(false)).Get<bool>();
		radianceChannelScale.enabled = props.Get(Property(prefix + ".enabled")(true)).Get<bool>();

		imagePipelines[imagePipelineIndex]->SetRadianceChannelScale(index, radianceChannelScale);
	}
}

void Film::ParseRadianceGroupsScales(const Properties &props) {
	// Look for the definition of multiple image pipelines
	vector<string> imagePipelineKeys = props.GetAllUniqueSubNames("film.imagepipelines", true);
	if (imagePipelineKeys.size() > 0) {
		for (vector<string>::const_iterator imagePipelineKey = imagePipelineKeys.begin(); imagePipelineKey != imagePipelineKeys.end(); ++imagePipelineKey) {
			// Extract the image pipeline priority name
			const string imagePipelineNumberStr = Property::ExtractField(*imagePipelineKey, 2);
			if (imagePipelineNumberStr == "")
				throw runtime_error("Syntax error in image pipeline radiance scale definition: " + *imagePipelineKey);
			
			u_int imagePipelineNumber;
			try {
				imagePipelineNumber = boost::lexical_cast<u_int>(imagePipelineNumberStr);
			} catch(boost::bad_lexical_cast &) {
				throw runtime_error("Syntax error in image pipeline radiance scale number: " + *imagePipelineKey);
			}

			const string prefix = "film.imagepipelines." + imagePipelineNumberStr + ".radiancescales";

			ParseRadianceGroupsScale(props, imagePipelineNumber, prefix);
		}
	} else {
		// Look for the definition of a single image pipeline
		ParseRadianceGroupsScale(props, 0, "film.imagepipeline.radiancescales");
	}
}

//------------------------------------------------------------------------------
// AllocImagePipeline
//------------------------------------------------------------------------------

ImagePipeline *Film::CreateImagePipeline(const Properties &props, const string &imagePipelinePrefix) {
	//--------------------------------------------------------------------------
	// Create the image pipeline
	//--------------------------------------------------------------------------

	const u_int keyFieldCount = Property::CountFields(imagePipelinePrefix);
	unique_ptr<ImagePipeline> imagePipeline(new ImagePipeline());

	vector<string> imagePipelineKeys = props.GetAllUniqueSubNames(imagePipelinePrefix, true);
	if (imagePipelineKeys.size() > 0) {
		SDL_LOG("Image pipeline: " << imagePipelinePrefix);
		for (vector<string>::const_iterator imagePipelineKey = imagePipelineKeys.begin(); imagePipelineKey != imagePipelineKeys.end(); ++imagePipelineKey) {
			// Extract the plugin priority name
			const string pluginPriority = Property::ExtractField(*imagePipelineKey, keyFieldCount);
			if (pluginPriority == "radiancescales") {
				// Radiance channel scales are parsed elsewhere
				continue;
			}
			if (pluginPriority == "")
				throw runtime_error("Syntax error in image pipeline plugin definition: " + *imagePipelineKey);
			const string prefix = imagePipelinePrefix + "." + pluginPriority;

			const string type = props.Get(Property(prefix + ".type")("")).Get<string>();
			if (type == "")
				throw runtime_error("Syntax error in " + prefix + ".type");

			SDL_LOG("Image pipeline step " << pluginPriority << ": " << type);

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
				const string type = props.Get(Property(prefix + ".channel")("DEPTH")).Get<string>();
				
				if (type == "CAUSTIC") {
					imagePipeline->AddPlugin(new OutputSwitcherPlugin(FilmOutputs::CAUSTIC));
				} else {
					imagePipeline->AddPlugin(new OutputSwitcherPlugin(
						Film::String2FilmChannelType(type),
						props.Get(Property(prefix + ".index")(0u)).Get<u_int>()));
				}
			} else if (type == "GAUSSIANFILTER_3x3") {
				const float weight = Clamp(props.Get(Property(prefix + ".weight")(.15f)).Get<float>(), 0.f, 1.f);

				imagePipeline->AddPlugin(new GaussianBlur3x3FilterPlugin(weight));
			} else if (type == "CAMERA_RESPONSE_FUNC") {
				imagePipeline->AddPlugin(new CameraResponsePlugin(
					props.Get(Property(prefix + ".name")("Advantix_100CD")).Get<string>()));
			} else if (type == "CONTOUR_LINES") {
				const float scale = props.Get(Property(prefix + ".scale")(179.f)).Get<float>();
				const float range = Max(0.f, props.Get(Property(prefix + ".range")(100.f)).Get<float>());
				const u_int steps = Max(2u, props.Get(Property(prefix + ".steps")(8)).Get<u_int>());
				const int zeroGridSize = props.Get(Property(prefix + ".zerogridsize")(8)).Get<int>();

				imagePipeline->AddPlugin(new ContourLinesPlugin(scale, range, steps, zeroGridSize));
			} else if (type == "BACKGROUND_IMG") {
				ImageMap *im = ImageMap::FromProperties(props, prefix);

				imagePipeline->AddPlugin(new BackgroundImgPlugin(im));
			} else if (type == "BLOOM") {
				const float radius = Clamp(props.Get(Property(prefix + ".radius")(.07f)).Get<float>(), 0.f, 1.f);
				const float weight = Clamp(props.Get(Property(prefix + ".weight")(.25f)).Get<float>(), 0.f, 1.f);

				imagePipeline->AddPlugin(new BloomFilterPlugin(radius, weight));
			} else if (type == "OBJECT_ID_MASK") {
				const u_int objectID = props.Get(Property(prefix + ".id")(0)).Get<u_int>();

				imagePipeline->AddPlugin(new ObjectIDMaskFilterPlugin(objectID));
			} else if (type == "VIGNETTING") {
				const float scale = Clamp(props.Get(Property(prefix + ".scale")(.4f)).Get<float>(), 0.f, 1.f);

				imagePipeline->AddPlugin(new VignettingPlugin(scale));
			} else if (type == "COLOR_ABERRATION") {
				const Property defaultProp = Property(prefix + ".amount")(.005f);
				const Property &prop = props.Get(defaultProp);
				float scaleX, scaleY;
				if (prop.GetSize() == 2) {
					scaleX = Clamp(prop.Get<float>(0), 0.f, 1.f);
					scaleY = Clamp(prop.Get<float>(1), 0.f, 1.f);
				} else {
					scaleX = Clamp(prop.Get<float>(), 0.f, 1.f);
					scaleY = scaleX;
				}

				imagePipeline->AddPlugin(new ColorAberrationPlugin(scaleX, scaleY));
			} else if (type == "PREMULTIPLY_ALPHA") {
				imagePipeline->AddPlugin(new PremultiplyAlphaPlugin());
			} else if (type == "MIST") {
				const Spectrum color = props.Get(Property(prefix + ".color")(1.f, 1.f, 1.f)).Get<Spectrum>();
				const float amount = Clamp(props.Get(Property(prefix + ".amount")(.005f)).Get<float>(), 0.f, 1.f);
				const float start = Clamp(props.Get(Property(prefix + ".startdistance")(0.f)).Get<float>(), 0.f, INFINITY);
				// Make sure end is at least start + a small epsilon to avoid problems
				const float end = Clamp(props.Get(Property(prefix + ".enddistance")(10000.f)).Get<float>(), start + 0.1f, INFINITY);
				const bool excludeBackground = props.Get(Property(prefix + ".excludebackground")(false)).Get<bool>();
			
				imagePipeline->AddPlugin(new MistPlugin(color, amount, start, end, excludeBackground));
			} else if (type == "BCD_DENOISER") {
				const float warmUpSamplesPerPixel = Max(props.Get(Property(prefix + ".warmupspp")(2.f)).Get<float>(), 1.f);
				const float histogramDistanceThreshold = Max(props.Get(Property(prefix + ".histdistthresh")(1.f)).Get<float>(), 0.f);
				const int patchRadius = Max(props.Get(Property(prefix + ".patchradius")(1)).Get<int>(), 1);
				const int searchWindowRadius = Max(props.Get(Property(prefix + ".searchwindowradius")(6)).Get<int>(), 1);
				const float minEigenValue = Max(props.Get(Property(prefix + ".mineigenvalue")(1.e-8f)).Get<float>(), 0.f);
				const bool useRandomPixelOrder = props.Get(Property(prefix + ".userandompixelorder")(true)).Get<bool>();
				const float markedPixelsSkippingProbability = Clamp(props.Get(Property(prefix + ".markedpixelsskippingprobability")(1.f)).Get<float>(), 0.f, 1.f);
				const int userThreadCount = Max(props.Get(Property(prefix + ".threadcount")(0)).Get<int>(), 0);
				const int scales = Max(props.Get(Property(prefix + ".scales")(3)).Get<int>(), 1);
				const bool filterSpikes = props.Get(Property(prefix + ".filterspikes")(false)).Get<bool>();
				const bool applyDenoise = props.Get(Property(prefix + ".applydenoise")(true)).Get<bool>();
				const float prefilterThresholdStDevFactor = props.Get(Property(prefix + ".spikestddev")(2.f)).Get<float>();

				const int threadCount = (userThreadCount > 0) ? userThreadCount : GetHardwareThreadCount();
				
				imagePipeline->AddPlugin(new BCDDenoiserPlugin(
						warmUpSamplesPerPixel,
						histogramDistanceThreshold,
						patchRadius,
						searchWindowRadius,
						minEigenValue,
						useRandomPixelOrder,
						markedPixelsSkippingProbability,
						threadCount,
						scales,
						filterSpikes,
						applyDenoise,
						prefilterThresholdStDevFactor));
			} else if (type == "PATTERNS") {
				const u_int type = props.Get(Property(prefix + ".index")(0)).Get<u_int>();
				imagePipeline->AddPlugin(new PatternsPlugin(type));
#if !defined(LUXCORE_DISABLE_OIDN)
			} else if (type == "INTEL_OIDN") {
				const string filterType = props.Get(Property(prefix + ".filter.type")("RT")).Get<string>();
				const int oidnMemLimit = props.Get(Property(prefix + ".oidnmemory")(6000)).Get<int>();
				const float sharpness = Clamp(props.Get(Property(prefix + ".sharpness")(.1f)).Get<float>(), 0.f, 1.f);
				const bool enablePrefiltering = props.Get(Property(prefix + ".prefilter.enable")(true)).Get<bool>();

				imagePipeline->AddPlugin(new IntelOIDN(filterType, oidnMemLimit, sharpness, enablePrefiltering));
#endif
			} else if (type == "WHITE_BALANCE") {
				const float temperature = Clamp(props.Get(Property(prefix + ".temperature")(6500.f)).Get<float>(), 1000.f, 40000.f);
				const bool reverse = props.Get(Property(prefix + ".reverse")(true)).Get<bool>();
				const bool normalize = props.Get(Property(prefix + ".normalize")(false)).Get<bool>();

				imagePipeline->AddPlugin(new WhiteBalance(temperature, reverse, normalize));
			} else if (type == "BAKEMAP_MARGIN") {
				const u_int marginPixels = Max(props.Get(Property(prefix + ".margin")(2)).Get<u_int>(), 1u);
				const float samplesThreshold = Max(props.Get(Property(prefix + ".samplesthreshold")(0.f)).Get<float>(), 0.f);
				imagePipeline->AddPlugin(new BakeMapMarginPlugin(marginPixels, samplesThreshold));
			} else if (type == "COLOR_LUT") {
				const string fileName = props.Get(Property(prefix + ".file")("lut.cube")).Get<string>();
				const float strength = Clamp(props.Get(Property(prefix + ".strength")(1.f)).Get<float>(), 0.f, 1.f);
				imagePipeline->AddPlugin(new ColorLUTPlugin(fileName, strength));
#if !defined(LUXRAYS_DISABLE_CUDA)
			} else if (type == "OPTIX_DENOISER") {
				const float sharpness = Clamp(props.Get(Property(prefix + ".sharpness")(.1f)).Get<float>(), 0.f, 1.f);
				const u_int minSPP = props.Get(Property(prefix + ".minspp")(0)).Get<u_int>();
				imagePipeline->AddPlugin(new OptixDenoiserPlugin(sharpness, minSPP));
#endif
			} else if (type == "TONEMAP_OPENCOLORIO") {
				const string mode = props.Get(Property(prefix + ".mode")("COLORSPACE_CONVERSION")).Get<string>();

				if (mode == "COLORSPACE_CONVERSION") {
					imagePipeline->AddPlugin(OpenColorIOToneMap::CreateColorSpaceConversion(
						props.Get(Property(prefix + ".config")("")).Get<string>(),
						props.Get(Property(prefix + ".src")(OCIO::ROLE_RENDERING)).Get<string>(),
						props.Get(Property(prefix + ".dst")(OCIO::ROLE_INTERCHANGE_DISPLAY)).Get<string>()));
				} else if (mode == "LUT_CONVERSION") {
					imagePipeline->AddPlugin(OpenColorIOToneMap::CreateLUTConversion(
						props.Get(Property(prefix + ".lutfile")("file.lut")).Get<string>()));
				} else if (mode == "DISPLAY_CONVERSION") {
					imagePipeline->AddPlugin(OpenColorIOToneMap::CreateDisplayConversion(
						props.Get(Property(prefix + ".config")("")).Get<string>(),
						props.Get(Property(prefix + ".src")(OCIO::ROLE_RENDERING)).Get<string>(),
						props.Get(Property(prefix + ".display")(OCIO::ROLE_INTERCHANGE_DISPLAY)).Get<string>(),
						props.Get(Property(prefix + ".view")(OCIO::OCIO_VIEW_USE_DISPLAY_NAME)).Get<string>(),
						props.Get(Property(prefix + ".look")("")).Get<string>()));
				} else if (mode == "LOOK_CONVERSION") {
					imagePipeline->AddPlugin(OpenColorIOToneMap::CreateLookConversion(
						props.Get(Property(prefix + ".config")("")).Get<string>(),
						props.Get(Property(prefix + ".src")(OCIO::ROLE_RENDERING)).Get<string>(),
						props.Get(Property(prefix + ".look")("look")).Get<string>()));
				} else
					throw runtime_error("Unknown mode for TONEMAP_OPENCOLORIO: " + mode);
			} else
				throw runtime_error("Unknown image pipeline plugin type: " + type);
		}
	} else {
		// The definition of image pipeline is missing, use the default
		imagePipeline->AddPlugin(new AutoLinearToneMap());
		imagePipeline->AddPlugin(new GammaCorrectionPlugin(2.2f));
	}

	if (props.IsDefined("film.gamma"))
		SLG_LOG("WARNING: deprecated property film.gamma has no effects");
	
	// Initialize the radiance channel scales
	imagePipeline->SetRadianceGroupCount(radianceGroupCount);

	return imagePipeline.release();
}

void Film::ParseImagePipelines(const Properties &props) {
	// Look for the definition of multiple image pipelines
	vector<string> imagePipelineKeys = props.GetAllUniqueSubNames("film.imagepipelines", true);
	if (imagePipelineKeys.size() > 0) {	
		for (vector<string>::const_iterator imagePipelineKey = imagePipelineKeys.begin(); imagePipelineKey != imagePipelineKeys.end(); ++imagePipelineKey) {
			// Extract the image pipeline priority name
			const string imagePipelineNumberStr = Property::ExtractField(*imagePipelineKey, 2);
			if (imagePipelineNumberStr == "")
				throw runtime_error("Syntax error in image pipeline definition: " + *imagePipelineKey);
			
			u_int imagePipelineNumber;
			try {
				imagePipelineNumber = boost::lexical_cast<u_int>(imagePipelineNumberStr);
			} catch(boost::bad_lexical_cast &) {
				throw runtime_error("Syntax error in image pipeline number: " + *imagePipelineKey);
			}
			const string prefix = "film.imagepipelines." + imagePipelineNumberStr;

			SetImagePipelines(imagePipelineNumber, CreateImagePipeline(props, prefix));
		}
	} else {
		// Look for the definition of a single image pipeline
		SetImagePipelines(CreateImagePipeline(props, "film.imagepipeline"));
	}
	
	bool denoiserFound = false;
	for (auto ip : imagePipelines) {
		if (ip->GetPlugin(typeid(BCDDenoiserPlugin))) {
			denoiserFound = true;
			break;
		}
	}

	if (denoiserFound)
		SLG_LOG("BCD denoiser statistics collection enabled");

	// Enable or disable the collection statistics required by BCD_DENOISER plugin
	filmDenoiser.SetEnabled(denoiserFound);
}

//------------------------------------------------------------------------------
// Film parser
//------------------------------------------------------------------------------

void Film::Parse(const Properties &props) {
	//--------------------------------------------------------------------------
	// Check if there is a new image pipeline definition
	//--------------------------------------------------------------------------

	if (props.HaveNamesRE("film\\.imagepipeline\\.[0-9]+\\.type") ||
			props.HaveNamesRE("film\\.imagepipelines\\.[0-9]+\\.[0-9]+\\.type"))
		ParseImagePipelines(props);

	//--------------------------------------------------------------------------
	// Check if there are new radiance group scales
	//--------------------------------------------------------------------------

	if (props.HaveNames("film.imagepipeline.radiancescales.") ||
			props.HaveNamesRE("film\\.imagepipelines\\.[0-9]+\\.radiancescales\\..*"))
		ParseRadianceGroupsScales(props);

	//--------------------------------------------------------------------------
	// Check if there are new output definitions
	//--------------------------------------------------------------------------

	if (props.HaveNames("film.outputs."))
		ParseOutputs(props);

	//--------------------------------------------------------------------------
	// Check if there is a new halt test
	//--------------------------------------------------------------------------

	if (props.IsDefined("batch.haltnoisethreshold") || 
		props.IsDefined("batch.haltthreshold")) {
		delete convTest;
		convTest = NULL;

		haltNoiseThreshold = props.Get(Property("batch.haltnoisethreshold")(
				props.Get(Property("batch.haltthreshold")(-1.f)).Get<float>()
				)).Get<float>();

		if (haltNoiseThreshold > 0.f) {
			haltNoiseThresholdWarmUp = props.Get(Property("batch.haltnoisethreshold.warmup")(
						props.Get(Property("batch.haltthreshold.warmup")(64)).Get<u_int>()
					)).Get<u_int>();

			haltNoiseThresholdTestStep = props.Get(Property("batch.haltnoisethreshold.step")(
						props.Get(Property("batch.haltthreshold.step")(64)).Get<u_int>()
					)).Get<u_int>();
					
			haltNoiseThresholdUseFilter = props.Get(Property("batch.haltnoisethreshold.filter.enable")(
						props.Get(Property("batch.haltthreshold.filter.enable")(true)).Get<bool>()
					)).Get<bool>();

			haltNoiseThresholdStopRendering = props.Get(Property("batch.haltnoisethreshold.stoprendering.enable")(
						props.Get(Property("batch.haltthreshold.stoprendering.enable")(true)).Get<bool>()
					)).Get<bool>();
				
			haltNoiseThresholdImagePipelineIndex = props.Get(Property("batch.haltnoisethreshold.index")(0)).Get<u_int>();

			if (haltNoiseThresholdImagePipelineIndex >= GetImagePipelineCount()) {
				SLG_LOG("WARNING: Halt threshold image pipeline index not available. Reverting to first image pipeline");
				haltNoiseThresholdImagePipelineIndex = 0;
			}

			convTest = new FilmConvTest(this, haltNoiseThreshold, haltNoiseThresholdWarmUp,
					haltNoiseThresholdTestStep, haltNoiseThresholdUseFilter,
					haltNoiseThresholdImagePipelineIndex);
		}
	}

	if (props.IsDefined("batch.halttime"))
		haltTime = Max(0.0, props.Get(Property("batch.halttime")(0.0)).Get<double>());

	if (props.IsDefined("batch.haltspp")) {
		const Property haltDefaultProp = Property("batch.haltspp")(0u);
		const Property &haltProp = props.Get(haltDefaultProp);
		switch (haltProp.GetSize()) {
			case 1:
				haltSPP = haltProp.Get<u_int>();
				haltSPP_PixelNormalized = 0;
				haltSPP_ScreenNormalized = 0;
				break;
			case 2:
				haltSPP = 0;
				haltSPP_PixelNormalized = haltProp.Get<u_int>(0);
				haltSPP_ScreenNormalized = haltProp.Get<u_int>(1);
				break;
			default:
				throw runtime_error("Wrong number of arguments in batch.haltspp property: " + ToString(haltProp.GetSize()));
		}
		
	}

	//--------------------------------------------------------------------------
	// Check if there is adaptive sampling
	//--------------------------------------------------------------------------

	if (props.HaveNamesRE("film.noiseestimation\\..+")) {
		delete noiseEstimation;
		noiseEstimation = NULL;

		noiseEstimationWarmUp = props.Get(Property("film.noiseestimation.warmup")(32)).Get<u_int>();
		noiseEstimationTestStep = props.Get(Property("film.noiseestimation.step")(32)).Get<u_int>();
		noiseEstimationFilterScale = props.Get(Property("film.noiseestimation.filter.scale")(4)).Get<u_int>();

		noiseEstimationImagePipelineIndex = props.Get(Property("film.noiseestimation.index")(0)).Get<u_int>();
		if (noiseEstimationImagePipelineIndex >= GetImagePipelineCount()) {
			SLG_LOG("WARNING: Noise estimation image pipeline index not available. Reverting to first image pipeline");
			noiseEstimationImagePipelineIndex = 0;
		}

		noiseEstimation = new FilmNoiseEstimation(this, noiseEstimationWarmUp,
				noiseEstimationTestStep, noiseEstimationFilterScale, noiseEstimationImagePipelineIndex);
	}
}
