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
#include "slg/engines/renderengine.h"
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
#include "slg/lights/lightstrategyregistry.h"

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
			Properties *props = new Properties();
			*props << RenderConfig::ToProperties(Properties());

			defaultProperties.reset(props);
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
	return ToProperties().Get(name);
}

void RenderConfig::Parse(const Properties &props) {
	// Reset the properties cache
	propsCache.Clear();

	cfg.Set(props);
	UpdateFilmProperties(props);

	// Scene epsilon is read directly from the cfg properties inside
	// render engine Start() method

	// Accelerator settings are read directly from the cfg properties inside
	// the render engine

	// Light strategy
	if (LightStrategy::GetType(cfg) != scene->lightDefs.GetLightStrategy()->GetType())
		scene->lightDefs.SetLightStrategy(LightStrategy::FromProperties(cfg));

	// Update the Camera
	u_int filmFullWidth, filmFullHeight, filmSubRegion[4];
	u_int *subRegion = GetFilmSize(&filmFullWidth, &filmFullHeight, filmSubRegion) ?
		filmSubRegion : NULL;
	scene->camera->Update(filmFullWidth, filmFullHeight, subRegion);
}

void RenderConfig::UpdateFilmProperties(const luxrays::Properties &props) {
	//--------------------------------------------------------------------------
	// Check if there was a new image pipeline definition
	//--------------------------------------------------------------------------

	if (props.HaveNames("film.imagepipeline.")) {
		// Delete the old image pipeline properties
		cfg.DeleteAll(cfg.GetAllNames("film.imagepipeline."));

		// Update the RenderConfig properties with the new image pipeline definition
		BOOST_FOREACH(string propName, props.GetAllNames()) {
			if (boost::starts_with(propName, "film.imagepipeline."))
				cfg.Set(props.Get(propName));
		}
		
		// Reset the properties cache
		propsCache.Clear();
	}

	//--------------------------------------------------------------------------
	// Check if there were new radiance groups scale
	//--------------------------------------------------------------------------

	if (props.HaveNames("film.radiancescales.")) {
		// Delete old radiance groups scale properties
		cfg.DeleteAll(cfg.GetAllNames("film.radiancescales."));
		
		// Update the RenderConfig properties with the new radiance groups scale properties
		BOOST_FOREACH(string propName, props.GetAllNames()) {
			if (boost::starts_with(propName, "film.radiancescales."))
				cfg.Set(props.Get(propName));
		}

		// Reset the properties cache
		propsCache.Clear();
	}

	//--------------------------------------------------------------------------
	// Check if there were new outputs definition
	//--------------------------------------------------------------------------

	if (props.HaveNames("film.outputs.")) {
		// Delete old radiance groups scale properties
		cfg.DeleteAll(cfg.GetAllNames("film.outputs."));
		
		// Update the RenderConfig properties with the new outputs definition properties
		BOOST_FOREACH(string propName, props.GetAllNames()) {
			if (boost::starts_with(propName, "film.outputs."))
				cfg.Set(props.Get(propName));
		}

		// Reset the properties cache
		propsCache.Clear();
	}
}

void RenderConfig::Delete(const string &prefix) {
	// Reset the properties cache
	propsCache.Clear();

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
	return Filter::FromProperties(cfg);
}

Film *RenderConfig::AllocFilm() const {
	//--------------------------------------------------------------------------
	// Create the Film
	//--------------------------------------------------------------------------

	u_int filmFullWidth, filmFullHeight, filmSubRegion[4];
	const bool filmSubRegionUsed = GetFilmSize(&filmFullWidth, &filmFullHeight, filmSubRegion);

	SLG_LOG("Film resolution: " << filmFullWidth << "x" << filmFullHeight);
	if (filmSubRegionUsed)
		SLG_LOG("Film sub-region: " << filmSubRegion[0] << " " << filmSubRegion[1] << filmSubRegion[2] << " " << filmSubRegion[3]);
	auto_ptr<Film> film(new Film(filmFullWidth, filmFullHeight,
			filmSubRegionUsed ? filmSubRegion : NULL));

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
	return SamplerSharedData::FromProperties(cfg, rndGen);
}

Sampler *RenderConfig::AllocSampler(RandomGenerator *rndGen, Film *film, const FilmSampleSplatter *flmSplatter,
		SamplerSharedData *sharedData) const {
	return Sampler::FromProperties(cfg, rndGen, film, flmSplatter, sharedData);
}

RenderEngine *RenderConfig::AllocRenderEngine(Film *film, boost::mutex *filmMutex) const {
	return RenderEngine::FromProperties(this, film, filmMutex);
}

const Properties &RenderConfig::ToProperties() const {
	if (!propsCache.GetSize())
		propsCache = ToProperties(cfg);

	return propsCache;
}

Properties RenderConfig::ToProperties(const Properties &cfg) {
	Properties props;

	// Ray intersection accelerators
	props << cfg.Get(Property("accelerator.type")("AUTO"));
	props << cfg.Get(Property("accelerator.instances.enable")(true));

	// Scene epsilon
	props << cfg.Get(Property("scene.epsilon.min")(DEFAULT_EPSILON_MIN));
	props << cfg.Get(Property("scene.epsilon.max")(DEFAULT_EPSILON_MAX));

	props << cfg.Get(Property("scene.file")("scenes/luxball/luxball.scn"));
	props << cfg.Get(Property("images.scale")(1.f));

	// LightStrategy
	props << LightStrategy::ToProperties(cfg);

	// RenderEngine (includes PixelFilter and Sampler where applicable)
	props << RenderEngine::ToProperties(cfg);

	// Film
	props << Film::ToProperties(cfg);

	// This property isn't really used by LuxCore but is useful for GUIs.
	props << cfg.Get(Property("screen.refresh.interval")(100u));

	props << cfg.Get(Property("screen.tiles.pending.show")(true));
	props << cfg.Get(Property("screen.tiles.converged.show")(false));
	props << cfg.Get(Property("screen.tiles.notconverged.show")(false));

	props << cfg.Get(Property("screen.tiles.passcount.show")(false));
	props << cfg.Get(Property("screen.tiles.error.show")(false));

	// The following properties aren't really used by LuxCore but they are useful for
	// applications using LuxCore
	props << cfg.Get(Property("batch.halttime")(0u));
	props << cfg.Get(Property("batch.haltspp")(0u));
	props << cfg.Get(Property("batch.haltthreshold")(-1.f));
	props << cfg.Get(Property("batch.haltdebug")(0u));

	return props;
}
