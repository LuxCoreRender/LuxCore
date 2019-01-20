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

#ifndef _SLG_LIGHTSTRATEGY_DLSCOCTREE_H
#define	_SLG_LIGHTSTRATEGY_DLSCOCTREE_H

#include <vector>

#include "luxrays/core/geometry/bbox.h"
#include "slg/slg.h"
#include "slg/bsdf/bsdf.h"
#include "slg/scene/scene.h"
#include "slg/samplers/sampler.h"
#include "slg/utils/pathdepthinfo.h"

namespace slg {

class DLSCOctree {
public:
	DLSCOctree(const std::vector<DLSCacheEntry> &allEntries, const luxrays::BBox &bbox,
			const float r, const float normAngle, const u_int md = 24);
	~DLSCOctree();

	void Add(const u_int entryIndex);

	u_int GetEntry(const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume) const;
	void GetAllNearEntries(std::vector<u_int> &entriesIndex,
			const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume,
			const float radius) const;

private:
	class DLSCOctreeNode {
	public:
		DLSCOctreeNode() {
			for (u_int i = 0; i < 8; ++i)
				children[i] = NULL;
		}

		~DLSCOctreeNode() {
			for (u_int i = 0; i < 8; ++i)
				delete children[i];
		}

		DLSCOctreeNode *children[8];
		std::vector<u_int> entriesIndex;
	};

	luxrays::BBox ChildNodeBBox(u_int child, const luxrays::BBox &nodeBBox,
		const luxrays::Point &pMid) const;

	void AddImpl(DLSCOctreeNode *node, const luxrays::BBox &nodeBBox,
		const u_int entryIndex, const luxrays::BBox &entryBBox,
		const float entryBBoxDiagonal2, const u_int depth = 0);

	u_int GetEntryImpl(const DLSCOctreeNode *node, const luxrays::BBox &nodeBBox,
		const luxrays::Point &p, const luxrays::Normal &n, const bool isVolume) const;
	void GetAllNearEntriesImpl(std::vector<u_int> &entries,
			const DLSCOctreeNode *node, const luxrays::BBox &nodeBBox,
			const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume,
			const luxrays::BBox areaBBox,
			const float areaRadius2) const;

	const std::vector<DLSCacheEntry> &allEntries;

	luxrays::BBox worldBBox;
	
	u_int maxDepth;
	float entryRadius, entryRadius2, entryNormalCosAngle;
	
	DLSCOctreeNode root;
};

}

#endif	/* _SLG_LIGHTSTRATEGY_DLSCOCTREE_H */
