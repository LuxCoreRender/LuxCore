/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#include "luxrays/core/extmeshcache.h"

using namespace std;
using namespace luxrays;

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

void ExtMeshCache::DefineExtMesh(const string &fileName, ExtTriangleMesh *mesh,
		const bool usePlyNormals) {
	const string key = (usePlyNormals ? "1-" : "0-") + fileName;

	if (meshByName.count(key) == 0) {
		// It is a new mesh
		meshByName.insert(make_pair(key, mesh));
		meshes.push_back(mesh);
	} else {
		// Replace an old mesh
		const u_int index = GetExtMeshIndex(fileName, usePlyNormals);
		ExtMesh *oldMesh = meshes[index];

		meshes[index] = mesh;
		meshByName.erase(key);
		meshByName.insert(make_pair(key, mesh));

		if (deleteMeshData)
			oldMesh->Delete();
		delete oldMesh;
	}
}

void ExtMeshCache::DefineExtMesh(const string &fileName,
		const u_int plyNbVerts, const u_int plyNbTris,
		Point *p, Triangle *vi, Normal *n, UV *uv, Spectrum *cols, float *alphas,
		const bool usePlyNormals) {
	ExtTriangleMesh *mesh = ExtTriangleMesh::CreateExtTriangleMesh(
			plyNbVerts, plyNbTris, p, vi, n, uv, cols, alphas,
			usePlyNormals);

	DefineExtMesh(fileName, mesh, usePlyNormals);
}

void ExtMeshCache::DeleteExtMesh(const string &fileName, const bool usePlyNormals) {
	const u_int index = GetExtMeshIndex(fileName, usePlyNormals);

	if (deleteMeshData)
		meshes[index]->Delete();
	delete meshes[index];

	meshes.erase(meshes.begin() + index);
	const string key = (usePlyNormals ? "1-" : "0-") + fileName;
	meshByName.erase(key);
}

ExtMesh *ExtMeshCache::GetExtMesh(const string &fileName, const bool usePlyNormals,
		const Transform *trans) {
	if (trans) {
		ExtTriangleMesh *mesh = (ExtTriangleMesh *)GetExtMesh(fileName, usePlyNormals);

		ExtInstanceTriangleMesh *imesh = new ExtInstanceTriangleMesh(mesh, *trans);
		meshes.push_back(imesh);

		return imesh;
	} else {
		const string key = (usePlyNormals ? "1-" : "0-") + fileName;

		// Check if the mesh has been already loaded
		boost::unordered_map<string, ExtTriangleMesh *>::const_iterator it = meshByName.find(key);

		if (it == meshByName.end()) {
			// I have yet to load the file
			ExtTriangleMesh *mesh = ExtTriangleMesh::LoadExtTriangleMesh(fileName, usePlyNormals);

			meshByName.insert(make_pair(key, mesh));
			meshes.push_back(mesh);

			return mesh;
		} else {
			//SDL_LOG("Cached mesh object: " << fileName << " (use PLY normals: " << usePlyNormals << ")");
			return it->second;
		}
	}
}

u_int ExtMeshCache::GetExtMeshIndex(const string &fileName, const bool usePlyNormals) const {
	const string key = (usePlyNormals ? "1-" : "0-") + fileName;
	boost::unordered_map<string, ExtTriangleMesh *>::const_iterator it = meshByName.find(key);

	return GetExtMeshIndex(it->second);
}

u_int ExtMeshCache::GetExtMeshIndex(const ExtMesh *m) const {
	// TODO: use a boost::unordered_map
	u_int i = 0;
	for (vector<ExtMesh *>::const_iterator it = meshes.begin(); it != meshes.end(); ++it) {
		if (*it == m)
			return i;
		else
			++i;
	}

	throw runtime_error("Unknown mesh: " + boost::lexical_cast<string>(m));
}
