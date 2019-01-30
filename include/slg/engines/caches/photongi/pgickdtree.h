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

#ifndef _SLG_PGICKDTREE_H
#define	_SLG_PGICKDTREE_H

#include <vector>

#include "slg/slg.h"
#include "slg/core/indexkdtree.h"

namespace slg {

//------------------------------------------------------------------------------
// PGICPhotonKdTree
//------------------------------------------------------------------------------

class Photon;
class NearPhoton;

class PGICPhotonKdTree : public IndexKdTree<Photon> {
public:
	PGICPhotonKdTree(const std::vector<Photon> &entries, const u_int entryMaxLookUpCount,
			const float normalAngle);
	virtual ~PGICPhotonKdTree();

	u_int GetEntryMaxLookUpCount() const { return entryMaxLookUpCount; }
	float GetEntryNormalCosAngle() const { return entryNormalCosAngle; }
	
	void GetAllNearEntries(std::vector<NearPhoton> &entries,
			const luxrays::Point &p, const luxrays::Normal &n,
			float &maxDistance2) const;

private:
//	void GetAllNearEntriesImpl(const u_int currentNodeIndex,
//			std::vector<NearPhoton> &entries,
//			const luxrays::Point &p, const luxrays::Normal &n,
//			float &maxDistance2) const;

	const u_int entryMaxLookUpCount;
	float entryNormalCosAngle;
};

}

#endif	/* _SLG_PGICKDTREE_H */
