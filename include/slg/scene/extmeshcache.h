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

#ifndef _SLG_EXTMESHCACHE_H
#define	_SLG_EXTMESHCACHE_H

#include <string>
#include <vector>

#include "luxrays/core/color/color.h"
#include "luxrays/core/context.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/namedobjectvector.h"
#include "luxrays/usings.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/core/sdl.h"

namespace slg {
using luxrays::ExtMesh;
using luxrays::ExtMeshRef;
using luxrays::ExtMeshUPtr;

class ExtMeshCache {
public:
	ExtMeshCache();
	~ExtMeshCache();

	void SetDeleteMeshData(const bool v) { deleteMeshData = v; }

	// This method can be safely called only from Scene::DefineMesh()
	std::tuple<ExtMesh&, ExtMeshUPtr> DefineExtMesh(ExtMeshUPtr&& mesh);
	void SetMeshVertexAOV(const std::string &meshName,
		const unsigned int index, std::span<float> data);
	void SetMeshTriangleAOV(const std::string &meshName,
		const unsigned int index, std::span<float> data);

	bool IsExtMeshDefined(const std::string &meshName) const;

	// Note: before calls to DeleteExtMesh, be sure to not have any instance referencing
	// the geometry
	ExtMeshUPtr DeleteExtMesh(const std::string &meshName);

	u_int GetSize() const;
	auto GetExtMeshNames() const {
		return meshes.GetNames();
	}


	luxrays::ExtMeshRef GetExtMesh(const std::string &meshName);
	luxrays::ExtMeshRef GetExtMesh(const u_int index);
	luxrays::ExtMeshConstRef GetExtMesh(const std::string &meshName) const;
	luxrays::ExtMeshConstRef GetExtMesh(const u_int index) const;
	u_int GetExtMeshIndex(const std::string &meshName) const;
	u_int GetExtMeshIndex(luxrays::ExtMeshConstRef m) const;

	std::string GetRealFileName(luxrays::ExtMeshConstRef m) const;
	std::string GetSequenceFileName(luxrays::ExtMeshConstRef m) const;

	friend class boost::serialization::access;

private:
	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	// Used to preserve insertion order and to retrieve insertion index
	// It includes all type of meshes: normal, instanced and motion blurred
	luxrays::NamedObjectVector meshes;

	bool deleteMeshData;
};

}

BOOST_CLASS_VERSION(slg::ExtMeshCache, 4)

BOOST_CLASS_EXPORT_KEY(slg::ExtMeshCache)

#endif	/* _SLG_EXTMESHCACHE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
