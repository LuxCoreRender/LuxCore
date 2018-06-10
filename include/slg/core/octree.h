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

#ifndef _SLG_OCTREE_H
#define	_SLG_OCTREE_H

#include <vector>

#include "luxrays/core/geometry/bbox.h"

namespace slg {

template <typename NodeData> class Octree {
public:
	Octree(const luxrays::BBox &bbox, const u_int md = 24) :
		worldBBox(bbox), maxDepth(md) {
	}
	virtual ~Octree() { }
	
	void Add(const NodeData &dataItem, const luxrays::BBox &dataBBox) {
		AddImpl(&root, worldBBox, dataItem, dataBBox, (dataBBox.pMax - dataBBox.pMin).LengthSquared());
	}

	void GetAllData(std::vector<NodeData> &allData) const {
		GetAllDataImpl(&root, allData);
	}

	void DebugExport(const std::string &fileName, const float sphereRadius) const;
	
	luxrays::BBox worldBBox;
	u_int maxDepth;

private:
	class OctreeNode {
	public:
		OctreeNode() {
			for (u_int i = 0; i < 8; ++i)
				children[i] = NULL;
		}

		~OctreeNode() {
			for (u_int i = 0; i < 8; ++i)
				delete children[i];
		}

		OctreeNode *children[8];
		std::vector<NodeData> data;
	};

	luxrays::BBox ChildNodeBBox(u_int child, const luxrays::BBox &nodeBBox,
		const luxrays::Point &pMid) const {
		luxrays::BBox childBound;

		childBound.pMin.x = (child & 0x4) ? pMid.x : nodeBBox.pMin.x;
		childBound.pMax.x = (child & 0x4) ? nodeBBox.pMax.x : pMid.x;
		childBound.pMin.y = (child & 0x2) ? pMid.y : nodeBBox.pMin.y;
		childBound.pMax.y = (child & 0x2) ? nodeBBox.pMax.y : pMid.y;
		childBound.pMin.z = (child & 0x1) ? pMid.z : nodeBBox.pMin.z;
		childBound.pMax.z = (child & 0x1) ? nodeBBox.pMax.z : pMid.z;

		return childBound;
	}

	void GetAllDataImpl(const OctreeNode *node, std::vector<NodeData> &allData) const {
		allData.insert(allData.end(), node->data.begin(), node->data.end());
		
		for (u_int child = 0; child < 8; ++child) {
			if (node->children[child])
				GetAllDataImpl(node->children[child], allData);
		}
	}

	void AddImpl(OctreeNode *node, const luxrays::BBox &nodeBBox,
		const NodeData &dataItem, const luxrays::BBox &dataBBox,
		const float dataBBoxDiagonal2, const u_int depth = 0) {
		// Check if I have to store the data in this node
		if ((depth == maxDepth) ||
				DistanceSquared(nodeBBox.pMin, nodeBBox.pMax) < dataBBoxDiagonal2) {
			node->data.push_back(dataItem);
			return;
		}

		// Determine which children the item overlaps
		const luxrays::Point pMid = .5 * (nodeBBox.pMin + nodeBBox.pMax);

		const bool x[2] = {
			dataBBox.pMin.x <= pMid.x,
			dataBBox.pMax.x > pMid.x
		};
		const bool y[2] = {
			dataBBox.pMin.y <= pMid.y,
			dataBBox.pMax.y > pMid.y
		};
		const bool z[2] = {
			dataBBox.pMin.z <= pMid.z,
			dataBBox.pMax.z > pMid.z
		};

		const bool overlap[8] = {
			bool(x[0] & y[0] & z[0]),
			bool(x[0] & y[0] & z[1]),
			bool(x[0] & y[1] & z[0]),
			bool(x[0] & y[1] & z[1]),
			bool(x[1] & y[0] & z[0]),
			bool(x[1] & y[0] & z[1]),
			bool(x[1] & y[1] & z[0]),
			bool(x[1] & y[1] & z[1])
		};

		for (u_int child = 0; child < 8; ++child) {
			if (!overlap[child])
				continue;

			// Allocated the child node if required
			if (!node->children[child])
				node->children[child] = new OctreeNode();

			// Add the data to each overlapping child
			const luxrays::BBox childBBox = ChildNodeBBox(child, nodeBBox, pMid);
			AddImpl(node->children[child], childBBox,
					dataItem, dataBBox, dataBBoxDiagonal2, depth + 1);
		}
	}

	OctreeNode root;
};

}

#endif	/* _SLG_OCTREE_H */
