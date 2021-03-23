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

#include <memory>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include "luxrays/utils/serializationutils.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Scene serialization
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::Scene)

Scene *Scene::LoadSerialized(const std::string &fileName) {
	SerializationInputFile<LuxInputBinArchive> sif(fileName);

	Scene *scene;
	sif.GetArchive() >> scene;

	if (!sif.IsGood())
		throw runtime_error("Error while loading serialized scene: " + fileName);

	return scene;
}

void Scene::SaveSerialized(const std::string &fileName, const Scene *scene) {
	SerializationOutputFile<LuxOutputBinArchive> sof(fileName);

	sof.GetArchive() << scene;

	if (!sof.IsGood())
		throw runtime_error("Error while saving serialized scene: " + fileName);

	sof.Flush();

	SLG_LOG("Scene saved: " << (sof.GetPosition() / 1024) << " Kbytes");
}

template<class Archive> void Scene::load(Archive &ar, const u_int version) {
	// Load ExtMeshCache
	ar & extMeshCache;

	// Load ImageMapCache
	ar & imgMapCache;

	// Load camera, material, texture, etc. definitions
	luxrays::Properties sceneProps;
	ar & sceneProps;

	// Load flags
	ar & enableParsePrint;

	// Parse all the scene properties
	Parse(sceneProps);
}

template<class Archive> void Scene::save(Archive &ar, const u_int version) const {
	// Save ExtMeshCache
	ar & extMeshCache;

	// Save ImageMapCache
	ar & imgMapCache;

	// Save camera, material, texture, etc. definitions
	luxrays::Properties sceneProps = ToProperties(true);
	ar & sceneProps;

	// Save flags
	ar & enableParsePrint;
}

namespace slg {
// Explicit instantiations for portable archives
template void Scene::save(LuxOutputBinArchive &ar, const u_int version) const;
template void Scene::load(LuxInputBinArchive &ar, const u_int version);
template void Scene::save(LuxOutputTextArchive &ar, const u_int version) const;
template void Scene::load(LuxInputTextArchive &ar, const u_int version);
}
