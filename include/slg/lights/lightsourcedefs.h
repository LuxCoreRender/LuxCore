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

#ifndef _SLG_LIGHTSOURCEDEFINITIONS_H
#define	_SLG_LIGHTSOURCEDEFINITIONS_H

#include <functional>
#include <robin_hood.h>

#include "luxrays/utils/properties.h"
#include "slg/lights/light.h"
#include "slg/lights/trianglelight.h"
#include "slg/lights/strategies/lightstrategy.h"
#include "slg/usings.h"

namespace slg {

//------------------------------------------------------------------------------
// LightSourceDefinitions
//------------------------------------------------------------------------------

class TriangleLight;
class Scene;

class LightSourceDefinitions {
public:
	LightSourceDefinitions();
	~LightSourceDefinitions();

	void SetLightStrategy(const luxrays::Properties &props);

	void UpdateVisibilityMaps(SceneConstRef scene, const bool useRTMode);

	LightSourceUPtr DefineLightSource(LightSourceUPtr&& l);
	bool IsLightSourceDefined(const std::string &name) const;

	LightSourceRef GetLightSource(const std::string &name);
	LightSourceConstRef GetLightSource(const std::string &name) const;
	LightSourcePtr GetLightSourcePtr(const std::string &name);
	LightSourceConstPtr GetLightSourcePtr(const std::string &name) const;

	u_int GetSize() const { return static_cast<u_int>(lightsByName.size()); }
	std::vector<std::string> GetLightSourceNames() const;

	LightSourceUPtr DeleteLightSource(const std::string &name);
	std::vector<LightSourceUPtr> DeleteLightSourceStartWith(const std::string &namePrefix);
	std::vector<LightSourceUPtr> DeleteLightSourceByMaterial(MaterialConstRef mat);

	void UpdateVolumeReferences(VolumeConstRef oldVol, VolumeRef newVol);

	//--------------------------------------------------------------------------
	// Following methods require Preprocess()
	//--------------------------------------------------------------------------

	TriangleLightConstRef GetLightSourceByMeshAndTriIndex(const u_int meshIndex, const u_int triIndex) const;

	u_int GetLightGroupCount() const { return lightGroupCount; }
	const u_int GetLightTypeCount(const LightSourceType type) const { return lightTypeCount[type]; }
	const std::vector<u_int> &GetLightTypeCounts() const { return lightTypeCount; }

	LightSourceRef GetLightSource(size_t n) const { return lights[n]; }
	LightSourcePtr GetLightSourcePtr(size_t n) const {
		return LightSourcePtr(std::addressof(lights[n].get()));
	}

	auto ViewEnvLightSources() {
		// Returns a view of references to the objects
		return envLightSources |
			std::views::transform([](const auto& obj) -> EnvLightSourceRef {
			return obj.get();
		});
	}
	auto ViewEnvLightSources() const {
		// Returns a view of references to the objects
		return envLightSources |
			std::views::transform([](const auto& obj) -> const EnvLightSourceRef {
			return obj.get();
		});
	}
	auto GetEnvLightSources() const {
		auto view = ViewEnvLightSources();
		return std::vector<std::reference_wrapper<EnvLightSource>>(view.begin(), view.end());
	}

	auto ViewIntersectableLightSources() const {
		// Returns a view of references to the objects
		return intersectableLightSources |
			std::views::transform([](const auto& obj) -> const TriangleLightRef {
			return obj.get();
		});
	}
	auto GetIntersectableLightSources() const {
		auto view = ViewIntersectableLightSources();
		return std::vector<std::reference_wrapper<IntersectableLightSource>>(view.begin(), view.end());
	}

	const std::vector<u_int> &GetLightIndexOffsetByMeshIndex() const {
		return lightIndexOffsetByMeshIndex;
	}
	const std::vector<u_int> &GetLightIndexByTriIndex() const {
		return lightIndexByTriIndex;
	}
	LightStrategyConstRef GetEmitLightStrategy() const { return *emitLightStrategy; }
	LightStrategyConstRef GetIlluminateLightStrategy() const {
		return *illuminateLightStrategy;
	}
	LightStrategyConstRef GetInfiniteLightStrategy() const {
		return *infiniteLightStrategy;
	}

	friend class Scene;

private:
	// Update lightGroupCount, envLightSources, intersectableLightSources,
	// lightIndexOffsetByMeshIndex, lightStrategyType, etc.
	// This is called by Scene::Preprocess()
	void Preprocess(SceneConstRef scene, const bool useRTMode);


	//--------------------------------------------------------------------------
	// Following fields are updated with Preprocess() method
	//--------------------------------------------------------------------------

	u_int lightGroupCount;

	std::vector<u_int> lightTypeCount;

	// Below, the actual owner of Light objects
	robin_hood::unordered_flat_map<std::string, LightSourceUPtr> lightsByName;

	// Following containers just store references to lights
	std::vector<std::reference_wrapper<LightSource>> lights;
	// Only intersectable light sources
	std::vector<std::reference_wrapper<TriangleLight>> intersectableLightSources;
	// Only env. light sources (i.e. sky, sun and infinite light, etc.)
	std::vector<std::reference_wrapper<EnvLightSource>> envLightSources;

	// 2 tables to go from mesh index and triangle index to light index
	std::vector<u_int> lightIndexOffsetByMeshIndex;
	std::vector<u_int> lightIndexByTriIndex;

	// The light strategies
	LightStrategyUPtr
		emitLightStrategy, illuminateLightStrategy, infiniteLightStrategy;
};

}

#endif	/* _SLG_LIGHTSOURCEDEFINITIONS_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
