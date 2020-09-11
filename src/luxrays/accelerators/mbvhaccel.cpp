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

// Boundary Volume Hierarchy accelerator
// Based of "Efficiency Issues for Ray Tracing" by Brian Smits
// Available at http://www.cs.utah.edu/~bes/papers/fastRT/paper.html

#include <iostream>
#include <functional>
#include <algorithm>
#include <limits>
#include <boost/foreach.hpp>

#include "luxrays/accelerators/mbvhaccel.h"
#include "luxrays/core/context.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/utils/strutils.h"

using namespace std;

namespace luxrays {

// MBVHAccel Method Definitions

MBVHAccel::MBVHAccel(const Context *context) : ctx(context) {
	params = BVHAccel::ToBVHParams(ctx->GetConfig());

	initialized = false;
}

MBVHAccel::~MBVHAccel() {
	if (initialized) {
		BOOST_FOREACH(const BVHAccel *bvh, uniqueLeafs)
			delete bvh;
		delete bvhRootTree;
	}
}

bool MBVHAccel::MeshPtrCompare(const Mesh *p0, const Mesh *p1) {
	return p0 < p1;
}

void MBVHAccel::Init(const deque<const Mesh *> &ms, const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount) {
	assert (!initialized);

	// Handle the empty DataSet case
	if (totalTriangleCount == 0) {
		LR_LOG(ctx, "Empty MBVH");
		nRootNodes = 0;
		bvhRootTree = NULL;
		initialized = true;

		return;
	}

	meshes = ms;

	const double t0 = WallClockTime();

	//--------------------------------------------------------------------------
	// Build all BVH leafs
	//--------------------------------------------------------------------------

	const u_int nLeafs = meshes.size();
	LR_LOG(ctx, "Building Multilevel Bounding Volume Hierarchy: " << nLeafs << " leafs");

	vector<u_int> leafsIndex;
	vector<u_int> leafsTransformIndex;
	vector<u_int> leafsMotionSystemIndex;

	leafsIndex.reserve(nLeafs);

	map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)> uniqueLeafIndexByMesh(MeshPtrCompare);

	double lastPrint = WallClockTime();
	for (u_int i = 0; i < nLeafs; ++i) {
		const double now = WallClockTime();
		if (now - lastPrint > 2.0) {
			LR_LOG(ctx, "Building BVH for MBVH leaf: " << i << "/" << nLeafs);
			lastPrint = now;
		}

		const Mesh *mesh = meshes[i];

		switch (mesh->GetType()) {
			case TYPE_TRIANGLE:
			case TYPE_EXT_TRIANGLE: {
				BVHAccel *leaf = new BVHAccel(ctx);
				deque<const Mesh *> mlist(1, mesh);
				leaf->Init(mlist, mesh->GetTotalVertexCount(), mesh->GetTotalTriangleCount());

				const u_int uniqueLeafIndex = uniqueLeafs.size();
				uniqueLeafIndexByMesh[mesh] = uniqueLeafIndex;
				uniqueLeafs.push_back(leaf);
				leafsIndex.push_back(uniqueLeafIndex);
				leafsTransformIndex.push_back(NULL_INDEX);
				leafsMotionSystemIndex.push_back(NULL_INDEX);
				break;
			}
			case TYPE_TRIANGLE_INSTANCE:
			case TYPE_EXT_TRIANGLE_INSTANCE: {
				const InstanceTriangleMesh *itm = dynamic_cast<const InstanceTriangleMesh *>(mesh);

				// Check if a BVH has already been created
				map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)>::iterator it =
						uniqueLeafIndexByMesh.find(itm->GetTriangleMesh());

				if (it == uniqueLeafIndexByMesh.end()) {
					TriangleMesh *instancedMesh = itm->GetTriangleMesh();

					// Create a new BVH
					BVHAccel *leaf = new BVHAccel(ctx);
					deque<const Mesh *> mlist(1, instancedMesh);
					leaf->Init(mlist, instancedMesh->GetTotalVertexCount(), instancedMesh->GetTotalTriangleCount());

					const u_int uniqueLeafIndex = uniqueLeafs.size();
					uniqueLeafIndexByMesh[instancedMesh] = uniqueLeafIndex;
					uniqueLeafs.push_back(leaf);
					leafsIndex.push_back(uniqueLeafIndex);
				} else {
					//LR_LOG(ctx, "Cached BVH leaf");
					leafsIndex.push_back(it->second);
				}

				leafsTransformIndex.push_back(uniqueLeafsTransform.size());
				uniqueLeafsTransform.push_back(&itm->GetTransformation());
				leafsMotionSystemIndex.push_back(NULL_INDEX);
				break;
			}
			case TYPE_TRIANGLE_MOTION:
			case TYPE_EXT_TRIANGLE_MOTION: {
				const MotionTriangleMesh *mtm = dynamic_cast<const MotionTriangleMesh *>(mesh);

				// Check if a BVH has already been created
				map<const Mesh *, u_int, bool (*)(const Mesh *, const Mesh *)>::iterator it =
						uniqueLeafIndexByMesh.find(mtm->GetTriangleMesh());

				if (it == uniqueLeafIndexByMesh.end()) {
					TriangleMesh *motionMesh = mtm->GetTriangleMesh();

					// Create a new BVH
					BVHAccel *leaf = new BVHAccel(ctx);
					deque<const Mesh *> mlist(1, motionMesh);
					leaf->Init(mlist, motionMesh->GetTotalVertexCount(), motionMesh->GetTotalTriangleCount());

					const u_int uniqueLeafIndex = uniqueLeafs.size();
					uniqueLeafIndexByMesh[motionMesh] = uniqueLeafIndex;
					uniqueLeafs.push_back(leaf);
					leafsIndex.push_back(uniqueLeafIndex);
				} else {
					//LR_LOG(ctx, "Cached BVH leaf");
					leafsIndex.push_back(it->second);
				}

				leafsMotionSystemIndex.push_back(uniqueLeafsMotionSystem.size());
				uniqueLeafsMotionSystem.push_back(&mtm->GetMotionSystem());
				leafsTransformIndex.push_back(NULL_INDEX);
				break;
			}
			default:
				throw runtime_error("Unknown Mesh type in MBVHAccel::Init(): " + ToString(mesh->GetType()));
		}
	}

	//--------------------------------------------------------------------------
	// Build the root BVH
	//--------------------------------------------------------------------------

	LR_LOG(ctx, "Building Multilevel Bounding Volume Hierarchy root tree");

	bvhLeafs.resize(nLeafs);
	bvhLeafsList.resize(nLeafs, NULL);
	for (u_int i = 0; i < nLeafs; ++i) {
		BVHTreeNode *bvhLeaf = &bvhLeafs[i];
		// Get the bounding box from the mesh so it is in global coordinates
		bvhLeaf->bbox = meshes[i]->GetBBox();
		bvhLeaf->bbox.Expand(MachineEpsilon::E(bvhLeaf->bbox));

		bvhLeaf->bvhLeaf.leafIndex = leafsIndex[i];
		bvhLeaf->bvhLeaf.transformIndex = leafsTransformIndex[i];
		bvhLeaf->bvhLeaf.motionIndex = leafsMotionSystemIndex[i];
		bvhLeaf->bvhLeaf.meshOffsetIndex = i;
		bvhLeaf->leftChild = NULL;
		bvhLeaf->rightSibling = NULL;
		bvhLeafsList[i] = bvhLeaf;
	}

	bvhRootTree = NULL;
	UpdateRootBVH();

	LR_LOG(ctx, "MBVH build time: " << int((WallClockTime() - t0) * 1000) << "ms");

	size_t totalMem = nRootNodes;
	BOOST_FOREACH(const BVHAccel *bvh, uniqueLeafs)
		totalMem += bvh->nNodes;
	totalMem *= sizeof(luxrays::ocl::BVHArrayNode);
	LR_LOG(ctx, "Total Multilevel BVH memory usage: " << totalMem / 1024 << "Kbytes");

	initialized = true;
}

void MBVHAccel::UpdateRootBVH() {
	delete bvhRootTree;
	bvhRootTree = NULL;

	const string builderType = ctx->GetConfig().Get(Property("accelerator.bvh.builder.type")(
		"EMBREE_BINNED_SAH"
		)).Get<string>();

	LR_LOG(ctx, "MBVH root tree builder: " << builderType);
	if (builderType == "CLASSIC")
		bvhRootTree = BuildBVH(params, &nRootNodes, NULL, bvhLeafsList);
	else if (builderType == "EMBREE_BINNED_SAH")
		bvhRootTree = BuildEmbreeBVHBinnedSAH(params, &nRootNodes, NULL, bvhLeafsList);
	else if (builderType == "EMBREE_MORTON")
		bvhRootTree = BuildEmbreeBVHMorton(params, &nRootNodes, NULL, bvhLeafsList);
	else
		throw runtime_error("Unknown BVH builder type in MBVHAccel::UpdateRootBVH(): " + builderType);
}

void MBVHAccel::Update() {
	// Update the BVH leaf bounding box
	const u_int nLeafs = meshes.size();
	for (u_int i = 0; i < nLeafs; ++i) {
		BVHTreeNode *bvhLeaf = &bvhLeafs[i];
		// Get the bounding box from the mesh so it is in global coordinates
		bvhLeaf->bbox = meshes[i]->GetBBox();
	}

	// Update the root BVH tree
	UpdateRootBVH();
	
	// Nothing else to do because uniqueLeafsTransform is a list of pointers
}

bool MBVHAccel::Intersect(const Ray *ray, RayHit *rayHit) const {
	assert (initialized);

	rayHit->t = ray->maxt;
	rayHit->SetMiss();
	if (!nRootNodes)
		return false;

	bool insideLeafTree = false;
	u_int currentRootNode = 0;
	const u_int rootStopNode = BVHNodeData_GetSkipIndex(bvhRootTree[0].nodeData); // Non-existent
	u_int currentNode = currentRootNode;
	u_int currentStopNode = rootStopNode; // Non-existent
	u_int currentMeshOffset = 0;
	luxrays::ocl::BVHArrayNode *currentTree = bvhRootTree;

	Ray currentRay(*ray);

	for (;;) {
		if (currentNode >= currentStopNode) {
			if (insideLeafTree) {
				// Go back to the root tree
				currentTree = bvhRootTree;
				currentNode = currentRootNode;
				currentStopNode = rootStopNode;
				currentRay = *ray;
				currentRay.maxt = rayHit->t;
				insideLeafTree = false;

				// Check if the leaf was the very last root node
				if (currentNode >= currentStopNode)
					break;
			} else {
				// Done
				break;
			}
		}

		const luxrays::ocl::BVHArrayNode &node = currentTree[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			if (insideLeafTree) {
				// I'm inside a leaf tree, I have to check the triangle
				const u_int absoluteMeshIndex = node.triangleLeaf.meshIndex + currentMeshOffset;
				const Mesh *currentMesh = meshes[absoluteMeshIndex];
				// I use currentMesh->GetVertices() in order to have access to not
				// transformed vertices in the case of instances
				const Point *vertices = currentMesh->GetVertices();
				const Point &p0 = vertices[node.triangleLeaf.v[0]];
				const Point &p1 = vertices[node.triangleLeaf.v[1]];
				const Point &p2 = vertices[node.triangleLeaf.v[2]];

				float t, b1, b2;
				if (Triangle::Intersect(currentRay, p0, p1, p2, &t, &b1, &b2)) {
					if (t < rayHit->t) {
						currentRay.maxt = t;
						rayHit->t = t;
						rayHit->b1 = b1;
						rayHit->b2 = b2;
						rayHit->meshIndex = absoluteMeshIndex;
						rayHit->triangleIndex = node.triangleLeaf.triangleIndex;
						// Continue testing for closer intersections
					}
				}

				++currentNode;
			} else {
				// I have to check a leaf tree
				currentTree = uniqueLeafs[node.bvhLeaf.leafIndex]->bvhTree;

				// Transform the ray in the local coordinate system
				if (node.bvhLeaf.transformIndex != NULL_INDEX)
					currentRay = Ray(Inverse(*uniqueLeafsTransform[node.bvhLeaf.transformIndex]) * (*ray));
				else if (node.bvhLeaf.motionIndex != NULL_INDEX)
					currentRay = Ray(uniqueLeafsMotionSystem[node.bvhLeaf.motionIndex]->Sample(ray->time) * (*ray));
				else
					currentRay = (*ray);

				currentRay.maxt = rayHit->t;

				currentMeshOffset = node.bvhLeaf.meshOffsetIndex;

				currentRootNode = currentNode + 1;
				currentNode = 0;
				currentStopNode = BVHNodeData_GetSkipIndex(currentTree[0].nodeData);

				// Now, I'm inside a leaf tree
				insideLeafTree = true;
			}
		} else {
			// It is a node, check the bounding box
			if (BBox::IntersectP(currentRay,
					*reinterpret_cast<const Point *>(&node.bvhNode.bboxMin[0]),
					*reinterpret_cast<const Point *>(&node.bvhNode.bboxMax[0])))
				++currentNode;
			else {
				// I don't need to use BVHNodeData_GetSkipIndex() here because
				// I already know the leaf flag is 0
				currentNode = nodeData;
			}
		}
	}

	return !rayHit->Miss();
}

}
