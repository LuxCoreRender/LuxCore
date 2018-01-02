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

#ifndef _SLG_RENDERCONFIG_H
#define	_SLG_RENDERCONFIG_H

#include <boost/thread/mutex.hpp>

#include <boost/serialization/version.hpp>
#include <boost/serialization/export.hpp>

#include "luxrays/core/randomgen.h"
#include "luxrays/utils/properties.h"
#include "slg/slg.h"
#include "slg/samplers/sampler.h"
#include "slg/scene/scene.h"

namespace slg {

class RenderConfig {
public:
	RenderConfig(const luxrays::Properties &props, Scene *scene = NULL);
	~RenderConfig();

	const luxrays::Property GetProperty(const std::string &name) const;

	void Parse(const luxrays::Properties &props);
	void UpdateFilmProperties(const luxrays::Properties &props);
	void Delete(const std::string &prefix);

	Filter *AllocPixelFilter() const;
	Film *AllocFilm() const;

	SamplerSharedData *AllocSamplerSharedData(luxrays::RandomGenerator *rndGen, Film *film) const;
	Sampler *AllocSampler(luxrays::RandomGenerator *rndGen, Film *film,
		const FilmSampleSplatter *flmSplatter,
		SamplerSharedData *sharedData) const;

	RenderEngine *AllocRenderEngine(Film *film, boost::mutex *filmMutex) const;

	const luxrays::Properties &ToProperties() const;

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static const luxrays::Properties &GetDefaultProperties();

	static RenderConfig *LoadSerialized(const std::string &fileName);
	static void SaveSerialized(const std::string &fileName, const RenderConfig *renderConfig);

	luxrays::Properties cfg;
	Scene *scene;

	friend class boost::serialization::access;

private:
	// Used by serialization
	RenderConfig() {
		allocatedScene = false;
	}

	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & cfg;
		ar & scene;
		ar & enableParsePrint;
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		// In case there is an error while reading the archive
		scene = NULL;
		allocatedScene = true;

		ar & cfg;
		ar & scene;
		ar & enableParsePrint;

		// Reset the properties cache
		propsCache.Clear();
	}

	BOOST_SERIALIZATION_SPLIT_MEMBER()

	static void InitDefaultProperties();

	mutable luxrays::Properties propsCache;

	bool allocatedScene, enableParsePrint;
};

}

BOOST_CLASS_VERSION(slg::RenderConfig, 1)

BOOST_CLASS_EXPORT_KEY(slg::RenderConfig)

#endif	/* _SLG_RENDERCONFIG_H */
