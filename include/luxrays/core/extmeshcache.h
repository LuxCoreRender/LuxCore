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

#ifndef _LUXRAYS_EXTMESHCACHE_H
#define	_LUXRAYS_EXTMESHCACHE_H

#include <string>
#include <vector>
#include <map>

#include <boost/unordered_map.hpp>

#include "luxrays/core/color/color.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/geometry/motionsystem.h"
#include "luxrays/core/context.h"
#include "luxrays/core/exttrianglemesh.h"

namespace luxrays {

class ExtMeshCache {
public:
	ExtMeshCache();
	~ExtMeshCache();

	void SetDeleteMeshData(const bool v) { deleteMeshData = v; }

	void DefineExtMesh(const std::string &fileName,
		const u_int plyNbVerts, const u_int plyNbTris,
		Point *p, Triangle *vi, Normal *n, UV *uv,
		Spectrum *cols, float *alphas);
	void DefineExtMesh(const std::string &fileName, ExtTriangleMesh *mesh);

	bool IsExtMeshDefined(const std::string &name) const { return meshByName.find(name) != meshByName.end(); }

	ExtMesh *GetExtMesh(const std::string &fileName);
	ExtMesh *GetExtMesh(const std::string &fileName, const Transform &trans);
	ExtMesh *GetExtMesh(const std::string &fileName, const MotionSystem &ms);

	// Note: before call to DeleteExtMesh, be sore to not have any instance referencing
	// the geometry
	void DeleteExtMesh(const std::string &fileName);

	u_int GetExtMeshIndex(const std::string &fileName) const;
	u_int GetExtMeshIndex(const ExtMesh *m) const;

	const std::vector<ExtMesh *> &GetMeshes() const { return meshes; }

public:
	boost::unordered_map<std::string, ExtTriangleMesh *> meshByName;
	// Used to preserve insertion order and to retrieve insertion index
	std::vector<ExtMesh *> meshes;

	bool deleteMeshData;
};

}

#endif	/* _LUXRAYS_EXTMESHCACHE_H */
