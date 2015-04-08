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

#include "slg/sdl/extmeshcache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

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

void ExtMeshCache::DefineExtMesh(const string &meshName, ExtMesh *mesh) {
	if (meshByName.count(meshName) == 0) {
		// It is a new mesh
		meshByName.insert(make_pair(meshName, mesh));
		meshes.push_back(mesh);
	} else {
		// Replace an old mesh
		const u_int index = GetExtMeshIndex(meshName);
		ExtMesh *oldMesh = meshes[index];

		meshes[index] = mesh;
		meshByName.erase(meshName);
		meshByName.insert(make_pair(meshName, mesh));

		if (deleteMeshData)
			oldMesh->Delete();
		delete oldMesh;
	}
}

void ExtMeshCache::DefineExtMesh(const string &meshName,
		const u_int plyNbVerts, const u_int plyNbTris,
		Point *p, Triangle *vi, Normal *n, UV *uv, Spectrum *cols, float *alphas) {
	ExtTriangleMesh *mesh = new ExtTriangleMesh(
			plyNbVerts, plyNbTris, p, vi, n, uv, cols, alphas);

	DefineExtMesh(meshName, mesh);
}

void ExtMeshCache::DeleteExtMesh(const string &meshName) {
	const u_int index = GetExtMeshIndex(meshName);

	if (deleteMeshData)
		meshes[index]->Delete();
	delete meshes[index];

	meshes.erase(meshes.begin() + index);
	meshByName.erase(meshName);
}

ExtMesh *ExtMeshCache::GetExtMesh(const string &meshName) {
	// Check if the mesh has been already loaded
	boost::unordered_map<string, ExtMesh *>::const_iterator it = meshByName.find(meshName);

	if (it == meshByName.end())
		throw runtime_error("Unknown mesh: " + meshName);
	else {
		//SDL_LOG("Cached mesh object: " << meshName << ")");
		return it->second;
	}
}

ExtMesh *ExtMeshCache::GetExtMesh(const string &meshName, const Transform &trans) {
	ExtMesh *mesh = GetExtMesh(meshName);
	if (!mesh)
		throw runtime_error("Unknown mesh: " + meshName);

	ExtTriangleMesh *tmesh = dynamic_cast<ExtTriangleMesh *>(mesh);
	if (!tmesh)
		throw runtime_error("Wrong mesh type: " + meshName);

	ExtInstanceTriangleMesh *imesh = new ExtInstanceTriangleMesh(tmesh, trans);
	meshes.push_back(imesh);

	return imesh;
}

ExtMesh *ExtMeshCache::GetExtMesh(const string &meshName, const MotionSystem &ms) {
	ExtMesh *mesh = GetExtMesh(meshName);
	if (!mesh)
		throw runtime_error("Unknown mesh: " + meshName);

	ExtTriangleMesh *tmesh = dynamic_cast<ExtTriangleMesh *>(mesh);
	if (!tmesh)
		throw runtime_error("Wrong mesh type: " + meshName);
	
	ExtMotionTriangleMesh *mmesh = new ExtMotionTriangleMesh(tmesh, ms);
	meshes.push_back(mmesh);

	return mmesh;
}

u_int ExtMeshCache::GetExtMeshIndex(const string &meshName) const {
	boost::unordered_map<string, ExtMesh *>::const_iterator it = meshByName.find(meshName);

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
