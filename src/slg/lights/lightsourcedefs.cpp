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

#include <boost/algorithm/string/predicate.hpp>

#include "slg/scene/scene.h"
#include "slg/lights/trianglelight.h"
#include "slg/lights/strategies/logpower.h"

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
	for (auto const &e : lightsByName)
		delete e.second;
}

void LightSourceDefinitions::DefineLightSource(LightSource *newLight) {
	const string &name = newLight->GetName();

	if (IsLightSourceDefined(name)) {
		const LightSource *oldLight = GetLightSource(name);

		// Update name/LightSource definition
		lightsByName.erase(name);
		lightsByName[name] = newLight;

		// Delete old LightSource
		delete oldLight;
	} else {
		// Add the new LightSource
		lightsByName[name] = newLight;
	}
}

bool LightSourceDefinitions::IsLightSourceDefined(const std::string &name) const {
	return (lightsByName.count(name) > 0);
}

const LightSource *LightSourceDefinitions::GetLightSource(const string &name) const {
	// Check if the LightSource has been already defined
	auto e = lightsByName.find(name);

	if (e == lightsByName.end())
		throw runtime_error("Reference to an undefined LightSource in LightSourceDefinitions::GetLightSource(): " + name);
	else
		return e->second;
}

LightSource *LightSourceDefinitions::GetLightSource(const string &name) {
	// Check if the LightSource has been already defined
	auto e = lightsByName.find(name);

	if (e == lightsByName.end())
		throw runtime_error("Reference to an undefined LightSource in LightSourceDefinitions::GetLightSource(): " + name);
	else
		return e->second;
}

const TriangleLight *LightSourceDefinitions::GetLightSourceByMeshAndTriIndex(const u_int meshIndex, const u_int triIndex) const {
	const u_int offset = lightIndexOffsetByMeshIndex[meshIndex];
	const u_int lightIndex = lightIndexByTriIndex[offset + triIndex];

	return (const TriangleLight *)lights[lightIndex];
}

vector<string> LightSourceDefinitions::GetLightSourceNames() const {
	vector<string> names;
	names.reserve(lights.size());
	for (auto const &e : lightsByName)
		names.push_back(e.first);

	return names;
}

void LightSourceDefinitions::DeleteLightSource(const string &name) {
	auto e = lightsByName.find(name);

	if (e == lightsByName.end())
		throw runtime_error("Reference to an undefined LightSource in LightSourceDefinitions::DeleteLightSource(): " + name);
	else {
		delete e->second;
		lightsByName.erase(name);
	}
}

void LightSourceDefinitions::DeleteLightSourceStartWith(const string &namePrefix) {
	// Build the list of lights to delete
	vector<const string *> nameList;
	for (auto const &e : lightsByName) {
		const string &name = e.first;

		if (boost::starts_with(name, namePrefix))
			nameList.push_back(&name);
	}

	for (auto const name : nameList)
		DeleteLightSource(*name);
}

void LightSourceDefinitions::DeleteLightSourceByMaterial(const Material *mat) {
	// Build the list of lights to delete
	vector<const string *> nameList;
	for (auto const &e : lightsByName) {
		const string &name = e.first;
		const LightSource *l = e.second;

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

void LightSourceDefinitions::Preprocess(const Scene *scene, const bool useRTMode) {
	// Update lightGroupCount, envLightSources, intersectableLightSources,
	// lightIndexOffsetByMeshIndex, lightsDistribution, etc.

	const double start = WallClockTime();
	
	lightGroupCount = 0;
	lights.clear();
	lights.resize(lightsByName.size());
	intersectableLightSources.clear();
	envLightSources.clear();
	fill(lightTypeCount.begin(), lightTypeCount.end(), 0);
	// To accelerate the light pointer to light index lookup
	robin_hood::unordered_flat_map<const LightSource *, u_int> light2indexLookupAccel;
	
	u_int i = 0;
	for (auto const &e : lightsByName) {
		LightSource *l = e.second;

		// Initialize the light source index
		l->lightSceneIndex = i;
		light2indexLookupAccel[l] = i;

		// Update the light group count
		lightGroupCount = Max(lightGroupCount, l->GetID() + 1);

		// Update the light type count
		++lightTypeCount[l->GetType()];

		// Update the list of all lights
		lights[i] = l;

		// Update the list of env. lights
		if (l->IsEnvironmental())
			envLightSources.push_back((EnvLightSource *)l);

		TriangleLight *tl = dynamic_cast<TriangleLight *>(l);
		if (tl) {
			intersectableLightSources.push_back(tl);

			tl->meshIndex = scene->objDefs.GetSceneObjectIndex(tl->sceneObject);
		}

		++i;
	}

	const double end1 = WallClockTime();
	SLG_LOG("Light step #1 preprocessing time: " << (end1 - start) << "secs");

	for(u_int i = 0; i < lights.size(); ++i) {
		TriangleLight *tl = dynamic_cast<TriangleLight *>(lights[i]);
		if (tl) {
			intersectableLightSources.push_back(tl);

			tl->meshIndex = scene->objDefs.GetSceneObjectIndex(tl->sceneObject);
		}
	}

	const double end2 = WallClockTime();
	SLG_LOG("Light step #2 preprocessing time: " << (end2 - end1) << "secs");

	// Build 2 tables to go from mesh index and triangle index to light index
	lightIndexOffsetByMeshIndex.resize(scene->objDefs.GetSize());
	lightIndexByTriIndex.clear();
	for (u_int meshIndex = 0; meshIndex < scene->objDefs.GetSize(); ++meshIndex) {
		const SceneObject *so = scene->objDefs.GetSceneObject(meshIndex);

		if (so->GetMaterial()->IsLightSource()) {
			lightIndexOffsetByMeshIndex[meshIndex] = lightIndexByTriIndex.size();

			const ExtMesh *mesh = so->GetExtMesh();
			for (u_int triIndex = 0; triIndex < mesh->GetTotalTriangleCount(); ++triIndex) {
				const string lightName = Scene::EncodeTriangleLightNamePrefix(so->GetName()) + ToString(triIndex);

				lightIndexByTriIndex.push_back(light2indexLookupAccel[GetLightSource(lightName)]);
			}
		} else
			lightIndexOffsetByMeshIndex[meshIndex] = NULL_INDEX;
	}
	
	const double end3 = WallClockTime();
	SLG_LOG("Light step #3 preprocessing time: " << (end3 - end2) << "secs");

	// I need to check all volume definitions for radiance group usage too
	for (u_int i = 0; i < scene->matDefs.GetSize(); ++i) {
		const Material *mat = scene->matDefs.GetMaterial(i);

		const Volume *vol = dynamic_cast<const Volume *>(mat);
		if (vol && vol->GetVolumeEmissionTexture()) {
			// Update the light group count
			lightGroupCount = Max(lightGroupCount, vol->GetVolumeLightID() + 1);
		}
	}
	//SLG_LOG("Radiance group count: " << lightGroupCount);

	const double end4 = WallClockTime();
	SLG_LOG("Light step #4 preprocessing time: " << (end4 - end3) << "secs");

	// Build the light strategy
	emitLightStrategy->Preprocess(scene, TASK_EMIT, useRTMode);
	illuminateLightStrategy->Preprocess(scene, TASK_ILLUMINATE, useRTMode);
	infiniteLightStrategy->Preprocess(scene, TASK_INFINITE_ONLY, useRTMode);

	const double end5 = WallClockTime();
	SLG_LOG("Light step #5 preprocessing time: " << (end5 - end4) << "secs");

	const double endTotal = WallClockTime();
	SLG_LOG("Light total preprocessing time: " << (endTotal - start) << "secs");
}

void LightSourceDefinitions::UpdateVisibilityMaps(const Scene *scene, const bool useRTMode) {
	// Build visibility maps for Env. lights
	for (EnvLightSource *envLight : GetEnvLightSources())
		envLight->UpdateVisibilityMap(scene, useRTMode);
}
