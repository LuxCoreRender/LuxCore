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

#ifndef _SLG_EXTMESHCACHE_H
#define	_SLG_EXTMESHCACHE_H

#include <string>
#include <vector>
#include <map>

#include <boost/unordered_map.hpp>

#include "luxrays/core/color/color.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/geometry/motionsystem.h"
#include "luxrays/core/context.h"
#include "luxrays/core/exttrianglemesh.h"

namespace slg {

class ExtMeshCache {
public:
	ExtMeshCache();
	~ExtMeshCache();

	void SetDeleteMeshData(const bool v) { deleteMeshData = v; }

	void DefineExtMesh(const std::string &meshName,
		const u_int plyNbVerts, const u_int plyNbTris,
		luxrays::Point *p, luxrays::Triangle *vi, luxrays::Normal *n, luxrays::UV *uv,
		luxrays::Spectrum *cols, float *alphas);
	void DefineExtMesh(const std::string &meshName, luxrays::ExtMesh *mesh);

	bool IsExtMeshDefined(const std::string &meshName) const { return meshByName.find(meshName) != meshByName.end(); }

	luxrays::ExtMesh *GetExtMesh(const std::string &meshName);
	luxrays::ExtMesh *GetExtMesh(const std::string &meshName, const luxrays::Transform &trans);
	luxrays::ExtMesh *GetExtMesh(const std::string &meshName, const luxrays::MotionSystem &ms);

	// Note: before call to DeleteExtMesh, be sure to not have any instance referencing
	// the geometry
	void DeleteExtMesh(const std::string &meshName);

	u_int GetExtMeshIndex(const std::string &meshName) const;
	u_int GetExtMeshIndex(const luxrays::ExtMesh *m) const;

	const std::vector<luxrays::ExtMesh *> &GetMeshes() const { return meshes; }

public:
	boost::unordered_map<std::string, luxrays::ExtMesh *> meshByName;
	// Used to preserve insertion order and to retrieve insertion index
	std::vector<luxrays::ExtMesh *> meshes;

	bool deleteMeshData;
};

}

#endif	/* _SLG_EXTMESHCACHE_H */
