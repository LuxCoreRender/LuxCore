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

#include <boost/algorithm/string/predicate.hpp>

#include "slg/scene/scene.h"
#include "slg/lights/trianglelight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightSourceDefinitions
//------------------------------------------------------------------------------

LightSourceDefinitions::LightSourceDefinitions() : lightTypeCount(LIGHT_SOURCE_TYPE_COUNT, 0) {
	emitLightStrategy = new LightStrategyLogPower();
	illuminateLightStrategy = new LightStrategyLogPower();
	infiniteLightStrategy = new LightStrategyLogPower();
	lightGroupCount = 1;
}

LightSourceDefinitions::~LightSourceDefinitions() {
	delete emitLightStrategy;
	delete illuminateLightStrategy;
	delete infiniteLightStrategy;
	for (boost::unordered_map<std::string, LightSource *>::const_iterator it = lightsByName.begin(); it != lightsByName.end(); ++it)
		delete it->second;
}

void LightSourceDefinitions::DefineLightSource(LightSource *newLight) {
	const string &name = newLight->GetName();

	if (IsLightSourceDefined(name)) {
		const LightSource *oldLight = GetLightSource(name);

		// Update name/LightSource definition
		lightsByName.erase(name);
		lightsByName.insert(std::make_pair(name, newLight));

		// Delete old LightSource
		delete oldLight;
	} else {
		// Add the new LightSource
		lightsByName.insert(std::make_pair(name, newLight));
	}
}

bool LightSourceDefinitions::IsLightSourceDefined(const std::string &name) const {
	return (lightsByName.count(name) > 0);
}

const LightSource *LightSourceDefinitions::GetLightSource(const string &name) const {
	// Check if the LightSource has been already defined
	boost::unordered_map<std::string, LightSource *>::const_iterator it = lightsByName.find(name);

	if (it == lightsByName.end())
		throw runtime_error("Reference to an undefined LightSource in LightSourceDefinitions::GetLightSource(): " + name);
	else
		return it->second;
}

LightSource *LightSourceDefinitions::GetLightSource(const string &name) {
	// Check if the LightSource has been already defined
	boost::unordered_map<std::string, LightSource *>::const_iterator it = lightsByName.find(name);

	if (it == lightsByName.end())
		throw runtime_error("Reference to an undefined LightSource in LightSourceDefinitions::GetLightSource(): " + name);
	else
		return it->second;
}

const TriangleLight *LightSourceDefinitions::GetLightSourceByMeshIndex(const u_int index) const {
	return (const TriangleLight *)lights[lightIndexByMeshIndex[index]];
}

vector<string> LightSourceDefinitions::GetLightSourceNames() const {
	vector<string> names;
	names.reserve(lights.size());
	for (boost::unordered_map<std::string, LightSource *>::const_iterator it = lightsByName.begin(); it != lightsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void LightSourceDefinitions::DeleteLightSource(const string &name) {
	boost::unordered_map<std::string, LightSource *>::const_iterator it = lightsByName.find(name);

	if (it == lightsByName.end())
		throw runtime_error("Reference to an undefined LightSource in LightSourceDefinitions::DeleteLightSource(): " + name);
	else {
		delete it->second;
		lightsByName.erase(name);
	}
}

void LightSourceDefinitions::DeleteLightSourceStartWith(const string &namePrefix) {
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

void LightSourceDefinitions::DeleteLightSourceByMaterial(const Material *mat) {
	// Build the list of lights to delete
	vector<const string *> nameList;
	for(boost::unordered_map<std::string, LightSource *>::const_iterator itr = lightsByName.begin(); itr != lightsByName.end(); ++itr) {
		const string &name = itr->first;
		const LightSource *l = itr->second;

		if ((l->GetType() == TYPE_TRIANGLE) && (((const TriangleLight *)l)->lightMaterial == mat))
			nameList.push_back(&name);
	}

	BOOST_FOREACH(const string *name, nameList)
		DeleteLightSource(*name);
}

void LightSourceDefinitions::SetLightStrategy(const luxrays::Properties &props) {
	if (LightStrategy::GetType(props) != emitLightStrategy->GetType()) {
		delete emitLightStrategy;
		emitLightStrategy = LightStrategy::FromProperties(props);
	}

	if (LightStrategy::GetType(props) != illuminateLightStrategy->GetType()) {
		delete illuminateLightStrategy;
		illuminateLightStrategy = LightStrategy::FromProperties(props);
	}

	if (LightStrategy::GetType(props) != infiniteLightStrategy->GetType()) {
		delete infiniteLightStrategy;
		infiniteLightStrategy = LightStrategy::FromProperties(props);
	}
}

void LightSourceDefinitions::Preprocess(const Scene *scene) {
	// Update lightGroupCount, envLightSources, intersectableLightSources,
	// lightIndexByMeshIndex, lightsDistribution, etc.

	lightGroupCount = 0;
	lights.clear();
	lights.resize(lightsByName.size());
	intersectableLightSources.clear();
	envLightSources.clear();
	fill(lightTypeCount.begin(), lightTypeCount.end(), 0);
	lightIndexByMeshIndex.resize(scene->objDefs.GetSize(), NULL_INDEX);
	u_int i = 0;

	for(boost::unordered_map<std::string, LightSource *>::const_iterator itr = lightsByName.begin(); itr != lightsByName.end(); ++itr) {
		LightSource *l = itr->second;

		// Initialize the light source index
		l->lightSceneIndex = i;

		// Update the light group count
		lightGroupCount = Max(lightGroupCount, l->GetID() + 1);

		// Update the light type count
		++lightTypeCount[l->GetType()];

		// Update the list of all lights
		lights[i] = l;

		// Update the list of env. lights
		if (l->IsEnvironmental())
			envLightSources.push_back((EnvLightSource *)l);

		// Build lightIndexByMeshIndex
		TriangleLight *tl = dynamic_cast<TriangleLight *>(l);
		if (tl) {
			lightIndexByMeshIndex[scene->objDefs.GetSceneObjectIndex(tl->mesh)] = i;
			intersectableLightSources.push_back(tl);
		}

		++i;
	}

	// Build the light strategy
	emitLightStrategy->Preprocess(scene, TASK_EMIT);
	illuminateLightStrategy->Preprocess(scene, TASK_ILLUMINATE);
	infiniteLightStrategy->Preprocess(scene, TASK_INFINITE_ONLY);
}


void LightSourceDefinitions::UpdateVisibilityMaps(const Scene *scene) {
	// Build visibility maps for Env. lights
	BOOST_FOREACH(EnvLightSource *envLight, GetEnvLightSources())
		envLight->UpdateVisibilityMap(scene);
}
