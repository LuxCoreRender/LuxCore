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

static bool MeshPtrCompare(const Mesh *p0, const Mesh *p1) {
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
	triNormals.resize(0);
	uvs.resize(0);
	cols.resize(0);
	alphas.resize(0);
	tris.resize(0);
	interpolatedTransforms.resize(0);
	meshDescs.resize(0);

	//--------------------------------------------------------------------------
	// Translate geometry
	//--------------------------------------------------------------------------

	// Not using boost::unordered_map because the key is an ExtMesh pointer
	map<ExtMesh *, u_int, bool (*)(const Mesh *, const Mesh *)> definedMeshs(MeshPtrCompare);

	slg::ocl::ExtMesh newMeshDesc;
	newMeshDesc.vertsOffset = 0;
	newMeshDesc.trisOffset = 0;
	newMeshDesc.normalsOffset = 0;
	newMeshDesc.triNormalsOffset = 0;
	newMeshDesc.uvsOffset = 0;
	newMeshDesc.colsOffset = 0;
	newMeshDesc.alphasOffset = 0;

	slg::ocl::ExtMesh currentMeshDesc;
	for (u_int i = 0; i < objCount; ++i) {
		const ExtMesh *mesh = scene->objDefs.GetSceneObject(i)->GetExtMesh();

		bool isExistingInstance;
		const ExtTriangleMesh *baseMesh = nullptr;
		switch (mesh->GetType()) {
			case TYPE_EXT_TRIANGLE_INSTANCE: {
				// It is an instanced mesh
				ExtInstanceTriangleMesh *imesh = (ExtInstanceTriangleMesh *)mesh;
				baseMesh = imesh->GetExtTriangleMesh();

				// Check if is one of the already defined meshes
				map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)>::iterator it = definedMeshs.find(imesh->GetExtTriangleMesh());
				if (it == definedMeshs.end()) {
					// It is a new one
					currentMeshDesc = newMeshDesc;

					newMeshDesc.vertsOffset += imesh->GetTotalVertexCount();
					newMeshDesc.trisOffset += imesh->GetTotalTriangleCount();
					newMeshDesc.triNormalsOffset += imesh->GetTotalTriangleCount();
					if (imesh->HasNormals())
						newMeshDesc.normalsOffset += imesh->GetTotalVertexCount();
					if (imesh->HasUVs())
						newMeshDesc.uvsOffset += imesh->GetTotalVertexCount();
					if (imesh->HasColors())
						newMeshDesc.colsOffset += imesh->GetTotalVertexCount();
					if (imesh->HasAlphas())
						newMeshDesc.alphasOffset += imesh->GetTotalVertexCount();

					isExistingInstance = false;

					const u_int index = meshDescs.size();
					definedMeshs[imesh->GetExtTriangleMesh()] = index;
				} else {
					currentMeshDesc = meshDescs[it->second];

					isExistingInstance = true;
				}

				currentMeshDesc.type = slg::ocl::TYPE_EXT_TRIANGLE_INSTANCE;
				memcpy(&currentMeshDesc.instance.trans.m, &imesh->GetTransformation().m, sizeof(float[4][4]));
				memcpy(&currentMeshDesc.instance.trans.mInv, &imesh->GetTransformation().mInv, sizeof(float[4][4]));
				break;
			}
			case TYPE_EXT_TRIANGLE_MOTION: {
				// It is an instanced mesh
				ExtMotionTriangleMesh *mmesh = (ExtMotionTriangleMesh *)mesh;
				baseMesh = mmesh->GetExtTriangleMesh();

				// Check if is one of the already defined meshes
				map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)>::iterator it = definedMeshs.find(mmesh->GetExtTriangleMesh());
				if (it == definedMeshs.end()) {
					// It is a new one
					currentMeshDesc = newMeshDesc;

					newMeshDesc.vertsOffset += mmesh->GetTotalVertexCount();
					newMeshDesc.trisOffset += mmesh->GetTotalTriangleCount();
					newMeshDesc.triNormalsOffset += mmesh->GetTotalTriangleCount();
					if (mmesh->HasNormals())
						newMeshDesc.normalsOffset += mmesh->GetTotalVertexCount();
					if (mmesh->HasUVs())
						newMeshDesc.uvsOffset += mmesh->GetTotalVertexCount();
					if (mmesh->HasColors())
						newMeshDesc.colsOffset += mmesh->GetTotalVertexCount();
					if (mmesh->HasAlphas())
						newMeshDesc.alphasOffset += mmesh->GetTotalVertexCount();

					isExistingInstance = false;

					const u_int index = meshDescs.size();
					definedMeshs[mmesh->GetExtTriangleMesh()] = index;
				} else {
					currentMeshDesc = meshDescs[it->second];

					isExistingInstance = true;
				}

				currentMeshDesc.type = slg::ocl::TYPE_EXT_TRIANGLE_MOTION;
				
				const MotionSystem &ms = mmesh->GetMotionSystem();

				// Copy the motion system information

				// Forward transformations
				currentMeshDesc.motion.motionSystem.interpolatedTransformFirstIndex = interpolatedTransforms.size();
				for (auto const &it : ms.interpolatedTransforms) {
					// Here, I assume that luxrays::ocl::InterpolatedTransform and
					// luxrays::InterpolatedTransform are the same
					interpolatedTransforms.push_back(*((const luxrays::ocl::InterpolatedTransform *)&it));
				}
				currentMeshDesc.motion.motionSystem.interpolatedTransformLastIndex = interpolatedTransforms.size() - 1;

				// Backward transformations
				currentMeshDesc.motion.motionSystem.interpolatedInverseTransformFirstIndex = interpolatedTransforms.size();
				for (auto const &it : ms.interpolatedInverseTransforms) {
					// Here, I assume that luxrays::ocl::InterpolatedTransform and
					// luxrays::InterpolatedTransform are the same
					interpolatedTransforms.push_back(*((const luxrays::ocl::InterpolatedTransform *)&it));
				}
				currentMeshDesc.motion.motionSystem.interpolatedInverseTransformLastIndex = interpolatedTransforms.size() - 1;
				break;
			}
			case TYPE_EXT_TRIANGLE: {
				baseMesh = (const ExtTriangleMesh *)mesh;

				// It is a not instanced mesh
				currentMeshDesc = newMeshDesc;

				newMeshDesc.vertsOffset += mesh->GetTotalVertexCount();
				newMeshDesc.trisOffset += mesh->GetTotalTriangleCount();
				newMeshDesc.triNormalsOffset += mesh->GetTotalTriangleCount();
				if (mesh->HasNormals())
					newMeshDesc.normalsOffset += mesh->GetTotalVertexCount();
				if (mesh->HasUVs())
					newMeshDesc.uvsOffset += mesh->GetTotalVertexCount();
				if (mesh->HasColors())
					newMeshDesc.colsOffset += mesh->GetTotalVertexCount();
				if (mesh->HasAlphas())
					newMeshDesc.alphasOffset += mesh->GetTotalVertexCount();

				currentMeshDesc.type = slg::ocl::TYPE_EXT_TRIANGLE;

				Transform t;
				// Time doesn't matter for ExtTriangleMesh
				baseMesh->GetLocal2World(0.f, t);
				memcpy(&currentMeshDesc.triangle.appliedTrans.m, &t.m, sizeof(float[4][4]));
				memcpy(&currentMeshDesc.triangle.appliedTrans.mInv, &t.mInv, sizeof(float[4][4]));

				isExistingInstance = false;
				break;
			}
			default:
				throw runtime_error("Unsupported mesh type in CompiledScene::CompileGeometry(): " + ToString(mesh->GetType()));
		}

		if (!isExistingInstance) {		
			//------------------------------------------------------------------
			// Compile mesh normals (expressed in local coordinates)
			//------------------------------------------------------------------

			if (baseMesh->HasNormals()) {
				const Normal *n = baseMesh->GetNormals();
				normals.insert(normals.end(), n, n + baseMesh->GetTotalVertexCount());
			} else
				currentMeshDesc.normalsOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Compile mesh triangle normals (expressed in local coordinates)
			//------------------------------------------------------------------

			const Normal *tn = baseMesh->GetTriNormals();
			triNormals.insert(triNormals.end(), tn, tn + baseMesh->GetTotalTriangleCount());

			//------------------------------------------------------------------
			// Compile vertex uvs
			//------------------------------------------------------------------

			if (baseMesh->HasUVs()) {
				const UV *u = baseMesh->GetUVs();
				uvs.insert(uvs.end(), u, u + baseMesh->GetTotalVertexCount());
			} else
				currentMeshDesc.uvsOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Compile vertex colors
			//------------------------------------------------------------------

			if (baseMesh->HasColors()) {
				const Spectrum *c = baseMesh->GetColors();
				cols.insert(cols.end(), c, c + baseMesh->GetTotalVertexCount());
			} else
				currentMeshDesc.colsOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Compile vertex alphas
			//------------------------------------------------------------------

			if (baseMesh->HasAlphas()) {
				const float *a = baseMesh->GetAlphas();
				alphas.insert(alphas.end(), a, a + baseMesh->GetTotalVertexCount());
			} else
				currentMeshDesc.alphasOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Compile baseMesh vertices (expressed in local coordinates)
			//------------------------------------------------------------------

			const Point *v = baseMesh->GetVertices();
			verts.insert(verts.end(), v, v + baseMesh->GetTotalVertexCount());

			//------------------------------------------------------------------
			// Compile baseMesh triangle indices
			//------------------------------------------------------------------

			const Triangle *t = baseMesh->GetTriangles();
			tris.insert(tris.end(), t, t + baseMesh->GetTotalTriangleCount());
		}

		meshDescs.push_back(currentMeshDesc);
	}

	worldBSphere = scene->dataSet->GetBSphere();

	const double tEnd = WallClockTime();
	SLG_LOG("Scene geometry compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

#endif
