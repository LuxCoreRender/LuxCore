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

void ExtMeshCache::DefineExtMesh(const string &fileName, ExtTriangleMesh *mesh) {
	if (meshByName.count(fileName) == 0) {
		// It is a new mesh
		meshByName.insert(make_pair(fileName, mesh));
		meshes.push_back(mesh);
	} else {
		// Replace an old mesh
		const u_int index = GetExtMeshIndex(fileName);
		ExtMesh *oldMesh = meshes[index];

		meshes[index] = mesh;
		meshByName.erase(fileName);
		meshByName.insert(make_pair(fileName, mesh));

		if (deleteMeshData)
			oldMesh->Delete();
		delete oldMesh;
	}
}

void ExtMeshCache::DefineExtMesh(const string &fileName,
		const u_int plyNbVerts, const u_int plyNbTris,
		Point *p, Triangle *vi, Normal *n, UV *uv, Spectrum *cols, float *alphas) {
	ExtTriangleMesh *mesh = ExtTriangleMesh::CreateExtTriangleMesh(
			plyNbVerts, plyNbTris, p, vi, n, uv, cols, alphas);

	DefineExtMesh(fileName, mesh);
}

void ExtMeshCache::DeleteExtMesh(const string &fileName) {
	const u_int index = GetExtMeshIndex(fileName);

	if (deleteMeshData)
		meshes[index]->Delete();
	delete meshes[index];

	meshes.erase(meshes.begin() + index);
	meshByName.erase(fileName);
}

ExtMesh *ExtMeshCache::GetExtMesh(const string &fileName) {
	// Check if the mesh has been already loaded
	boost::unordered_map<string, ExtTriangleMesh *>::const_iterator it = meshByName.find(fileName);

	if (it == meshByName.end()) {
		// I have yet to load the file
		ExtTriangleMesh *mesh = ExtTriangleMesh::LoadExtTriangleMesh(fileName);

		meshByName.insert(make_pair(fileName, mesh));
		meshes.push_back(mesh);

		return mesh;
	} else {
		//SDL_LOG("Cached mesh object: " << fileName << ")");
		return it->second;
	}
}

ExtMesh *ExtMeshCache::GetExtMesh(const string &fileName, const Transform &trans) {
	ExtTriangleMesh *mesh = (ExtTriangleMesh *)GetExtMesh(fileName);

	ExtInstanceTriangleMesh *imesh = new ExtInstanceTriangleMesh(mesh, trans);
	meshes.push_back(imesh);

	return imesh;
}

ExtMesh *ExtMeshCache::GetExtMesh(const string &fileName, const MotionSystem &ms) {
	ExtTriangleMesh *mesh = (ExtTriangleMesh *)GetExtMesh(fileName);

	ExtMotionTriangleMesh *mmesh = new ExtMotionTriangleMesh(mesh, ms);
	meshes.push_back(mmesh);

	return mmesh;
}

u_int ExtMeshCache::GetExtMeshIndex(const string &fileName) const {
	boost::unordered_map<string, ExtTriangleMesh *>::const_iterator it = meshByName.find(fileName);

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
