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

#ifndef __SLG_INDEXOCTREE_H
#define	__SLG_INDEXOCTREE_H

#include <vector>

#include "luxrays/core/geometry/bbox.h"
#include "slg/slg.h"

namespace slg {

template <class T>
class IndexOctree {
public:
	IndexOctree(const std::vector<T> &allEntries, const luxrays::BBox &bbox,
			const float r, const float normAngle, const u_int md = 24);
	virtual ~IndexOctree();

	// This method is not thread safe
	void Add(const u_int entryIndex);

protected:
	class IndexOctreeNode {
	public:
		IndexOctreeNode() {
			for (u_int i = 0; i < 8; ++i)
				children[i] = NULL;
		}

		~IndexOctreeNode() {
			for (u_int i = 0; i < 8; ++i)
				delete children[i];
		}

		IndexOctreeNode *children[8];
		std::vector<u_int> entriesIndex;
	};

	luxrays::BBox ChildNodeBBox(u_int child, const luxrays::BBox &nodeBBox,
		const luxrays::Point &pMid) const;

	void AddImpl(IndexOctreeNode *node, const luxrays::BBox &nodeBBox,
		const u_int entryIndex, const luxrays::BBox &entryBBox,
		const float entryBBoxDiagonal2, const u_int depth = 0);

	const std::vector<T> &allEntries;

	luxrays::BBox worldBBox;
	
	u_int maxDepth;
	float entryRadius, entryRadius2, entryNormalCosAngle;
	
	IndexOctreeNode root;
};

}

#endif	/* __SLG_INDEXOCTREE_H */
