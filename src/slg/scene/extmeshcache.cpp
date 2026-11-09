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

#include <boost/format.hpp>
#include <type_traits>

#include "slg/scene/extmeshcache.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/namedobject.h"
#include "luxrays/usings.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ExtMeshCache
//------------------------------------------------------------------------------

ExtMeshCache::ExtMeshCache() {
	deleteMeshData = true;
}

ExtMeshCache::~ExtMeshCache() {
	for(NamedObjectRef obj: meshes.GetObjs()) {
		auto& mesh = static_cast<ExtMesh &>(obj);

		if (deleteMeshData)
			mesh.Delete();

		// Mesh are deleted by NameObjectVector destructor
	}
}

bool ExtMeshCache::IsExtMeshDefined(const std::string &meshName) const {
	return meshes.IsObjDefined(meshName);
}

std::tuple<ExtMesh&, ExtMeshUPtr>
ExtMeshCache::DefineExtMesh(ExtMeshUPtr&& mesh) {



	const string &meshName = mesh->GetName();

	if (!meshes.IsObjDefined(meshName)) {
		// It is a new mesh
		return meshes.DefineObj(std::move(mesh));

	} else {
		// There is already a similar mesh
		//
		// Check if both meshes are of the same type
		auto& meshToReplace = static_cast<ExtMeshRef>(meshes.GetObj(meshName));
		if (meshToReplace.GetType() != mesh->GetType()) {
			throw runtime_error(
				"Mesh " + meshName + " of type " + ToString(mesh->GetType()) +
				" can not replace a mesh of type " + ToString(meshToReplace.GetType())
				+ ". Delete the old mesh first.");
		}

		// Replace the old mesh
		auto [newMeshRef, oldMeshPtr] = meshes.DefineObj(std::move(mesh));
		assert(oldMeshPtr);

		if (oldMeshPtr->GetType() == TYPE_EXT_TRIANGLE) {
			// I have also to check/update all instances and motion blur meshes for
			// reference to the old mesh
			auto& nm = dynamic_cast<ExtTriangleMesh&>(newMeshRef);
			auto& om = dynamic_cast<ExtTriangleMesh&>(*oldMeshPtr);

			for(NamedObjectRef no: meshes.GetObjs()) {
				auto& meshobj = static_cast<ExtMesh&>(no);

				switch (meshobj.GetType()) {
					case TYPE_EXT_TRIANGLE_INSTANCE: {
						auto& imesh = static_cast<ExtInstanceTriangleMesh&>(meshobj);
						imesh.UpdateMeshReferences(om, nm);
						break;
					}
					case TYPE_EXT_TRIANGLE_MOTION: {
						auto& mmesh = static_cast<ExtMotionTriangleMesh&>(meshobj);
						mmesh.UpdateMeshReferences(om, nm);
						break;
					}
					default:
						break;
				}  // switch
			}  // for
		}  // if
		// Rebuild the tuple and return
		return std::make_tuple(std::ref(newMeshRef), std::move(oldMeshPtr));
	}
}

void ExtMeshCache::SetMeshVertexAOV(const string &meshName,
	const unsigned int index, std::span<float> data) {
	if (!meshes.IsObjDefined(meshName))
		throw runtime_error("Unknown mesh " + meshName + " while trying to set vertex AOV");

	auto& mesh = static_cast<ExtMesh&>(meshes.GetObj(meshName));
	if (mesh.GetType() != TYPE_EXT_TRIANGLE)
		throw runtime_error("Can not set vertex AOV of mesh " + meshName + " of type " + ToString(mesh.GetType()));

	auto& triMesh = static_cast<ExtTriangleMesh&>(mesh);
	triMesh.DeleteVertexAOV(index);
	triMesh.SetVertexAOV(index, std::span(data));
}

void ExtMeshCache::SetMeshTriangleAOV(const string &meshName,
	const unsigned int index, std::span<float> data) {
	if (!meshes.IsObjDefined(meshName))
		throw runtime_error("Unknown mesh " + meshName + " while trying to set triangle AOV");

	auto& mesh = static_cast<ExtMesh&>(meshes.GetObj(meshName));
	if (mesh.GetType() != TYPE_EXT_TRIANGLE)
		throw runtime_error("Can not set triangle AOV of mesh " + meshName + " of type " + ToString(mesh.GetType()));

	auto& triMesh = static_cast<ExtTriangleMesh&>(mesh);
	triMesh.DeleteTriAOV(index);
	triMesh.SetTriAOV(index, data);
}

ExtMeshUPtr ExtMeshCache::DeleteExtMesh(const string &meshName) {
	if (deleteMeshData) {
		auto& mesh = static_cast<ExtMesh&>(meshes.GetObj(meshName));
		mesh.Delete();
	}
	return meshes.DeleteObj<ExtMesh>(meshName);
}

u_int ExtMeshCache::GetSize() const {
	return meshes.GetSize();
}

ExtMeshRef ExtMeshCache::GetExtMesh(const string &meshName) {
	return static_cast<ExtMesh&>(meshes.GetObj(meshName));
}

ExtMeshRef ExtMeshCache::GetExtMesh(const u_int index) {
	return static_cast<ExtMesh&>(meshes.GetObj(index));
}

ExtMeshConstRef ExtMeshCache::GetExtMesh(const string &meshName) const {
	return static_cast<const ExtMesh&>(meshes.GetObj(meshName));
}

ExtMeshConstRef ExtMeshCache::GetExtMesh(const u_int index) const {
	return static_cast<const ExtMesh&>(meshes.GetObj(index));
}

u_int ExtMeshCache::GetExtMeshIndex(const string &meshName) const {
	return meshes.GetIndex(meshName);
}

u_int ExtMeshCache::GetExtMeshIndex(ExtMeshConstRef m) const {
	return meshes.GetIndex(m);
}

string ExtMeshCache::GetRealFileName(ExtMeshConstRef m) const {
	if (m.GetType() == TYPE_EXT_TRIANGLE_MOTION) {
		auto& mot = static_cast<const ExtMotionTriangleMesh&>(m);
		auto& meshToFind = mot.GetExtTriangleMesh();
		return meshes.GetName(meshToFind);
	} else if (m.GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
		auto& inst = static_cast<const ExtInstanceTriangleMesh&>(m);
		auto& meshToFind = inst.GetExtTriangleMesh();
		return meshes.GetName(meshToFind);
	} else {
		auto& meshToFind = m;
		return meshes.GetName(meshToFind);
	}

}

string ExtMeshCache::GetSequenceFileName(ExtMeshConstRef m) const {

	u_int meshIndex;
	if (m.GetType() == TYPE_EXT_TRIANGLE_MOTION) {
		auto& mot = static_cast<const ExtMotionTriangleMesh&>(m);
		meshIndex = GetExtMeshIndex(mot.GetExtTriangleMesh());
	} else if (m.GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
		auto& inst = static_cast<const ExtInstanceTriangleMesh&>(m);
		meshIndex = GetExtMeshIndex(inst.GetExtTriangleMesh());
	} else
		meshIndex = GetExtMeshIndex(m);

	return "mesh-" + (boost::format("%05d") % meshIndex).str() + ".ply";
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
