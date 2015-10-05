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

#include <memory>

#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp> 
#include <boost/filesystem.hpp>

#include "slg/renderconfig.h"
#include "slg/renderengine.h"
#include "slg/film/film.h"

#include "slg/samplers/random.h"
#include "slg/samplers/sobol.h"
#include "slg/samplers/metropolis.h"

#include "slg/film/filters/box.h"
#include "slg/film/filters/gaussian.h"
#include "slg/film/filters/mitchell.h"
#include "slg/film/filters/mitchellss.h"
#include "slg/film/filters/blackmanharris.h"

#include "slg/film/imagepipeline/plugins/tonemaps/autolinear.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"

#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/engines/rtbiaspathocl/rtbiaspathocl.h"
#include "slg/engines/lightcpu/lightcpu.h"
#include "slg/engines/pathcpu/pathcpu.h"
#include "slg/engines/bidircpu/bidircpu.h"
#include "slg/engines/bidirvmcpu/bidirvmcpu.h"
#include "slg/engines/filesaver/filesaver.h"
#include "slg/engines/biaspathcpu/biaspathcpu.h"
#include "slg/engines/biaspathocl/biaspathocl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

static boost::mutex defaultPropertiesMutex;
static auto_ptr<Properties> defaultProperties;

void RenderConfig::InitDefaultProperties() {
	// Check if I have to initialize the default Properties
	if (!defaultProperties.get()) {
		boost::unique_lock<boost::mutex> lock(defaultPropertiesMutex);
		if (!defaultProperties.get()) {
			defaultProperties.reset(new Properties());

			defaultProperties->Set(Property("accelerator.instances.enable")(true));
			defaultProperties->Set(Property("accelerator.type")("AUTO"));

			defaultProperties->Set(Property("lightstrategy.type")("LOG_POWER"));

			// Batch related Properties
			defaultProperties->Set(Property("batch.halttime")(0u));
			defaultProperties->Set(Property("batch.haltspp")(0u));
			defaultProperties->Set(Property("batch.haltthreshold")(-1.f));
			defaultProperties->Set(Property("batch.haltdebug")(0u));

			// Film Filter related Properties
			defaultProperties->Set(Property("film.filter.type")("BLACKMANHARRIS"));
			defaultProperties->Set(Property("film.filter.width")(2.f));
			defaultProperties->Set(Property("film.filter.gaussian.alpha")(2.f));
			defaultProperties->Set(Property("film.filter.mitchell.b")(1.f / 3.f));
			defaultProperties->Set(Property("film.filter.mitchell.c")(1.f / 3.f));
			defaultProperties->Set(Property("film.filter.mitchellss.b")(1.f / 3.f));
			defaultProperties->Set(Property("film.filter.mitchellss.c")(1.f / 3.f));

			defaultProperties->Set(Property("film.height")(480u));
			defaultProperties->Set(Property("film.width")(640u));

			// Sampler related Properties
			defaultProperties->Set(Property("sampler.type")("RANDOM"));
			defaultProperties->Set(Property("sampler.metropolis.largesteprate")(.4f));
			defaultProperties->Set(Property("sampler.metropolis.maxconsecutivereject")(512));
			defaultProperties->Set(Property("sampler.metropolis.imagemutationrate")(.1f));
			
			defaultProperties->Set(Property("images.scale")(1.f));
			defaultProperties->Set(Property("renderengine.type")("PATHOCL"));
			defaultProperties->Set(Property("scene.file")("scenes/luxball/luxball.scn"));
			defaultProperties->Set(Property("screen.refresh.interval")(100u));

			// Specific RenderEngine settings are defined in each RenderEngine::Start() method
		}
	}
}

const Properties &RenderConfig::GetDefaultProperties() {
	InitDefaultProperties();

	return *defaultProperties;
}

RenderConfig::RenderConfig(const Properties &props, Scene *scn) : scene(scn) {
	InitDefaultProperties();

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
		const string defaultSceneName = GetDefaultProperties().Get("scene.file").Get<string>();
		const string sceneFileName = props.Get(Property("scene.file")(defaultSceneName)).Get<string>();
		const float defaultImageScale = GetDefaultProperties().Get("images.scale").Get<float>();
		const float imageScale = Max(.01f, props.Get(Property("images.scale")(defaultImageScale)).Get<float>());

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

const Property RenderConfig::GetProperty(const string &name) const {
	if (cfg.IsDefined(name))
		return cfg.Get(name);
	else {
		// Use the default value
		return defaultProperties->Get(name);
	}
}

void RenderConfig::Parse(const Properties &props) {
	cfg.Set(props);

	scene->enableInstanceSupport = GetProperty("accelerator.instances.enable").Get<bool>();
	const string accelType = GetProperty("accelerator.type").Get<string>();
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
	else if (accelType == "EMBREE")
		scene->accelType = ACCEL_EMBREE;
	else {
		SLG_LOG("Unknown accelerator type (using AUTO instead): " << accelType);
	}

	const string lightStrategy = GetProperty("lightstrategy.type").Get<string>();
	if (lightStrategy == "UNIFORM")
		scene->lightDefs.SetLightStrategy(TYPE_UNIFORM);
	else if (lightStrategy == "POWER")
		scene->lightDefs.SetLightStrategy(TYPE_POWER);
	else if (lightStrategy == "LOG_POWER")
		scene->lightDefs.SetLightStrategy(TYPE_LOG_POWER);
	else {
		SLG_LOG("Unknown light strategy type (using AUTO instead): " << lightStrategy);
	}

	// Update the Camera
	u_int filmFullWidth, filmFullHeight, filmSubRegion[4];
	u_int *subRegion = GetFilmSize(&filmFullWidth, &filmFullHeight, filmSubRegion) ?
		filmSubRegion : NULL;
	scene->camera->Update(filmFullWidth, filmFullHeight, subRegion);
}

void RenderConfig::Delete(const string &prefix) {
	cfg.DeleteAll(cfg.GetAllNames(prefix));
}

bool RenderConfig::GetFilmSize(u_int *filmFullWidth, u_int *filmFullHeight,
		u_int *filmSubRegion) const {
	u_int width = 640;
	if (cfg.IsDefined("image.width")) {
		SLG_LOG("WARNING: deprecated property image.width");
		width = cfg.Get(Property("image.width")(width)).Get<u_int>();
	}
	width = GetProperty("film.width").Get<u_int>();

	u_int height = 480;
	if (cfg.IsDefined("image.height")) {
		SLG_LOG("WARNING: deprecated property image.height");
		height = cfg.Get(Property("image.height")(height)).Get<u_int>();
	}
	height = GetProperty("film.height").Get<u_int>();

	// Check if I'm rendering a film subregion
	u_int subRegion[4];
	bool subRegionUsed;
	if (cfg.IsDefined("film.subregion")) {
		const Property &prop = cfg.Get(Property("film.subregion")(0, width - 1u, 0, height - 1u));

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

	if (filmFullWidth)
		*filmFullWidth = width;
	if (filmFullHeight)
		*filmFullHeight = height;

	if (filmSubRegion) {
		filmSubRegion[0] = subRegion[0];
		filmSubRegion[1] = subRegion[1];
		filmSubRegion[2] = subRegion[2];
		filmSubRegion[3] = subRegion[3];
	}

	return subRegionUsed;
}

Filter *RenderConfig::AllocPixelFilter() const {
	//--------------------------------------------------------------------------
	// Create the filter
	//--------------------------------------------------------------------------

	const FilterType filterType = Filter::String2FilterType(GetProperty("film.filter.type").Get<string>());
	const float defaultFilterWidth = GetProperty("film.filter.width").Get<float>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<float>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<float>();

	auto_ptr<Filter> filter;
	switch (filterType) {
		case FILTER_NONE:
			break;
		case FILTER_BOX:
			filter.reset(new BoxFilter(filterXWidth, filterYWidth));
			break;
		case FILTER_GAUSSIAN: {
			const float alpha = GetProperty("film.filter.gaussian.alpha").Get<float>();
			filter.reset(new GaussianFilter(filterXWidth, filterYWidth, alpha));
			break;
		}
		case FILTER_MITCHELL: {
			const float b = GetProperty("film.filter.mitchell.b").Get<float>();
			const float c = GetProperty("film.filter.mitchell.c").Get<float>();
			filter.reset(new MitchellFilter(filterXWidth, filterYWidth, b, c));
			break;
		}
		case FILTER_MITCHELL_SS: {
			const float b = GetProperty("film.filter.mitchellss.b").Get<float>();
			const float c = GetProperty("film.filter.mitchellss.c").Get<float>();
			filter.reset(new MitchellFilterSS(filterXWidth, filterYWidth, b, c));
			break;
		}
		case FILTER_BLACKMANHARRIS: {
			filter.reset(new BlackmanHarrisFilter(filterXWidth, filterYWidth));
			break;
		}
		default:
			throw runtime_error("Unknown filter type: " + boost::lexical_cast<string>(filterType));
	}
	
	return filter.release();
}

Film *RenderConfig::AllocFilm() const {
	//--------------------------------------------------------------------------
	// Create the Film
	//--------------------------------------------------------------------------

	u_int filmFullWidth, filmFullHeight, filmSubRegion[4];
	GetFilmSize(&filmFullWidth, &filmFullHeight, filmSubRegion);

	SLG_LOG("Film resolution: " << filmFullWidth << "x" << filmFullHeight);
	auto_ptr<Film> film(new Film(filmFullWidth, filmFullHeight));

	// For compatibility with the past
	if (cfg.IsDefined("film.alphachannel.enable")) {
		SLG_LOG("WARNING: deprecated property film.alphachannel.enable");

		if (cfg.Get(Property("film.alphachannel.enable")(0)).Get<bool>())
			film->AddChannel(Film::ALPHA);
		else
			film->RemoveChannel(Film::ALPHA);
	}

	//--------------------------------------------------------------------------
	// Add the default image pipeline
	//--------------------------------------------------------------------------
		
	auto_ptr<ImagePipeline> imagePipeline(new ImagePipeline());
	imagePipeline->AddPlugin(new AutoLinearToneMap());
	imagePipeline->AddPlugin(new GammaCorrectionPlugin(2.2f));

	film->SetImagePipeline(imagePipeline.release());

	//--------------------------------------------------------------------------
	// Add the default output
	//--------------------------------------------------------------------------

	film->Parse(Properties() << 
			Property("film.outputs.0.type")("RGB_TONEMAPPED") <<
			Property("film.outputs.0.filename")("image.png"));

	//--------------------------------------------------------------------------
	// Create the image pipeline, initialize radiance channel scales
	// and film outputs
	//--------------------------------------------------------------------------

	film->Parse(cfg);

	return film.release();
}

SamplerSharedData *RenderConfig::AllocSamplerSharedData(RandomGenerator *rndGen) const {
	const SamplerType samplerType = Sampler::String2SamplerType(GetProperty("sampler.type").Get<string>());
	switch (samplerType) {
		case RANDOM:
			return NULL;
		case METROPOLIS:
			return new MetropolisSamplerSharedData();
		case SOBOL:
			return new SobolSamplerSharedData(rndGen);
		default:
			throw runtime_error("Unknown sampler.type: " + boost::lexical_cast<string>(samplerType));
	}
}

Sampler *RenderConfig::AllocSampler(RandomGenerator *rndGen, Film *film, const FilmSampleSplatter *flmSplatter,
		const u_int threadIndex, const u_int threadCount, SamplerSharedData *sharedData) const {
	const SamplerType samplerType = Sampler::String2SamplerType(GetProperty("sampler.type").Get<string>());
	switch (samplerType) {
		case RANDOM:
			return new RandomSampler(rndGen, film, flmSplatter);
		case METROPOLIS: {
			const float rate = GetProperty("sampler.metropolis.largesteprate").Get<float>();
			const float reject = GetProperty("sampler.metropolis.maxconsecutivereject").Get<float>();
			const float mutationrate = GetProperty("sampler.metropolis.imagemutationrate").Get<float>();

			return new MetropolisSampler(rndGen, film, flmSplatter,
					reject, rate, mutationrate,
					(MetropolisSamplerSharedData *)sharedData);
		}
		case SOBOL:
			return new SobolSampler(rndGen, film, flmSplatter, (SobolSamplerSharedData *)sharedData);
		default:
			throw runtime_error("Unknown sampler.type: " + boost::lexical_cast<string>(samplerType));
	}
}

RenderEngine *RenderConfig::AllocRenderEngine(Film *film, boost::mutex *filmMutex) const {
	const RenderEngineType renderEngineType = RenderEngine::String2RenderEngineType(
		GetProperty("renderengine.type").Get<string>());

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
			throw runtime_error("Unknown render engine type: " + boost::lexical_cast<string>(renderEngineType));
	}
}
