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

#ifndef _SLG_PGCIOCTREE_H
#define	_SLG_PGCIOCTREE_H

#include "slg/slg.h"
#include "slg/core/indexoctree.h"

namespace slg {

class VisibilityParticle;

class PGCIOctree : public IndexOctree<VisibilityParticle> {
public:
	PGCIOctree(const std::vector<VisibilityParticle> &allEntries, const luxrays::BBox &bbox,
			const float r, const float normAngle, const u_int md = 24);
	virtual ~PGCIOctree();

	u_int GetNearestEntry(const luxrays::Point &p, const luxrays::Normal &n) const;

private:
	void GetNearestEntryImpl(const IndexOctreeNode *node, const luxrays::BBox &nodeBBox,
		const luxrays::Point &p, const luxrays::Normal &n,
		u_int &nearestEntryIndex, float &nearestDistance2) const;
};

}

#endif	/* _SLG_PGCIOCTREE_H */
