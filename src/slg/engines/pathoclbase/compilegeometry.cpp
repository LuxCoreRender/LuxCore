/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/utils/buffer.h"
#include <functional>
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

static bool MeshPtrCompare(const Mesh * p0, const Mesh * p1) {
	return p0 < p1;
}

void CompiledScene::CompileGeometry() {
	SLG_LOG("Compile Geometry");
	wasGeometryCompiled = true;

	const u_int objCount = scene.GetObjects().GetSize();

	const double tStart = WallClockTime();

	// Clear vectors
	verts.resize(0);
	normals.resize(0);
	triNormals.resize(0);
	uvs.resize(0);
	cols.resize(0);
	alphas.resize(0);
	vertexAOVs.resize(0);
	triAOVs.resize(0);
	tris.resize(0);
	interpolatedTransforms.resize(0);
	meshDescs.resize(0);

	//--------------------------------------------------------------------------
	// Translate geometry
	//--------------------------------------------------------------------------

	// Not using std::unordered_map because the key is an ExtMesh pointer
	std::map<const ExtMesh *, u_int, bool (*)(const Mesh * , const Mesh * )> definedMeshs(MeshPtrCompare);

	u_int vertsOffset = 0;
	u_int trisOffset = 0;
	u_int normalsOffset = 0;
	u_int triNormalsOffset = 0;
	u_int uvsOffset = 0;
	u_int colsOffset = 0;
	u_int alphasOffset = 0;
	u_int vertexAOVOffset = 0;
	u_int triAOVOffset = 0;

	auto InitMeshDesc = [&](slg::ocl::ExtMesh &dstMeshDesc, const ExtMesh &srcMesh) { 
        dstMeshDesc.vertsOffset = vertsOffset;
		vertsOffset += srcMesh.GetTotalVertexCount();

		dstMeshDesc.trisOffset = trisOffset;
		trisOffset += srcMesh.GetTotalTriangleCount();

		if (srcMesh.HasNormals()) {
			dstMeshDesc.normalsOffset = normalsOffset;
			normalsOffset += srcMesh.GetTotalVertexCount();
		} else
			dstMeshDesc.normalsOffset = NULL_INDEX;

		dstMeshDesc.triNormalsOffset = triNormalsOffset;
		triNormalsOffset += srcMesh.GetTotalTriangleCount();

		for (u_int dataIndex = 0; dataIndex < EXTMESH_MAX_DATA_COUNT; ++dataIndex) {
			if (srcMesh.HasUVs(dataIndex)) {
				dstMeshDesc.uvsOffset[dataIndex] = uvsOffset;
				uvsOffset += srcMesh.GetTotalVertexCount();
			} else
				dstMeshDesc.uvsOffset[dataIndex] = NULL_INDEX;

			if (srcMesh.HasColors(dataIndex)) {
				dstMeshDesc.colsOffset[dataIndex] = colsOffset;
				colsOffset += srcMesh.GetTotalVertexCount();
			} else
				dstMeshDesc.colsOffset[dataIndex] = NULL_INDEX;

			if (srcMesh.HasAlphas(dataIndex)) {
				dstMeshDesc.alphasOffset[dataIndex] = alphasOffset;
				alphasOffset += srcMesh.GetTotalVertexCount();
			} else
				dstMeshDesc.alphasOffset[dataIndex] = NULL_INDEX;

			if (srcMesh.HasVertexAOV(dataIndex)) {
				dstMeshDesc.vertexAOVOffset[dataIndex] = vertexAOVOffset;
				vertexAOVOffset += srcMesh.GetTotalVertexCount();
			} else
				dstMeshDesc.vertexAOVOffset[dataIndex] = NULL_INDEX;

			if (srcMesh.HasTriAOV(dataIndex)) {
				dstMeshDesc.triAOVOffset[dataIndex] = triAOVOffset;
				triAOVOffset += srcMesh.GetTotalTriangleCount();
			} else
				dstMeshDesc.triAOVOffset[dataIndex] = NULL_INDEX;
		}
    };

	slg::ocl::ExtMesh currentMeshDesc;
	// Placeholder mesh (serialization ctor) only used as initial value of
	// baseMesh, which is always reassigned in the loop below
	static Shape dummyShape;
	ExtTriangleMesh initBaseMesh(dummyShape);
	std::reference_wrapper<const ExtTriangleMesh> baseMesh = initBaseMesh;
	for (u_int i = 0; i < objCount; ++i) {
		ExtMeshConstRef mesh = scene.GetObjects().GetSceneObject(i).GetExtMesh();

		bool isExistingInstance;
		switch (mesh.GetType()) {
			case TYPE_EXT_TRIANGLE_INSTANCE: {
				// It is an instanced mesh
				auto& imesh = static_cast<const ExtInstanceTriangleMesh &>(mesh);
				baseMesh = imesh.GetExtTriangleMesh();

				// Check if is one of the already defined meshes
				ExtTriangleMeshConstRef trimesh = imesh.GetExtTriangleMesh();
				auto it = definedMeshs.find(&trimesh);
				// auto it = definedMeshs.find(&imesh.GetExtTriangleMesh());
				if (it == definedMeshs.end()) {
					// It is a new one
					InitMeshDesc(currentMeshDesc, imesh);

					isExistingInstance = false;

					const u_int index = meshDescs.size();
					definedMeshs[&imesh.GetExtTriangleMesh()] = index;
				} else {
					currentMeshDesc = meshDescs[it->second];

					isExistingInstance = true;
				}

				currentMeshDesc.type = slg::ocl::TYPE_EXT_TRIANGLE_INSTANCE;
				memcpy(&currentMeshDesc.instance.trans.m, &imesh.GetTransformation().m, sizeof(float[4][4]));
				memcpy(&currentMeshDesc.instance.trans.mInv, &imesh.GetTransformation().mInv, sizeof(float[4][4]));
				currentMeshDesc.instance.transSwapsHandedness = imesh.GetTransformation().SwapsHandedness();
				break;
			}
			case TYPE_EXT_TRIANGLE_MOTION: {
				// It is an instanced mesh
				auto& mmesh = static_cast<const ExtMotionTriangleMesh&>(mesh);
				baseMesh = mmesh.GetExtTriangleMesh();

				// Check if is one of the already defined meshes
				auto it = definedMeshs.find(&mmesh.GetExtTriangleMesh());
				if (it == definedMeshs.end()) {
					// It is a new one
					InitMeshDesc(currentMeshDesc, mmesh);

					isExistingInstance = false;

					const u_int index = meshDescs.size();
					definedMeshs[&mmesh.GetExtTriangleMesh()] = index;
				} else {
					currentMeshDesc = meshDescs[it->second];

					isExistingInstance = true;
				}

				currentMeshDesc.type = slg::ocl::TYPE_EXT_TRIANGLE_MOTION;
				
				const MotionSystem &ms = mmesh.GetMotionSystem();

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
				baseMesh = static_cast<const ExtTriangleMesh &>(mesh);

				// It is a not instanced mesh
				InitMeshDesc(currentMeshDesc, baseMesh);

				currentMeshDesc.type = slg::ocl::TYPE_EXT_TRIANGLE;

				Transform t;
				// Time doesn't matter for ExtTriangleMesh
t = 				baseMesh.get().GetLocal2World(0.f);
				memcpy(&currentMeshDesc.triangle.appliedTrans.m, &t.m, sizeof(float[4][4]));
				memcpy(&currentMeshDesc.triangle.appliedTrans.mInv, &t.mInv, sizeof(float[4][4]));
				currentMeshDesc.triangle.appliedTransSwapsHandedness = t.SwapsHandedness();

				isExistingInstance = false;
				break;
			}
			default:
				throw runtime_error("Unsupported mesh type in CompiledScene::CompileGeometry(): " + ToString(mesh.GetType()));
		}

		if (!isExistingInstance) {		
			//------------------------------------------------------------------
			// Compile mesh normals (expressed in local coordinates)
			//------------------------------------------------------------------

			if (baseMesh.get().HasNormals()) {
				const auto& n = baseMesh.get().GetNormals().GetObjects();
				normals.insert(normals.end(), n.begin(), n.begin() + baseMesh.get().GetTotalVertexCount());
			}

			//------------------------------------------------------------------
			// Compile mesh triangle normals (expressed in local coordinates)
			//------------------------------------------------------------------

			const auto& tn = baseMesh.get().GetTriNormals().GetObjects();
			triNormals.insert(
				triNormals.end(),
				tn.begin(),
				tn.begin() + baseMesh.get().GetTotalTriangleCount()
			);

			for (u_int dataIndex = 0; dataIndex < EXTMESH_MAX_DATA_COUNT; ++dataIndex) {
				//--------------------------------------------------------------
				// Compile vertex uvs
				//--------------------------------------------------------------

				if (baseMesh.get().HasUVs(dataIndex)) {
					const UV *u = baseMesh.get().GetUVs(dataIndex).get();
					uvs.insert(uvs.end(), u, u + baseMesh.get().GetTotalVertexCount());
				}

				//--------------------------------------------------------------
				// Compile vertex colors
				//--------------------------------------------------------------

				if (baseMesh.get().HasColors(dataIndex)) {
					const Spectrum *c = baseMesh.get().GetColors(dataIndex).get();
					cols.insert(cols.end(), c, c + baseMesh.get().GetTotalVertexCount());
				}

				//--------------------------------------------------------------
				// Compile vertex alphas
				//--------------------------------------------------------------

				if (baseMesh.get().HasAlphas(dataIndex)) {
					const float *a = baseMesh.get().GetAlphas(dataIndex).get();
					alphas.insert(alphas.end(), a, a + baseMesh.get().GetTotalVertexCount());
				}

				//--------------------------------------------------------------
				// Compile vertex AOVs
				//--------------------------------------------------------------

				if (baseMesh.get().HasVertexAOV(dataIndex)) {
					const float *v = baseMesh.get().GetVertexAOVs(dataIndex).get();
					vertexAOVs.insert(vertexAOVs.end(), v, v + baseMesh.get().GetTotalVertexCount());
				}

				//--------------------------------------------------------------
				// Compile triangle AOVs
				//--------------------------------------------------------------

				if (baseMesh.get().HasTriAOV(dataIndex)) {
					const float *t = baseMesh.get().GetTriAOVs(dataIndex).get();
					triAOVs.insert(triAOVs.end(), t, t + baseMesh.get().GetTotalTriangleCount());
				}
			}

			//------------------------------------------------------------------
			// Compile baseMesh vertices (expressed in local coordinates)
			//------------------------------------------------------------------

			const Points v = baseMesh.get().GetVertices();
			verts.insert(
				verts.end(),
				v.begin(),
				v.begin() + baseMesh.get().GetTotalVertexCount()
			);

			//------------------------------------------------------------------
			// Compile baseMesh triangle indices
			//------------------------------------------------------------------

			const auto t = baseMesh.get().GetTriangles();
			tris.insert(tris.end(), t.begin(), t.begin() + baseMesh.get().GetTotalTriangleCount());
		}

		meshDescs.push_back(currentMeshDesc);
	}

	worldBSphere = scene.GetDataSet().GetBSphere();

	const double tEnd = WallClockTime();
	SLG_LOG("Scene geometry compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
