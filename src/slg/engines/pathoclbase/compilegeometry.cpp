/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iosfwd>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/kernels/kernels.h"

using namespace std;
using namespace luxrays;
using namespace slg;

static bool MeshPtrCompare(Mesh *p0, Mesh *p1) {
	return p0 < p1;
}

void CompiledScene::CompileGeometry() {
	SLG_LOG("Compile Geometry");
	wasGeometryCompiled = true;

	const u_int objCount = scene->objDefs.GetSize();

	const double tStart = WallClockTime();

	// Clear vectors
	verts.resize(0);
	normals.resize(0);
	uvs.resize(0);
	cols.resize(0);
	alphas.resize(0);
	tris.resize(0);
	meshDescs.resize(0);

	//--------------------------------------------------------------------------
	// Translate geometry
	//--------------------------------------------------------------------------

	// Not using boost::unordered_map because because the key is an ExtMesh pointer
	map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)> definedMeshs(MeshPtrCompare);

	slg::ocl::Mesh newMeshDesc;
	newMeshDesc.vertsOffset = 0;
	newMeshDesc.trisOffset = 0;
	newMeshDesc.normalsOffset = 0;
	newMeshDesc.uvsOffset = 0;
	newMeshDesc.colsOffset = 0;
	newMeshDesc.alphasOffset = 0;
	memcpy(&newMeshDesc.trans.m, &Matrix4x4::MAT_IDENTITY, sizeof(float[4][4]));
	memcpy(&newMeshDesc.trans.mInv, &Matrix4x4::MAT_IDENTITY, sizeof(float[4][4]));

	slg::ocl::Mesh currentMeshDesc;
	for (u_int i = 0; i < objCount; ++i) {
		const ExtMesh *mesh = scene->objDefs.GetSceneObject(i)->GetExtMesh();

		bool isExistingInstance;
		// TODO: Motion blur is not supported here (!)
		if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
			// It is an instanced mesh
			ExtInstanceTriangleMesh *imesh = (ExtInstanceTriangleMesh *)mesh;

			// Check if is one of the already defined meshes
			map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)>::iterator it = definedMeshs.find(imesh->GetExtTriangleMesh());
			if (it == definedMeshs.end()) {
				// It is a new one
				currentMeshDesc = newMeshDesc;

				newMeshDesc.vertsOffset += mesh->GetTotalVertexCount();
				newMeshDesc.trisOffset += mesh->GetTotalTriangleCount();
				if (mesh->HasNormals())
					newMeshDesc.normalsOffset += mesh->GetTotalVertexCount();
				if (mesh->HasUVs())
					newMeshDesc.uvsOffset += mesh->GetTotalVertexCount();
				if (mesh->HasColors())
					newMeshDesc.colsOffset += mesh->GetTotalVertexCount();
				if (mesh->HasAlphas())
					newMeshDesc.alphasOffset += mesh->GetTotalVertexCount();

				isExistingInstance = false;

				const u_int index = meshDescs.size();
				definedMeshs[imesh->GetExtTriangleMesh()] = index;
			} else {
				currentMeshDesc = meshDescs[it->second];

				isExistingInstance = true;
			}

			// Overwrite the only different fields in an instanced mesh
			memcpy(&currentMeshDesc.trans.m, &imesh->GetTransformation().m, sizeof(float[4][4]));
			memcpy(&currentMeshDesc.trans.mInv, &imesh->GetTransformation().mInv, sizeof(float[4][4]));

			// In order to express normals and vertices in local coordinates
			mesh = imesh->GetExtTriangleMesh();
		} else {
			// It is a not instanced mesh
			currentMeshDesc = newMeshDesc;

			newMeshDesc.vertsOffset += mesh->GetTotalVertexCount();
			newMeshDesc.trisOffset += mesh->GetTotalTriangleCount();
			if (mesh->HasNormals())
				newMeshDesc.normalsOffset += mesh->GetTotalVertexCount();
			if (mesh->HasUVs())
				newMeshDesc.uvsOffset += mesh->GetTotalVertexCount();
			if (mesh->HasColors())
				newMeshDesc.colsOffset += mesh->GetTotalVertexCount();
			if (mesh->HasAlphas())
				newMeshDesc.alphasOffset += mesh->GetTotalVertexCount();

			memcpy(&currentMeshDesc.trans.m, &Matrix4x4::MAT_IDENTITY, sizeof(float[4][4]));
			memcpy(&currentMeshDesc.trans.mInv, &Matrix4x4::MAT_IDENTITY, sizeof(float[4][4]));

			isExistingInstance = false;
		}

		if (!isExistingInstance) {
			//------------------------------------------------------------------
			// Translate mesh normals (expressed in local coordinates)
			//------------------------------------------------------------------

			if (mesh->HasNormals()) {
				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					normals.push_back(mesh->GetShadeNormal(0.f, j));
			} else
				currentMeshDesc.normalsOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Translate vertex uvs
			//------------------------------------------------------------------

			if (mesh->HasUVs()) {
				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					uvs.push_back(mesh->GetUV(j));
			} else
				currentMeshDesc.uvsOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Translate vertex colors
			//------------------------------------------------------------------

			if (mesh->HasColors()) {
				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					cols.push_back(mesh->GetColor(j));
			} else
				currentMeshDesc.colsOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Translate vertex alphas
			//------------------------------------------------------------------

			if (mesh->HasAlphas()) {
				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					alphas.push_back(mesh->GetAlpha(j));
			} else
				currentMeshDesc.alphasOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Translate mesh vertices (expressed in local coordinates)
			//------------------------------------------------------------------

			for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
				verts.push_back(mesh->GetVertex(0.f, j));

			//------------------------------------------------------------------
			// Translate mesh indices
			//------------------------------------------------------------------

			Triangle *mtris = mesh->GetTriangles();
			for (u_int j = 0; j < mesh->GetTotalTriangleCount(); ++j)
				tris.push_back(mtris[j]);
		}

		meshDescs.push_back(currentMeshDesc);
	}

	worldBSphere = scene->dataSet->GetBSphere();

	const double tEnd = WallClockTime();
	SLG_LOG("Scene geometry compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

#endif
