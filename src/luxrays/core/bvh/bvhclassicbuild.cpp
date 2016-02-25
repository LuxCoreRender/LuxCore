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

// Boundary Volume Hierarchy
// Based of "Efficiency Issues for Ray Tracing" by Brian Smits
// Available at http://www.cs.utah.edu/~bes/papers/fastRT/paper.html

#include <iostream>
#include <functional>
#include <algorithm>
#include <limits>

#include "luxrays/core/bvh/bvhbuild.h"

using std::bind2nd;
using std::ptr_fun;

namespace luxrays {

// Build an array of comparators for each axis

static bool bvh_ltf_x(BVHTreeNode *n, float v) {
	return n->bbox.pMax.x + n->bbox.pMin.x < v;
}

static bool bvh_ltf_y(BVHTreeNode *n, float v) {
	return n->bbox.pMax.y + n->bbox.pMin.y < v;
}

static bool bvh_ltf_z(BVHTreeNode *n, float v) {
	return n->bbox.pMax.z + n->bbox.pMin.z < v;
}

static bool (* const bvh_ltf[3])(BVHTreeNode *n, float v) = { bvh_ltf_x, bvh_ltf_y, bvh_ltf_z };

static void FindBestSplit(const BVHParams &params, std::vector<BVHTreeNode *> &list,
		u_int begin, u_int end, float *splitValue, u_int *bestAxis) {
	if (end - begin == 2) {
		// Trivial case with two elements
		*splitValue = (list[begin]->bbox.pMax[0] + list[begin]->bbox.pMin[0] +
				list[end - 1]->bbox.pMax[0] + list[end - 1]->bbox.pMin[0]) / 2;
		*bestAxis = 0;
	} else {
		// Calculate BBs mean center (times 2)
		Point mean2(0, 0, 0), var(0, 0, 0);
		for (u_int i = begin; i < end; i++)
			mean2 += list[i]->bbox.pMax + list[i]->bbox.pMin;
		mean2 /= static_cast<float>(end - begin);

		// Calculate variance
		for (u_int i = begin; i < end; i++) {
			Vector v = list[i]->bbox.pMax + list[i]->bbox.pMin - mean2;
			v.x *= v.x;
			v.y *= v.y;
			v.z *= v.z;
			var += v;
		}
		// Select axis with more variance
		if (var.x > var.y && var.x > var.z)
			*bestAxis = 0;
		else if (var.y > var.z)
			*bestAxis = 1;
		else
			*bestAxis = 2;

		if (params.costSamples > 1) {
			BBox nodeBounds;
			for (u_int i = begin; i < end; i++)
				nodeBounds = Union(nodeBounds, list[i]->bbox);

			Vector d = nodeBounds.pMax - nodeBounds.pMin;
			const float invTotalSA = 1.f / nodeBounds.SurfaceArea();

			// Sample cost for split at some points
			const float increment = 2 * d[*bestAxis] / (params.costSamples + 1);
			float bestCost = INFINITY;
			for (float splitVal = 2 * nodeBounds.pMin[*bestAxis] + increment; splitVal < 2 * nodeBounds.pMax[*bestAxis]; splitVal += increment) {
				int nBelow = 0, nAbove = 0;
				BBox bbBelow, bbAbove;
				for (u_int j = begin; j < end; j++) {
					if ((list[j]->bbox.pMax[*bestAxis] + list[j]->bbox.pMin[*bestAxis]) < splitVal) {
						nBelow++;
						bbBelow = Union(bbBelow, list[j]->bbox);
					} else {
						nAbove++;
						bbAbove = Union(bbAbove, list[j]->bbox);
					}
				}
				const float pBelow = bbBelow.SurfaceArea() * invTotalSA;
				const float pAbove = bbAbove.SurfaceArea() * invTotalSA;
				const float eb = (nAbove == 0 || nBelow == 0) ? params.emptyBonus : 0.f;
				const float cost = params.traversalCost + params.isectCost * (1.f - eb) * (pBelow * nBelow + pAbove * nAbove);
				// Update best split if this is lowest cost so far
				if (cost < bestCost) {
					bestCost = cost;
					*splitValue = splitVal;
				}
			}
		} else {
			// Split in half around the mean center
			*splitValue = mean2[*bestAxis];
		}
	}
}

static BVHTreeNode *BuildBVH(u_int *nNodes, const BVHParams &params,
		std::vector<BVHTreeNode *> &list, u_int begin, u_int end, u_int axis) {
	u_int splitAxis = axis;
	float splitValue;

	*nNodes += 1;
	if (end - begin == 1) {
		// Only a single item in list so return it
		BVHTreeNode *node = new BVHTreeNode();
		*node = *(list[begin]);
		return node;
	}

	BVHTreeNode *parent = new BVHTreeNode();
	parent->leftChild = NULL;
	parent->rightSibling = NULL;

	std::vector<u_int> splits;
	splits.reserve(params.treeType + 1);
	splits.push_back(begin);
	splits.push_back(end);
	for (u_int i = 2; i <= params.treeType; i *= 2) { // Calculate splits, according to tree type and do partition
		for (u_int j = 0, offset = 0; j + offset < i && splits.size() > j + 1; j += 2) {
			if (splits[j + 1] - splits[j] < 2) {
				j--;
				offset++;
				continue; // Less than two elements: no need to split
			}

			FindBestSplit(params, list, splits[j], splits[j + 1], &splitValue, &splitAxis);

			std::vector<BVHTreeNode *>::iterator it =
					partition(list.begin() + splits[j], list.begin() + splits[j + 1], bind2nd(ptr_fun(bvh_ltf[splitAxis]), splitValue));
			u_int middle = distance(list.begin(), it);
			middle = Max(splits[j] + 1, Min(splits[j + 1] - 1, middle)); // Make sure coincidental BBs are still split
			splits.insert(splits.begin() + j + 1, middle);
		}
	}

	// Left Child
	BVHTreeNode *child = BuildBVH(nNodes, params, list, splits[0], splits[1], splitAxis);
	parent->leftChild = child;
	parent->bbox = child->bbox;
	BVHTreeNode *lastChild = child;

	// Add remaining children
	for (u_int i = 1; i < splits.size() - 1; i++) {
		child = BuildBVH(nNodes, params, list, splits[i], splits[i + 1], splitAxis);
		lastChild->rightSibling = child;
		parent->bbox = Union(parent->bbox, child->bbox);
		lastChild = child;
	}

	return parent;
}

BVHTreeNode *BuildBVH(u_int *nNodes, const BVHParams &params, std::vector<BVHTreeNode *> &list) {
	return BuildBVH(nNodes, params, list, 0, list.size(), 2);
}

}
