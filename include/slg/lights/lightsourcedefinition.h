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

	void SetLightStrategy(const LightStrategyType type);

	// Update lightGroupCount, envLightSources, intersectableLightSources,
	// lightIndexByMeshIndex and lightStrategyType
	void Preprocess(const Scene *scene);

	bool IsLightSourceDefined(const std::string &name) const {
		return (lightsByName.count(name) > 0);
	}
	void DefineLightSource(const std::string &name, LightSource *l);

	const LightSource *GetLightSource(const std::string &name) const;
	LightSource *GetLightSource(const std::string &name);
	const LightSource *GetLightSource(const u_int index) const { return lights[index]; }
	LightSource *GetLightSource(const u_int index) { return lights[index]; }
	u_int GetLightSourceIndex(const std::string &name) const;
	u_int GetLightSourceIndex(const LightSource *l) const;
	const LightSource *GetLightByType(const LightSourceType type) const;
	const TriangleLight *GetLightSourceByMeshIndex(const u_int index) const;

	u_int GetSize() const { return static_cast<u_int>(lights.size()); }
	std::vector<std::string> GetLightSourceNames() const;

	// Update any reference to oldMat with newMat
	void UpdateMaterialReferences(const Material *oldMat, const Material *newMat);

	void DeleteLightSource(const std::string &name);
	void DeleteLightSourceStartWith(const std::string &namePrefix);
  
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
	const LightStrategy *GetLightStrategy() const { return lightStrategy; }

private:
	std::vector<LightSource *> lights;
	boost::unordered_map<std::string, LightSource *> lightsByName;
	vector<u_int> lightTypeCount;

	LightStrategy *lightStrategy;

	//--------------------------------------------------------------------------
	// Following fields are updated with Preprocess() method
	//--------------------------------------------------------------------------

	u_int lightGroupCount;

	std::vector<u_int> lightIndexByMeshIndex;

	// Only intersectable light sources
	std::vector<TriangleLight *> intersectableLightSources;
	// Only env. light sources (i.e. sky, sun and infinite light, etc.)
	std::vector<EnvLightSource *> envLightSources;
};

}

#endif	/* _SLG_LIGHTSOURCEDEFINITIONS_H */
