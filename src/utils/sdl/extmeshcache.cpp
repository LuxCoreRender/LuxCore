/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include "luxrays/core/context.h"
#include "luxrays/utils/sdl/sdl.h"
#include "luxrays/utils/sdl/extmeshcache.h"

using namespace luxrays;
using namespace luxrays::sdl;

ExtMeshCache::ExtMeshCache() {
	deleteMeshData = true;
}

ExtMeshCache::~ExtMeshCache() {
	for (size_t i = 0; i < meshes.size(); ++i) {
		if (deleteMeshData)
			meshes[i]->Delete();
		delete meshes[i];
	}
}

void ExtMeshCache::DefineExtMesh(const std::string &fileName, ExtTriangleMesh *mesh,
		const bool usePlyNormals) {
	assert (n || (!usePlyNormals));
	std::string key = (usePlyNormals ? "1-" : "0-") + fileName;
	maps.insert(std::make_pair(key, mesh));
	meshes.push_back(mesh);
}

void ExtMeshCache::DefineExtMesh(const std::string &fileName,
		const long plyNbVerts, const long plyNbTris,
		Point *p, Triangle *vi, Normal *n, UV *uv,
		const bool usePlyNormals) {
	ExtTriangleMesh *mesh = ExtTriangleMesh::CreateExtTriangleMesh(
			plyNbVerts, plyNbTris, p, vi, n, uv,
			usePlyNormals);

	DefineExtMesh(fileName, mesh, usePlyNormals);
}

ExtMesh *ExtMeshCache::GetExtMesh(const std::string &fileName, const bool usePlyNormals) {
	std::string key = (usePlyNormals ? "1-" : "0-") + fileName;

	// Check if the mesh has been already loaded
	std::map<std::string, ExtTriangleMesh *>::const_iterator it = maps.find(key);

	if (it == maps.end()) {
		// I have yet to load the file
		ExtTriangleMesh *mesh = ExtTriangleMesh::LoadExtTriangleMesh(fileName, usePlyNormals);

		maps.insert(std::make_pair(key, mesh));
		meshes.push_back(mesh);

		return mesh;
	} else {
		//SDL_LOG("Cached mesh object: " << fileName << " (use PLY normals: " << usePlyNormals << ")");
		return it->second;
	}
}

ExtMesh *ExtMeshCache::GetExtMesh(const std::string &fileName, const bool usePlyNormals,
		const Transform &trans) {
	ExtTriangleMesh *mesh = (ExtTriangleMesh *)GetExtMesh(fileName, usePlyNormals);

	ExtInstanceTriangleMesh *imesh = new ExtInstanceTriangleMesh(mesh, trans);
	meshes.push_back(imesh);

	return imesh;
}
