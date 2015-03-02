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

#include <boost/algorithm/string/predicate.hpp>

#include "slg/lights/lightsourcedefinition.h"
#include "slg/lights/trianglelight.h"
#include "slg/sdl/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightSourceDefinitions
//------------------------------------------------------------------------------

LightSourceDefinitions::LightSourceDefinitions() : lightTypeCount(LIGHT_SOURCE_TYPE_COUNT, 0) {
	lightStrategy = new LightStrategyPower();
	lightGroupCount = 1;
}

LightSourceDefinitions::~LightSourceDefinitions() {
	delete lightStrategy;
	BOOST_FOREACH(LightSource *l, lights)
		delete l;
}

void LightSourceDefinitions::DefineLightSource(const std::string &name, LightSource *newLight) {
	if (IsLightSourceDefined(name)) {
		const LightSource *oldLight = GetLightSource(name);

		// Update name/LightSource definition
		const u_int index = GetLightSourceIndex(name);
		lights[index] = newLight;
		lightsByName.erase(name);
		--lightTypeCount[oldLight->GetType()];
		lightsByName.insert(std::make_pair(name, newLight));
		++lightTypeCount[newLight->GetType()];

		// Delete old LightSource
		delete oldLight;
	} else {
		// Add the new LightSource
		lights.push_back(newLight);
		lightsByName.insert(std::make_pair(name, newLight));
		++lightTypeCount[newLight->GetType()];
	}
}

const LightSource *LightSourceDefinitions::GetLightSource(const std::string &name) const {
	// Check if the LightSource has been already defined
	boost::unordered_map<std::string, LightSource *>::const_iterator it = lightsByName.find(name);

	if (it == lightsByName.end())
		throw std::runtime_error("Reference to an undefined LightSource: " + name);
	else
		return it->second;
}

LightSource *LightSourceDefinitions::GetLightSource(const std::string &name) {
	// Check if the LightSource has been already defined
	boost::unordered_map<std::string, LightSource *>::const_iterator it = lightsByName.find(name);

	if (it == lightsByName.end())
		throw std::runtime_error("Reference to an undefined LightSource: " + name);
	else
		return it->second;
}

u_int LightSourceDefinitions::GetLightSourceIndex(const std::string &name) const {
	return GetLightSourceIndex(GetLightSource(name));
}

u_int LightSourceDefinitions::GetLightSourceIndex(const LightSource *m) const {
	for (u_int i = 0; i < lights.size(); ++i) {
		if (m == lights[i])
			return i;
	}

	throw std::runtime_error("Reference to an undefined LightSource: " + boost::lexical_cast<std::string>(m));
}

const LightSource *LightSourceDefinitions::GetLightByType(const LightSourceType type) const {
	BOOST_FOREACH(LightSource *l, lights) {
		if (l->GetType() == type)
			return l;
	}

	return NULL;
}

const TriangleLight *LightSourceDefinitions::GetLightSourceByMeshIndex(const u_int index) const {
	return (const TriangleLight *)lights[lightIndexByMeshIndex[index]];
}

std::vector<std::string> LightSourceDefinitions::GetLightSourceNames() const {
	std::vector<std::string> names;
	names.reserve(lights.size());
	for (boost::unordered_map<std::string, LightSource *>::const_iterator it = lightsByName.begin(); it != lightsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void LightSourceDefinitions::UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
	// Replace old material direct references with new ones
	BOOST_FOREACH(LightSource *l, lights) {
		TriangleLight *tl = dynamic_cast<TriangleLight *>(l);
		if (tl)
			tl->UpdateMaterialReferences(oldMat, newMat);
	}
}

void LightSourceDefinitions::DeleteLightSource(const std::string &name) {
	const u_int index = GetLightSourceIndex(name);
	--lightTypeCount[lights[index]->GetType()];
	delete lights[index];

	lights.erase(lights.begin() + index);
	lightsByName.erase(name);
}

void LightSourceDefinitions::DeleteLightSourceStartWith(const std::string &namePrefix) {
	// Build the list of lights to delete
	vector<const string *> nameList;
	for(boost::unordered_map<std::string, LightSource *>::const_iterator itr = lightsByName.begin(); itr != lightsByName.end(); ++itr) {
		const string &name = itr->first;

		if (boost::starts_with(name, namePrefix))
			nameList.push_back(&name);
	}
	
	BOOST_FOREACH(const string *name, nameList)
		DeleteLightSource(*name);
}

void LightSourceDefinitions::SetLightStrategy(const LightStrategyType type) {
	if (lightStrategy && (lightStrategy->GetType() == type))
		return;

	delete lightStrategy;
	lightStrategy = NULL;

	switch (type) {
		case TYPE_UNIFORM:
			lightStrategy = new LightStrategyUniform();
			break;
		case TYPE_POWER:
			lightStrategy = new LightStrategyPower();
			break;
		case TYPE_LOG_POWER:
			lightStrategy = new LightStrategyLogPower();
			break;
		default:
			throw runtime_error("Unknown LightStrategyType in LightSourceDefinitions::SetLightStrategy(): " + ToString(type));
	}
}

void LightSourceDefinitions::Preprocess(const Scene *scene) {
	// Update lightGroupCount, envLightSources, intersectableLightSources,
	// lightIndexByMeshIndex and lightsDistribution

	lightGroupCount = 0;
	intersectableLightSources.clear();
	envLightSources.clear();

	lightIndexByMeshIndex.resize(scene->objDefs.GetSize(), NULL_INDEX);
	for (u_int i = 0; i < lights.size(); ++i) {
		LightSource *l = lights[i];
		// Initialize the light source index
		l->lightSceneIndex = i;

		lightGroupCount = Max(lightGroupCount, l->GetID() + 1);

		// Update the list of env. lights
		if (l->IsEnvironmental())
			envLightSources.push_back((EnvLightSource *)l);

		// Build lightIndexByMeshIndex
		TriangleLight *tl = dynamic_cast<TriangleLight *>(l);
		if (tl) {
			lightIndexByMeshIndex[scene->objDefs.GetSceneObjectIndex(tl->mesh)] = i;
			intersectableLightSources.push_back(tl);
		}
	}

	// Build the light strategy
	lightStrategy->Preprocess(scene);
}
