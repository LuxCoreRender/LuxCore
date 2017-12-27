/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/utils/fileext.h"
#include "slg/core/sdl.h"
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
#include "slg/film/imagepipeline/plugins/backgroundimg.h"
#include "slg/film/imagepipeline/plugins/bloom.h"
#include "slg/film/imagepipeline/plugins/objectidmask.h"
#include "slg/film/imagepipeline/plugins/vignetting.h"
#include "slg/film/imagepipeline/plugins/coloraberration.h"
#include "slg/film/imagepipeline/plugins/premultiplyalpha.h"
#include "slg/film/imagepipeline/plugins/mist.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ParseOutputs
//------------------------------------------------------------------------------

void Film::ParseOutputs(const Properties &props) {
	// Initialize the FilmOutputs

	filmOutputs.Clear();

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
			case FilmOutputs::DIRECT_GLOSSY: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::DIRECT_GLOSSY);
					filmOutputs.Add(FilmOutputs::DIRECT_GLOSSY, fileName);
				} else
					throw runtime_error("Direct glossy image can be saved only in HDR formats: " + outputName);
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
			case FilmOutputs::INDIRECT_GLOSSY: {
				if (hdrImage) {
					if (!initialized)
						AddChannel(Film::INDIRECT_GLOSSY);
					filmOutputs.Add(FilmOutputs::INDIRECT_GLOSSY, fileName);
				} else
					throw runtime_error("Indirect glossy image can be saved only in HDR formats: " + outputName);
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
			case FilmOutputs::FRAMEBUFFER_MASK: {
				if (!initialized)
					AddChannel(Film::FRAMEBUFFER_MASK);
				filmOutputs.Add(FilmOutputs::FRAMEBUFFER_MASK, fileName);
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
// SetRadianceGroupsScale
//------------------------------------------------------------------------------

void Film::ParseRadianceGroupsScale(const Properties &props) {
	boost::unordered_set<string> radianceScaleIndices;
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

		const string prefix = "film.radiancescales." + indexStr;
		Film::RadianceChannelScale radianceChannelScale;
		radianceChannelScale.globalScale = props.Get(Property(prefix + ".globalscale")(1.f)).Get<float>();
		radianceChannelScale.temperature = props.Get(Property(prefix + ".temperature")(0.f)).Get<float>();
		radianceChannelScale.rgbScale = props.Get(Property(prefix + ".rgbscale")(1.f, 1.f, 1.f)).Get<Spectrum>();
		radianceChannelScale.enabled = props.Get(Property(prefix + ".enabled")(true)).Get<bool>();

		SetRadianceChannelScale(index, radianceChannelScale);
	}
}

//------------------------------------------------------------------------------
// AllocImagePipeline
//------------------------------------------------------------------------------

ImagePipeline *Film::AllocImagePipeline(const Properties &props, const string &imagePipelinePrefix) {
	//--------------------------------------------------------------------------
	// Create the image pipeline
	//--------------------------------------------------------------------------

	const u_int keyFieldCount = Property::CountFields(imagePipelinePrefix);
	auto_ptr<ImagePipeline> imagePipeline(new ImagePipeline());

	vector<string> imagePipelineKeys = props.GetAllUniqueSubNames(imagePipelinePrefix);
	if (imagePipelineKeys.size() > 0) {
		// Sort the entries
		sort(imagePipelineKeys.begin(), imagePipelineKeys.end());

		SDL_LOG("Image pipeline: " << imagePipelinePrefix);
		for (vector<string>::const_iterator imagePipelineKey = imagePipelineKeys.begin(); imagePipelineKey != imagePipelineKeys.end(); ++imagePipelineKey) {
			// Extract the plugin priority name
			const string pluginPriority = Property::ExtractField(*imagePipelineKey, keyFieldCount);
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
				imagePipeline->AddPlugin(new OutputSwitcherPlugin(
					Film::String2FilmChannelType(props.Get(Property(prefix + ".channel")("DEPTH")).Get<string>()),
					props.Get(Property(prefix + ".index")(0u)).Get<u_int>()));
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
				const float scale = Clamp(props.Get(Property(prefix + ".amount")(.005f)).Get<float>(), 0.f, 1.f);

				imagePipeline->AddPlugin(new ColorAberrationPlugin(scale));
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
	
	return imagePipeline.release();
}

vector<ImagePipeline *> Film::AllocImagePipelines(const Properties &props) {
	vector<ImagePipeline *> imagePipelines;

	// Look for the definition of multiple image pipelines
	vector<string> imagePipelineKeys = props.GetAllUniqueSubNames("film.imagepipelines");
	if (imagePipelineKeys.size() > 0) {
		// Sort the entries
		sort(imagePipelineKeys.begin(), imagePipelineKeys.end());

		for (vector<string>::const_iterator imagePipelineKey = imagePipelineKeys.begin(); imagePipelineKey != imagePipelineKeys.end(); ++imagePipelineKey) {
			// Extract the image pipeline priority name
			const string imagePieplinePriority = Property::ExtractField(*imagePipelineKey, 2);
			if (imagePieplinePriority == "")
				throw runtime_error("Syntax error in image pipeline definition: " + *imagePipelineKey);
			const string prefix = "film.imagepipelines." + imagePieplinePriority;

			imagePipelines.push_back(AllocImagePipeline(props, prefix));
		}
	} else {
		// Look for the definition of a single image pipeline
		imagePipelines.push_back(AllocImagePipeline(props, "film.imagepipeline"));
	}
	
	return imagePipelines;
}

//------------------------------------------------------------------------------
// Film parser
//------------------------------------------------------------------------------

void Film::Parse(const Properties &props) {
	//--------------------------------------------------------------------------
	// Check if there is a new image pipeline definition
	//--------------------------------------------------------------------------

	if (props.HaveNames("film.imagepipeline.") || props.HaveNames("film.imagepipelines.")) {
		// Create the new image pipeline(s)
		vector<ImagePipeline *> newImagePipelines = AllocImagePipelines(props);

		// Use the new image pipeline
		SetImagePipelines(newImagePipelines);
	}

	//--------------------------------------------------------------------------
	// Check if there are new radiance groups scale
	//--------------------------------------------------------------------------

	if (props.HaveNames("film.radiancescales."))
		ParseRadianceGroupsScale(props);

	//--------------------------------------------------------------------------
	// Check if there are new output definitions
	//--------------------------------------------------------------------------

	if (props.HaveNames("film.outputs."))
		ParseOutputs(props);
}
