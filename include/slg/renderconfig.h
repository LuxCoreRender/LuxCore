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

#ifndef _SLG_RENDERCONFIG_H
#define	_SLG_RENDERCONFIG_H

#include "luxrays/core/randomgen.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/slg.h"
#include "slg/samplers/sampler.h"
#include "slg/scene/scene.h"

namespace slg {

class RenderConfig {
public:
	RenderConfig(const luxrays::Properties &props, Scene *scene = NULL);
	~RenderConfig();

	bool HasCachedKernels();

	const luxrays::Property GetProperty(const std::string &name) const;

	void Parse(const luxrays::Properties &props);
	void DeleteAllFilmImagePipelinesProperties();
	void UpdateFilmProperties(const luxrays::Properties &props);
	void Delete(const std::string &prefix);

	Filter *AllocPixelFilter() const;
	Film *AllocFilm() const;

	SamplerSharedData *AllocSamplerSharedData(luxrays::RandomGenerator *rndGen, Film *film) const;
	Sampler *AllocSampler(luxrays::RandomGenerator *rndGen, Film *film,
		const FilmSampleSplatter *flmSplatter,
		SamplerSharedData *sharedData,
		const luxrays::Properties &additionalProps) const;

	RenderEngine *AllocRenderEngine() const;

	const luxrays::Properties &ToProperties() const;

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static const luxrays::Properties &GetDefaultProperties();

	static RenderConfig *LoadSerialized(const std::string &fileName);
	static void SaveSerialized(const std::string &fileName, const RenderConfig *renderConfig);
	static void SaveSerialized(const std::string &fileName, const RenderConfig *renderConfig,
		const luxrays::Properties &additionalCfg);

	luxrays::Properties cfg;
	Scene *scene;

	friend class boost::serialization::access;

private:
	// Used by serialization
	RenderConfig() {
		allocatedScene = false;
	}

	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	static void InitDefaultProperties();

	mutable luxrays::Properties propsCache;
	// This is a temporary field used to exchange data between SaveSerialized())
	// and save()
	mutable luxrays::Properties saveAdditionalCfg;

	bool allocatedScene;
};

}

BOOST_CLASS_VERSION(slg::RenderConfig, 2)

BOOST_CLASS_EXPORT_KEY(slg::RenderConfig)

#endif	/* _SLG_RENDERCONFIG_H */
