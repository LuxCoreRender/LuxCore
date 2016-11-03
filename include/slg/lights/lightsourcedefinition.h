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

#ifndef _SLG_LIGHTSOURCEDEFINITIONS_H
#define	_SLG_LIGHTSOURCEDEFINITIONS_H

#include "luxrays/utils/properties.h"
#include "slg/lights/light.h"
#include "slg/lights/lightstrategy.h"

namespace slg {

//------------------------------------------------------------------------------
// LightSourceDefinitions
//------------------------------------------------------------------------------

class TriangleLight;

class LightSourceDefinitions {
public:
	LightSourceDefinitions();
	~LightSourceDefinitions();

	void SetLightStrategy(const luxrays::Properties &props);

	// Update lightGroupCount, envLightSources, intersectableLightSources,
	// lightIndexByMeshIndex, lightStrategyType, etc.
	void Preprocess(const Scene *scene);

	void DefineLightSource(const std::string &name, LightSource *l);
	bool IsLightSourceDefined(const std::string &name) const;

	const LightSource *GetLightSource(const std::string &name) const;
	LightSource *GetLightSource(const std::string &name);

	u_int GetSize() const { return static_cast<u_int>(lightsByName.size()); }
	std::vector<std::string> GetLightSourceNames() const;

	void DeleteLightSource(const std::string &name);
	void DeleteLightSourceStartWith(const std::string &namePrefix);
	void DeleteLightSourceByMaterial(const Material *mat);

	//--------------------------------------------------------------------------
	// Following methods require Preprocess()
	//--------------------------------------------------------------------------

	const TriangleLight *GetLightSourceByMeshIndex(const u_int index) const;
 
	u_int GetLightGroupCount() const { return lightGroupCount; }
	const u_int GetLightTypeCount(const LightSourceType type) const { return lightTypeCount[type]; }
	const vector<u_int> &GetLightTypeCounts() const { return lightTypeCount; }

	const std::vector<LightSource *> &GetLightSources() const {
		return lights;
	}
	const std::vector<EnvLightSource *> &GetEnvLightSources() const {
		return envLightSources;
	}
	const std::vector<TriangleLight *> &GetIntersectableLightSources() const {
		return intersectableLightSources;
	}
	const std::vector<u_int> &GetLightIndexByMeshIndex() const { return lightIndexByMeshIndex; }
	const LightStrategy *GetEmitLightStrategy() const { return emitLightStrategy; }
	const LightStrategy *GetIlluminateLightStrategy() const { return illuminateLightStrategy; }
	const LightStrategy *GetInfiniteLightStrategy() const { return infiniteLightStrategy; }

private:
	boost::unordered_map<std::string, LightSource *> lightsByName;

	//--------------------------------------------------------------------------
	// Following fields are updated with Preprocess() method
	//--------------------------------------------------------------------------

	u_int lightGroupCount;

	std::vector<u_int> lightTypeCount;

	std::vector<LightSource *> lights;
	// Only intersectable light sources
	std::vector<TriangleLight *> intersectableLightSources;
	// Only env. light sources (i.e. sky, sun and infinite light, etc.)
	std::vector<EnvLightSource *> envLightSources;

	std::vector<u_int> lightIndexByMeshIndex;

	LightStrategy *emitLightStrategy;
	LightStrategy *illuminateLightStrategy;
	LightStrategy *infiniteLightStrategy;
};

}

#endif	/* _SLG_LIGHTSOURCEDEFINITIONS_H */
