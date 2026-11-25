/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
#include "luxrays/usings.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/slg.h"
#include "slg/samplers/sampler.h"
#include "slg/scene/scene.h"
#include "slg/usings.h"
#include <functional>
#include <memory>

namespace slg {
using luxrays::PropertiesRPtr;
using luxrays::PropertiesUPtr;
using luxrays::PropertiesConstRPtr;
using luxrays::PropertiesConstRef;
using luxrays::PropertiesRef;

class RenderConfig {

	struct Private{ explicit Private() = default; };

public:

	// Factory
	template<typename... Args>
	static RenderConfigUPtr Create(Args... args) {
		return std::make_unique<RenderConfig>(Private(), std::forward<Args>(args)...);
	}

	// Constructors are private, please use factory instead

	// Case #1: External scene provided
	RenderConfig(Private, PropertiesRPtr props, SceneRef scene);

	// Case #2: No external scene provided, will generate an internal one
	RenderConfig(Private, PropertiesRPtr props);

	bool HasCachedKernels();

	const luxrays::Property GetProperty(const std::string &name) const;

	void Parse(const luxrays::Properties &props);
	void DeleteAllFilmImagePipelinesProperties();
	void UpdateFilmProperties(const luxrays::Properties &props);
	void Delete(const std::string &prefix);

	FilterUPtr AllocPixelFilter() const;
	FilmUPtr AllocFilm() const;

	SamplerSharedDataUPtr AllocSamplerSharedData(
		const luxrays::RandomGeneratorUPtr & rndGen,
		FilmRef film
	) const;
	SamplerSharedDataUPtr AllocSamplerSharedData(
		const luxrays::RandomGeneratorUPtr & rndGen,
		FilmPtr film
	) const;

	SamplerUPtr AllocSampler(
		const luxrays::RandomGeneratorUPtr & rndGen,
		FilmRef film,
		FilmSampleSplatterRPtr flmSplatter,
		const std::shared_ptr<SamplerSharedData> sharedData,
		const luxrays::Properties &additionalProps
	) const;
	SamplerUPtr AllocSampler(
		const luxrays::RandomGeneratorUPtr & rndGen,
		FilmPtr film,
		FilmSampleSplatterRPtr flmSplatter,
		const std::shared_ptr<SamplerSharedData> sharedData,
		const luxrays::Properties &additionalProps
	) const;


	RenderEngineUPtr AllocRenderEngine();

	PropertiesRPtr ToProperties() const;

	static luxrays::PropertiesUPtr ToProperties(const luxrays::Properties &cfg);
	static luxrays::PropertiesRPtr GetDefaultProperties();

	static RenderConfigUPtr LoadSerialized(const std::string &fileName);
	static void SaveSerialized(
		const std::string &fileName,
		const RenderConfigUPtr& renderConfig
	);
	static void SaveSerialized(
		const std::string &fileName,
		const RenderConfigUPtr& renderConfig,
		const luxrays::Properties &additionalCfg
	);
	static void SaveSerialized(
		const std::string &fileName,
		RenderConfigConstRef renderConfig,
		const luxrays::Properties &additionalCfg
	);

	// Accessors
	SceneConstRef GetScene() const { return *sceneRef; }
	SceneRef GetScene() { return *sceneRef; }
	PropertiesConstRef GetConfig() const { return *cfg; }
	PropertiesRef GetConfig() { return *cfg; }
	PropertiesRPtr GetConfigPtr() const { return cfg; }


	// Serialization stuff
	friend class boost::serialization::access;

	template<class Archive> static void save_construct_data(
		Archive & ar, const RenderConfig * t, const unsigned int file_version
	);
	template<class Archive> static void load_construct_data(
		Archive & ar, RenderConfig * t, const unsigned int file_version
	);
	template<class Archive> void serialize(Archive & ar, const unsigned int file_version);


private:

	// For deserialization only
	RenderConfig(PropertiesUPtr&& p_cfg, SceneRef p_scn, SceneUPtr&& p_internalscene);

	static void InitDefaultProperties();

	mutable luxrays::PropertiesUPtr propsCache{std::make_unique<luxrays::Properties>()};
	// This is a temporary field used to exchange data between SaveSerialized())
	// and save()
	mutable luxrays::Properties saveAdditionalCfg;

	// RenderConfig owns its configuration
	luxrays::PropertiesUPtr cfg;

	// RenderConfig owns the scene... or not
	// There are 2 cases:
	// - Either a scene is generated outside of RenderConfig and passed as
	//   reference to RenderConfig constructor: in this case, we just set sceneRef
	//   to the external scene
	// - Or no scene is passed to RenderConfig constructor, and in this case
	//   RenderConfig constructor will generate an internal scene, and sceneRef will
	//   be set to reference this internal scene
	SceneUPtr internalScene;
	ScenePtr sceneRef;
};

}  // namespace slg

//------------------------------------------------------------------------------
// Serialization plumbing
//------------------------------------------------------------------------------

namespace boost {
namespace serialization {

// Non-default construction
template<class Archive>
inline void save_construct_data(
    Archive & ar, const slg::RenderConfig * t, const unsigned int file_version
){
	slg::RenderConfig::save_construct_data(ar, t, file_version);
}

template<class Archive>
inline void load_construct_data(
    Archive & ar, slg::RenderConfig * t, const unsigned int file_version
){
	slg::RenderConfig::load_construct_data(ar, t, file_version);
}


} // namespace serialization
} // namespace boost

BOOST_CLASS_VERSION(slg::RenderConfig, 2)

BOOST_CLASS_EXPORT_KEY(slg::RenderConfig)

#endif	/* _SLG_RENDERCONFIG_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
