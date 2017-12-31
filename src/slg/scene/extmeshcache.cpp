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

#include <boost/format.hpp>

#include "slg/scene/extmeshcache.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ExtMeshCache
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ExtMeshCache)

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

		if (oldMesh->GetType() != mesh->GetType())
			throw runtime_error("Mesh " + meshName + " of type " + ToString(mesh->GetType()) +
					" can not replace a mesh of type " + ToString(oldMesh->GetType()) + ". Delete the old mesh first.");

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

void ExtMeshCache::DefineExtMesh(const string &instMeshName, const string &meshName,
		const Transform &trans) {
	ExtMesh *mesh = GetExtMesh(meshName);
	if (!mesh)
		throw runtime_error("Unknown mesh in ExtMeshCache::DefineExtMesh(): " + meshName);

	ExtTriangleMesh *etMesh = dynamic_cast<ExtTriangleMesh *>(mesh);
	if (!etMesh)
		throw runtime_error("Wrong mesh type: " + meshName);

	ExtInstanceTriangleMesh *iMesh = new ExtInstanceTriangleMesh(etMesh, trans);
	DefineExtMesh(instMeshName, iMesh);
}

void ExtMeshCache::DefineExtMesh(const string &motMeshName, const string &meshName,
		const MotionSystem &ms) {
	ExtMesh *mesh = GetExtMesh(meshName);
	if (!mesh)
		throw runtime_error("Unknown mesh in ExtMeshCache::DefineExtMesh(): " + meshName);

	ExtTriangleMesh *etMesh = dynamic_cast<ExtTriangleMesh *>(mesh);
	if (!etMesh)
		throw runtime_error("Wrong mesh type: " + meshName);
	
	ExtMotionTriangleMesh *motMesh = new ExtMotionTriangleMesh(etMesh, ms);
	DefineExtMesh(motMeshName, motMesh);
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
	// Check if the mesh has been already defined
	boost::unordered_map<string, ExtMesh *>::const_iterator it = meshByName.find(meshName);

	if (it == meshByName.end())
		throw runtime_error("Unknown mesh in ExtMeshCache::GetExtMesh(): " + meshName);
	else {
		//SDL_LOG("Cached mesh object: " << meshName << ")");
		return it->second;
	}
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

	throw runtime_error("Unknown mesh in ExtMeshCache::GetExtMeshIndex(): " + boost::lexical_cast<string>(m));
}

string ExtMeshCache::GetRealFileName(const ExtMesh *m) const {
	const ExtMesh *meshToFind;
	if (m->GetType() == TYPE_EXT_TRIANGLE_MOTION) {
		const ExtMotionTriangleMesh *mot = (const ExtMotionTriangleMesh *)m;
		meshToFind = mot->GetExtTriangleMesh();
	} else if (m->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
		const ExtInstanceTriangleMesh *inst = (const ExtInstanceTriangleMesh *)m;
		meshToFind = inst->GetExtTriangleMesh();
	} else
		meshToFind = m;

	for (boost::unordered_map<std::string, ExtMesh *>::const_iterator it = meshByName.begin(); it != meshByName.end(); ++it) {
		if (it->second == meshToFind) {

			return it->first;
		}
	}

	throw runtime_error("Unknown mesh in ExtMeshCache::GetRealFileName(): " + boost::lexical_cast<string>(m));
}

string ExtMeshCache::GetSequenceFileName(const ExtMesh *m) const {

	u_int meshIndex;
	if (m->GetType() == TYPE_EXT_TRIANGLE_MOTION) {
		const ExtMotionTriangleMesh *mot = (const ExtMotionTriangleMesh *)m;
		meshIndex = GetExtMeshIndex(mot->GetExtTriangleMesh());
	} else if (m->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
		const ExtInstanceTriangleMesh *inst = (const ExtInstanceTriangleMesh *)m;
		meshIndex = GetExtMeshIndex(inst->GetExtTriangleMesh());
	} else
		meshIndex = GetExtMeshIndex(m);

	return "mesh-" + (boost::format("%05d") % meshIndex).str() + ".ply";
}
