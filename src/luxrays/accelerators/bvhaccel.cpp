/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

// Boundary Volume Hierarchy accelerator
// Based of "Efficiency Issues for Ray Tracing" by Brian Smits
// Available at http://www.cs.utah.edu/~bes/papers/fastRT/paper.html

#include <iostream>
#include <functional>
#include <algorithm>
#include <limits>

#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/core/utils.h"
#include "luxrays/core/context.h"

using namespace std;

namespace luxrays {

// BVHAccel Method Definitions

BVHAccel::BVHAccel(const Context *context) : ctx(context) {
	params = ToBVHParams(ctx->GetConfig());

	initialized = false;
}

BVHAccel::~BVHAccel() {
	if (initialized)
		delete bvhTree;
}

BVHParams BVHAccel::ToBVHParams(const Properties &props) {
	// Tree type to generate (2 = binary, 4 = quad, 8 = octree)
	const int treeType = props.Get(Property("accelerator.bvh.treetype")(4)).Get<int>();
	// Samples to get for cost minimization
	const int costSamples = props.Get(Property("accelerator.bvh.costsamples")(0)).Get<int>();
	const int isectCost = props.Get(Property("accelerator.bvh.isectcost")(80)).Get<int>();
	const int travCost = props.Get(Property("accelerator.bvh.travcost")(10)).Get<int>();
	const float emptyBonus = props.Get(Property("accelerator.bvh.emptybonus")(.5)).Get<float>();
	
	BVHParams params;
	// Make sure treeType is 2, 4 or 8
	if (treeType <= 2) params.treeType = 2;
	else if (treeType <= 4) params.treeType = 4;
	else params.treeType = 8;

	params.costSamples = costSamples;
	params.isectCost = isectCost;
	params.traversalCost = travCost;
	params.emptyBonus = emptyBonus;

	return params;
}

void BVHAccel::Init(const deque<const Mesh *> &ms, const u_longlong totVert,
		const u_longlong totTri) {
	assert (!initialized);

	meshes = ms;
	totalVertexCount = totVert;
	totalTriangleCount = totTri;

	// Handle the empty DataSet case
	if (totalTriangleCount == 0) {
		LR_LOG(ctx, "Empty BVH");
		nNodes = 0;
		bvhTree = NULL;
		initialized = true;

		return;
	}

	const double t0 = WallClockTime();

	//--------------------------------------------------------------------------
	// Build the list of triangles
	//--------------------------------------------------------------------------
	
	vector<BVHTreeNode> bvNodes(totalTriangleCount);
	vector<BVHTreeNode *> bvList(totalTriangleCount, NULL);
	u_int meshIndex = 0;
	u_int bvListIndex = 0;
	BOOST_FOREACH(const Mesh *mesh, meshes) {
		const Triangle *p = mesh->GetTriangles();
		const u_int triangleCount = mesh->GetTotalTriangleCount();

		#pragma omp parallel for
		for (
				// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
				unsigned
#endif
				int i = 0; i < triangleCount; ++i) {
			const int index = bvListIndex + i;
			BVHTreeNode *node = &bvNodes[index];

			node->bbox = Union(
					BBox(mesh->GetVertex(0.f, p[i].v[0]), mesh->GetVertex(0.f, p[i].v[1])),
					mesh->GetVertex(0.f, p[i].v[2]));
			// NOTE - Ratow - Expand bbox a little to make sure rays collide
			node->bbox.Expand(MachineEpsilon::E(node->bbox));
			node->triangleLeaf.meshIndex = meshIndex;
			node->triangleLeaf.triangleIndex = i;

			node->leftChild = NULL;
			node->rightSibling = NULL;

			bvList[index] = node;
		}
		
		bvListIndex += triangleCount;
		++meshIndex;
	}

	LR_LOG(ctx, "BVH Dataset preprocessing time: " << int((WallClockTime() - t0) * 1000) << "ms");

	//--------------------------------------------------------------------------
	// Build the BVH hierarchy
	//--------------------------------------------------------------------------

	const double t1 = WallClockTime();

	const string builderType = ctx->GetConfig().Get(Property("accelerator.bvh.builder.type")("EMBREE_BINNED_SAH")).Get<string>();
	LR_LOG(ctx, "BVH builder: " << builderType);
	if (builderType == "EMBREE_BINNED_SAH")
		bvhTree = BuildEmbreeBVHBinnedSAH(params, &nNodes, &meshes, bvList);
	else if (builderType == "EMBREE_MORTON")
		bvhTree = BuildEmbreeBVHMorton(params, &nNodes, &meshes, bvList);
	else if (builderType == "CLASSIC")
		bvhTree = BuildBVH(params, &nNodes, &meshes, bvList);
	else
		throw runtime_error("Unknown BVH builder type in BVHAccel::Init(): " + builderType);

	LR_LOG(ctx, "BVH build hierarchy time: " << int((WallClockTime() - t1) * 1000) << "ms");

	//--------------------------------------------------------------------------
	// Done
	//--------------------------------------------------------------------------

	LR_LOG(ctx, "BVH total build time: " << int((WallClockTime() - t0) * 1000) << "ms");
	LR_LOG(ctx, "Total BVH memory usage: " << nNodes * sizeof(luxrays::ocl::BVHArrayNode) / 1024 << "Kbytes");

	initialized = true;
}

bool BVHAccel::Intersect(const Ray *initialRay, RayHit *rayHit) const {
	assert (initialized);

	rayHit->t = initialRay->maxt;
	rayHit->SetMiss();
	if (!nNodes)
		return false;

	Ray ray(*initialRay);

	u_int currentNode = 0; // Root Node
	const u_int stopNode = BVHNodeData_GetSkipIndex(bvhTree[0].nodeData); // Non-existent

	float t, b1, b2;
	while (currentNode < stopNode) {
		const luxrays::ocl::BVHArrayNode &node = bvhTree[currentNode];

		const u_int nodeData = node.nodeData;
		if (BVHNodeData_IsLeaf(nodeData)) {
			// It is a leaf, check the triangle
			const Mesh *mesh = meshes[node.triangleLeaf.meshIndex];
			const Point p0 = mesh->GetVertex(0.f, node.triangleLeaf.v[0]);
			const Point p1 = mesh->GetVertex(0.f, node.triangleLeaf.v[1]);
			const Point p2 = mesh->GetVertex(0.f, node.triangleLeaf.v[2]);

			if (Triangle::Intersect(ray, p0, p1, p2, &t, &b1, &b2)) {
				if (t < rayHit->t) {
					ray.maxt = t;
					rayHit->t = t;
					rayHit->b1 = b1;
					rayHit->b2 = b2;
					rayHit->meshIndex = node.triangleLeaf.meshIndex;
					rayHit->triangleIndex = node.triangleLeaf.triangleIndex;
					// Continue testing for closer intersections
				}
			}

			++currentNode;
		} else {
			// It is a node, check the bounding box
			if (BBox::IntersectP(ray,
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
