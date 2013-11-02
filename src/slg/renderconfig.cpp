/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include <boost/lexical_cast.hpp>
#include <memory>

#include "slg/renderconfig.h"
#include "slg/renderengine.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/engines/rtbiaspathocl/rtbiaspathocl.h"
#include "slg/engines/lightcpu/lightcpu.h"
#include "slg/engines/pathcpu/pathcpu.h"
#include "slg/engines/bidircpu/bidircpu.h"
#include "slg/engines/bidirhybrid/bidirhybrid.h"
#include "slg/engines/cbidirhybrid/cbidirhybrid.h"
#include "slg/engines/bidirvmcpu/bidirvmcpu.h"
#include "slg/engines/filesaver/filesaver.h"
#include "slg/engines/pathhybrid/pathhybrid.h"
#include "slg/engines/biaspathcpu/biaspathcpu.h"
#include "slg/engines/biaspathocl/biaspathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

RenderConfig::RenderConfig(const luxrays::Properties &props, Scene *scn) : scene(scn) {
	SLG_LOG("Configuration: ");
	const vector<string> &keys = props.GetAllNames();
	for (vector<string>::const_iterator i = keys.begin(); i != keys.end(); ++i)
		SLG_LOG("  " << props.Get(*i));

	// Set the Scene
	if (scn) {
		scene = scn;
		allocatedScene = false;
	} else {
		// Create the Scene
		const string sceneFileName = props.Get(Property("scene.file")("scenes/luxball/luxball.scn")).Get<string>();
		const float imageScale = Max(.01f, props.Get(Property("images.scale")(1.f)).Get<float>());

		scene = new Scene(sceneFileName, imageScale);
		allocatedScene = true;
	}

	// Parse the configuration
	Parse(props);
}

RenderConfig::~RenderConfig() {
	// Check if the scene was allocated by me
	if (allocatedScene)
		delete scene;
}

void RenderConfig::Parse(const luxrays::Properties &props) {
	cfg.Set(props);

	// RTPATHOCL has a different default screen.refresh.interval value
	const RenderEngineType renderEngineType = RenderEngine::String2RenderEngineType(
		cfg.Get(Property("renderengine.type")("PATHOCL")).Get<string>());
	const int interval = (renderEngineType == RTPATHOCL) ? 33 : 100;
	screenRefreshInterval = cfg.Get(Property("screen.refresh.interval")(interval)).Get<u_int>();

	scene->enableInstanceSupport = cfg.Get(Property("accelerator.instances.enable")(true)).Get<bool>();
	const string accelType = cfg.Get(Property("accelerator.type")("AUTO")).Get<string>();
	// "-1" is for compatibility with the past. However all other old values are
	// not emulated (i.e. the "AUTO" behavior is preferred in that case)
	if ((accelType == "AUTO") || (accelType == "-1"))
		scene->accelType = ACCEL_AUTO;
	else if (accelType == "BVH")
		scene->accelType = ACCEL_BVH;
	else if (accelType == "MBVH")
		scene->accelType = ACCEL_MBVH;
	else if (accelType == "QBVH")
		scene->accelType = ACCEL_QBVH;
	else if (accelType == "MQBVH")
		scene->accelType = ACCEL_MQBVH;
	else {
		SLG_LOG("Unknown accelerator type (using AUTO instead): " << accelType);
	}

	// Update the Camera
	u_int filmFullWidth, filmFullHeight, filmSubRegion[4];
	u_int *subRegion = GetFilmSize(&filmFullWidth, &filmFullHeight, filmSubRegion) ?
		filmSubRegion : NULL;
	scene->camera->Update(filmFullWidth, filmFullHeight, subRegion);
}

void RenderConfig::SetScreenRefreshInterval(const u_int t) {
	screenRefreshInterval = t;
}

u_int RenderConfig::GetScreenRefreshInterval() const {
	return screenRefreshInterval;
}

bool RenderConfig::GetFilmSize(u_int *filmFullWidth, u_int *filmFullHeight,
		u_int *filmSubRegion) const {
	u_int width = 640;
	if (cfg.IsDefined("image.width")) {
		SLG_LOG("WARNING: deprecated property image.width");
		width = cfg.Get(Property("image.width")(width)).Get<u_int>();
	}
	width = cfg.Get(Property("film.width")(width)).Get<u_int>();

	u_int height = 480;
	if (cfg.IsDefined("image.height")) {
		SLG_LOG("WARNING: deprecated property image.height");
		height = cfg.Get(Property("image.height")(height)).Get<u_int>();
	}
	height = cfg.Get(Property("film.height")(height)).Get<u_int>();

	// Check if I'm rendering a film subregion
	u_int subRegion[4];
	bool subRegionUsed;
	if (cfg.IsDefined("film.subregion")) {
		const Property &prop = cfg.Get(Property("film.subregion")(0, width - 1u, 0, height - 1u));
		if (prop.GetSize() != 4)
			throw runtime_error("Syntax error in film.subregion (required 4 parameters)");

		subRegion[0] = Max(0u, Min(width - 1, prop.Get<u_int>(0)));
		subRegion[1] = Max(0u, Min(width - 1, Max(subRegion[0] + 1, prop.Get<u_int>(1))));
		subRegion[2] = Max(0u, Min(height - 1, prop.Get<u_int>(2)));
		subRegion[3] = Max(0u, Min(height - 1, Max(subRegion[2] + 1, prop.Get<u_int>(3))));
		subRegionUsed = true;
	} else {
		subRegion[0] = 0;
		subRegion[1] = width - 1;
		subRegion[2] = 0;
		subRegion[3] = height - 1;
		subRegionUsed = false;
	}

	*filmFullWidth = width;
	*filmFullHeight = height;
	if (filmSubRegion) {
		filmSubRegion[0] = subRegion[0];
		filmSubRegion[1] = subRegion[1];
		filmSubRegion[2] = subRegion[2];
		filmSubRegion[3] = subRegion[3];
	}

	return subRegionUsed;
}

void RenderConfig::GetScreenSize(u_int *screenWidth, u_int *screenHeight) const {
	u_int filmWidth, filmHeight, filmSubRegion[4];
	GetFilmSize(&filmWidth, &filmHeight, filmSubRegion);

	*screenWidth = filmSubRegion[1] - filmSubRegion[0] + 1;
	*screenHeight = filmSubRegion[3] - filmSubRegion[2] + 1;
}

Film *RenderConfig::AllocFilm(FilmOutputs &filmOutputs) const {
	//--------------------------------------------------------------------------
	// Create the filter
	//--------------------------------------------------------------------------

	const FilterType filterType = Filter::String2FilterType(cfg.Get(Property("film.filter.type")("GAUSSIAN")).Get<string>());
	const float filterWidth = cfg.Get(Property("film.filter.width")(1.5f)).Get<float>();

	auto_ptr<Filter> filter;
	switch (filterType) {
		case FILTER_NONE:
			break;
		case FILTER_BOX:
			filter.reset(new BoxFilter(filterWidth, filterWidth));
			break;
		case FILTER_GAUSSIAN: {
			const float alpha = cfg.Get(Property("film.filter.gaussian.alpha")(2.f)).Get<float>();
			filter.reset(new GaussianFilter(filterWidth, filterWidth, alpha));
			break;
		}
		case FILTER_MITCHELL: {
			const float b = cfg.Get(Property("film.filter.mitchell.b")(1.f / 3.f)).Get<float>();
			const float c = cfg.Get(Property("film.filter.mitchell.c")(1.f / 3.f)).Get<float>();
			filter.reset(new MitchellFilter(filterWidth, filterWidth, b, c));
			break;
		}
		case FILTER_MITCHELL_SS: {
			const float b = cfg.Get(Property("film.filter.mitchellss.b")(1.f / 3.f)).Get<float>();
			const float c = cfg.Get(Property("film.filter.mitchellss.c")(1.f / 3.f)).Get<float>();
			filter.reset(new MitchellFilterSS(filterWidth, filterWidth, b, c));
			break;
		}
		default:
			throw std::runtime_error("Unknown filter type: " + boost::lexical_cast<std::string>(filterType));
	}

	//--------------------------------------------------------------------------
	// Create the Film
	//--------------------------------------------------------------------------

	u_int filmFullWidth, filmFullHeight, filmSubRegion[4];
	GetFilmSize(&filmFullWidth, &filmFullHeight, filmSubRegion);

	SDL_LOG("Film resolution: " << filmFullWidth << "x" << filmFullHeight);
	auto_ptr<Film> film(new Film(filmFullWidth, filmFullHeight));
	film->SetFilter(filter.release());

	const int toneMapType = cfg.Get(Property("film.tonemap.type")(0)).Get<int>();
	if (toneMapType == 0) {
		LinearToneMapParams params;
		params.scale = cfg.Get(Property("film.tonemap.linear.scale")(params.scale)).Get<float>();
		film->SetToneMapParams(params);
	} else {
		Reinhard02ToneMapParams params;
		params.preScale = cfg.Get(Property("film.tonemap.reinhard02.prescale")(params.preScale)).Get<float>();
		params.postScale = cfg.Get(Property("film.tonemap.reinhard02.postscale")(params.postScale)).Get<float>();
		params.burn = cfg.Get(Property("film.tonemap.reinhard02.burn")(params.burn)).Get<float>();
		film->SetToneMapParams(params);
	}

	const float gamma = cfg.Get(Property("film.gamma")(2.2f)).Get<float>();
	if (gamma != 2.2f)
		film->SetGamma(gamma);

	// For compatibility with the past
	if (cfg.IsDefined("film.alphachannel.enable")) {
		SLG_LOG("WARNING: deprecated property film.alphachannel.enable");

		if (cfg.Get(Property("film.alphachannel.enable")(0)).Get<bool>())
			film->AddChannel(Film::ALPHA);
		else
			film->RemoveChannel(Film::ALPHA);
	}

	//--------------------------------------------------------------------------
	// Initialize the FilmOutputs
	//--------------------------------------------------------------------------

	set<string> outputNames;
	vector<string> outputKeys = cfg.GetAllNames("film.outputs.");
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
		const string type = cfg.Get(Property("film.outputs." + outputName + ".type")("RGB_TONEMAPPED")).Get<string>();
		const string fileName = cfg.Get(Property("film.outputs." + outputName + ".filename")("image.png")).Get<string>();

		SDL_LOG("Film output definition: " << type << " [" << fileName << "]");

		// Check if it is a supported file format
		FREE_IMAGE_FORMAT fif = FREEIMAGE_GETFIFFROMFILENAME(FREEIMAGE_CONVFILENAME(fileName).c_str());
		if (fif == FIF_UNKNOWN)
			throw std::runtime_error("Unknown image format in film output: " + outputName);

		// HDR image or not
		const bool hdrImage = ((fif == FIF_HDR) || (fif == FIF_EXR));

		if (type == "RGB") {
			if (hdrImage)
				filmOutputs.Add(FilmOutputs::RGB, fileName);
			else
				throw std::runtime_error("Not tonemapped image can be saved only in HDR formats: " + outputName);
		} else if (type == "RGBA") {
			if (hdrImage) {
				film->AddChannel(Film::ALPHA);
				filmOutputs.Add(FilmOutputs::RGBA, fileName);
			} else
				throw std::runtime_error("Not tonemapped image can be saved only in HDR formats: " + outputName);
		} else if (type == "RGB_TONEMAPPED")
			filmOutputs.Add(FilmOutputs::RGB_TONEMAPPED, fileName);
		else if (type == "RGBA_TONEMAPPED") {
			film->AddChannel(Film::ALPHA);
			filmOutputs.Add(FilmOutputs::RGBA_TONEMAPPED, fileName);
		} else if (type == "ALPHA") {
			film->AddChannel(Film::ALPHA);
			filmOutputs.Add(FilmOutputs::ALPHA, fileName);
		} else if (type == "DEPTH") {
			if (hdrImage) {
				film->AddChannel(Film::DEPTH);
				filmOutputs.Add(FilmOutputs::DEPTH, fileName);
			} else
				throw std::runtime_error("Depth image can be saved only in HDR formats: " + outputName);
		} else if (type == "POSITION") {
			if (hdrImage) {
				film->AddChannel(Film::DEPTH);
				film->AddChannel(Film::POSITION);
				filmOutputs.Add(FilmOutputs::POSITION, fileName);
			} else
				throw std::runtime_error("Position image can be saved only in HDR formats: " + outputName);
		} else if (type == "GEOMETRY_NORMAL") {
			if (hdrImage) {
				film->AddChannel(Film::DEPTH);
				film->AddChannel(Film::GEOMETRY_NORMAL);
				filmOutputs.Add(FilmOutputs::GEOMETRY_NORMAL, fileName);
			} else
				throw std::runtime_error("Geometry normal image can be saved only in HDR formats: " + outputName);
		} else if (type == "SHADING_NORMAL") {
			if (hdrImage) {
				film->AddChannel(Film::DEPTH);
				film->AddChannel(Film::SHADING_NORMAL);
				filmOutputs.Add(FilmOutputs::SHADING_NORMAL, fileName);
			} else
				throw std::runtime_error("Shading normal image can be saved only in HDR formats: " + outputName);
		} else if (type == "MATERIAL_ID") {
			if (!hdrImage) {
				film->AddChannel(Film::DEPTH);
				film->AddChannel(Film::MATERIAL_ID);
				filmOutputs.Add(FilmOutputs::MATERIAL_ID, fileName);
			} else
				throw std::runtime_error("Material ID image can be saved only in no HDR formats: " + outputName);
		} else if (type == "DIRECT_DIFFUSE") {
			if (hdrImage) {
				film->AddChannel(Film::DIRECT_DIFFUSE);
				filmOutputs.Add(FilmOutputs::DIRECT_DIFFUSE, fileName);
			} else
				throw std::runtime_error("Direct diffuse image can be saved only in HDR formats: " + outputName);
		} else if (type == "DIRECT_GLOSSY") {
			if (hdrImage) {
				film->AddChannel(Film::DIRECT_GLOSSY);
				filmOutputs.Add(FilmOutputs::DIRECT_GLOSSY, fileName);
			} else
				throw std::runtime_error("Direct glossy image can be saved only in HDR formats: " + outputName);
		} else if (type == "EMISSION") {
			if (hdrImage) {
				film->AddChannel(Film::EMISSION);
				filmOutputs.Add(FilmOutputs::EMISSION, fileName);
			} else
				throw std::runtime_error("Emission image can be saved only in HDR formats: " + outputName);
		} else if (type == "INDIRECT_DIFFUSE") {
			if (hdrImage) {
				film->AddChannel(Film::INDIRECT_DIFFUSE);
				filmOutputs.Add(FilmOutputs::INDIRECT_DIFFUSE, fileName);
			} else
				throw std::runtime_error("Indirect diffuse image can be saved only in HDR formats: " + outputName);
		} else if (type == "INDIRECT_GLOSSY") {
			if (hdrImage) {
				film->AddChannel(Film::INDIRECT_GLOSSY);
				filmOutputs.Add(FilmOutputs::INDIRECT_GLOSSY, fileName);
			} else
				throw std::runtime_error("Indirect glossy image can be saved only in HDR formats: " + outputName);
		} else if (type == "INDIRECT_SPECULAR") {
			if (hdrImage) {
				film->AddChannel(Film::INDIRECT_SPECULAR);
				filmOutputs.Add(FilmOutputs::INDIRECT_SPECULAR, fileName);
			} else
				throw std::runtime_error("Indirect specular image can be saved only in HDR formats: " + outputName);
		} else if (type == "MATERIAL_ID_MASK") {
			const u_int materialID = cfg.Get(Property("film.outputs." + outputName + ".id")(255)).Get<u_int>();
			Properties prop;
			prop.Set(Property("id")(materialID));

			film->AddChannel(Film::MATERIAL_ID);
			film->AddChannel(Film::MATERIAL_ID_MASK, &prop);
			filmOutputs.Add(FilmOutputs::MATERIAL_ID_MASK, fileName, &prop);
		} else if (type == "DIRECT_SHADOW_MASK") {
			film->AddChannel(Film::DIRECT_SHADOW_MASK);
			filmOutputs.Add(FilmOutputs::DIRECT_SHADOW_MASK, fileName);
		} else if (type == "INDIRECT_SHADOW_MASK") {
			film->AddChannel(Film::INDIRECT_SHADOW_MASK);
			filmOutputs.Add(FilmOutputs::INDIRECT_SHADOW_MASK, fileName);
		} else if (type == "RADIANCE_GROUP") {
			const u_int lightID = cfg.Get(Property("film.outputs." + outputName + ".id")(0)).Get<u_int>();
			Properties prop;
			prop.Set(Property("id")(lightID));

			filmOutputs.Add(FilmOutputs::RADIANCE_GROUP, fileName, &prop);
		} else if (type == "UV") {
			film->AddChannel(Film::DEPTH);
			film->AddChannel(Film::UV);
			filmOutputs.Add(FilmOutputs::UV, fileName);
		} else if (type == "RAYCOUNT") {
			film->AddChannel(Film::RAYCOUNT);
			filmOutputs.Add(FilmOutputs::RAYCOUNT, fileName);
		} else
			throw std::runtime_error("Unknown type in film output: " + type);
	}

	// For compatibility with the past
	if (cfg.IsDefined("image.filename")) {
		SLG_LOG("WARNING: deprecated property image.filename");
		filmOutputs.Add(film->HasChannel(Film::ALPHA) ? FilmOutputs::RGBA_TONEMAPPED : FilmOutputs::RGB_TONEMAPPED,
				cfg.Get(Property("image.filename")("image.png")).Get<string>());
	}

	// Default setting
	if (filmOutputs.GetCount() == 0)
		filmOutputs.Add(FilmOutputs::RGB_TONEMAPPED, "image.png");

	return film.release();
}

Sampler *RenderConfig::AllocSampler(RandomGenerator *rndGen, Film *film,
		double *metropolisSharedTotalLuminance, double *metropolisSharedSampleCount) const {
	const SamplerType samplerType = Sampler::String2SamplerType(cfg.Get(Property("sampler.type")("RANDOM")).Get<string>());
	switch (samplerType) {
		case RANDOM:
			return new RandomSampler(rndGen, film);
		case METROPOLIS: {
			const float rate = cfg.Get(Property("sampler.largesteprate")(.4f)).Get<float>();
			const float reject = cfg.Get(Property("sampler.maxconsecutivereject")(512)).Get<float>();
			const float mutationrate = cfg.Get(Property("sampler.imagemutationrate")(.1f)).Get<float>();

			return new MetropolisSampler(rndGen, film, reject, rate, mutationrate,
					metropolisSharedTotalLuminance, metropolisSharedSampleCount);
		}
		case SOBOL:
			return new SobolSampler(rndGen, film);
		default:
			throw std::runtime_error("Unknown sampler.type: " + boost::lexical_cast<std::string>(samplerType));
	}
}

RenderEngine *RenderConfig::AllocRenderEngine(Film *film, boost::mutex *filmMutex) const {
	const RenderEngineType renderEngineType = RenderEngine::String2RenderEngineType(
		cfg.Get(Property("renderengine.type")("PATHOCL")).Get<string>());

	switch (renderEngineType) {
		case LIGHTCPU:
			return new LightCPURenderEngine(this, film, filmMutex);
		case PATHOCL:
#ifndef LUXRAYS_DISABLE_OPENCL
			return new PathOCLRenderEngine(this, film, filmMutex);
#else
			SLG_LOG("OpenCL unavailable, falling back to CPU rendering");
#endif
		case PATHCPU:
			return new PathCPURenderEngine(this, film, filmMutex);
		case BIDIRCPU:
			return new BiDirCPURenderEngine(this, film, filmMutex);
		case BIDIRHYBRID:
			return new BiDirHybridRenderEngine(this, film, filmMutex);
		case CBIDIRHYBRID:
			return new CBiDirHybridRenderEngine(this, film, filmMutex);
		case BIDIRVMCPU:
			return new BiDirVMCPURenderEngine(this, film, filmMutex);
		case FILESAVER:
			return new FileSaverRenderEngine(this, film, filmMutex);
		case RTPATHOCL:
#ifndef LUXRAYS_DISABLE_OPENCL
			return new RTPathOCLRenderEngine(this, film, filmMutex);
#else
			SLG_LOG("OpenCL unavailable, falling back to CPU rendering");
			return new PathCPURenderEngine(this, film, filmMutex);
#endif
		case PATHHYBRID:
			return new PathHybridRenderEngine(this, film, filmMutex);
		case BIASPATHCPU:
			return new BiasPathCPURenderEngine(this, film, filmMutex);
		case BIASPATHOCL:
#ifndef LUXRAYS_DISABLE_OPENCL
			return new BiasPathOCLRenderEngine(this, film, filmMutex);
#else
			SLG_LOG("OpenCL unavailable, falling back to CPU rendering");
			return new BiasPathCPURenderEngine(this, film, filmMutex);
#endif
		case RTBIASPATHOCL:
#ifndef LUXRAYS_DISABLE_OPENCL
			return new RTBiasPathOCLRenderEngine(this, film, filmMutex);
#else
			SLG_LOG("OpenCL unavailable, falling back to CPU rendering");
			return new BiasPathCPURenderEngine(this, film, filmMutex);
#endif
		default:
			throw runtime_error("Unknown render engine type: " + boost::lexical_cast<std::string>(renderEngineType));
	}
}
