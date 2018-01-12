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

#ifndef _SLG_EXTMESHCACHE_H
#define	_SLG_EXTMESHCACHE_H

#include <string>
#include <vector>
#include <map>

#include <boost/unordered_map.hpp>
#include <boost/serialization/version.hpp>

#include "luxrays/core/color/color.h"
#include "luxrays/core/context.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/core/sdl.h"

namespace slg {

class ExtMeshCache {
public:
	ExtMeshCache();
	~ExtMeshCache();

	void SetDeleteMeshData(const bool v) { deleteMeshData = v; }

	// This method can be safely called only from Scene::DefineMesh()
	void DefineExtMesh(const std::string &meshName, luxrays::ExtMesh *mesh);

	bool IsExtMeshDefined(const std::string &meshName) const { return meshByName.find(meshName) != meshByName.end(); }

	luxrays::ExtMesh *GetExtMesh(const std::string &meshName);

	// Note: before calls to DeleteExtMesh, be sure to not have any instance referencing
	// the geometry
	void DeleteExtMesh(const std::string &meshName);

	u_int GetExtMeshIndex(const std::string &meshName) const;
	u_int GetExtMeshIndex(const luxrays::ExtMesh *m) const;

	const std::vector<luxrays::ExtMesh *> &GetMeshes() const { return meshes; }

	std::string GetRealFileName(const luxrays::ExtMesh *m) const;
	std::string GetSequenceFileName(const luxrays::ExtMesh *m) const;

	friend class boost::serialization::access;

private:
	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	// Used to preserve insertion order and to retrieve insertion index
	// It includes all type of meshes: normal, instanced and motion blurred
	std::vector<luxrays::ExtMesh *> meshes;

	boost::unordered_map<std::string, luxrays::ExtMesh *> meshByName;

	bool deleteMeshData;
};

template<class Archive> void ExtMeshCache::load(Archive &ar, const u_int version) {
	// Load the size
	u_int s;
	ar & s;
	meshes.resize(s, NULL);

	for (u_int i = 0; i < meshes.size(); ++i) {
		// Load the name
		std::string name;
		ar & name;
		SDL_LOG("Loading serialized mesh: " << name);

		// Load the mesh
		luxrays::ExtMesh *m;
		ar & m;
		meshes[i] = m;

		meshByName.insert(make_pair(name, m));
	}

	ar & deleteMeshData;
}

template<class Archive> void ExtMeshCache::save(Archive &ar, const u_int version) const {
	// Save the size
	const u_int s = meshes.size();
	ar & s;

	for (boost::unordered_map<std::string, luxrays::ExtMesh *>::const_iterator it = meshByName.begin(); it != meshByName.end(); ++it) {
		// Save the name
		const std::string &name = it->first;
		SDL_LOG("Saving serialized mesh: " << name);
		ar & name;

		// Save the ImageMap
		luxrays::ExtMesh *m = it->second;
		ar & m;
	}

	ar & deleteMeshData;
}

}

BOOST_CLASS_VERSION(slg::ExtMeshCache, 3)

BOOST_CLASS_EXPORT_KEY(slg::ExtMeshCache)

#endif	/* _SLG_EXTMESHCACHE_H */
