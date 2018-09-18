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

#ifndef _SLG_LIGHTSTRATEGY_DLSCBVH_H
#define	_SLG_LIGHTSTRATEGY_DLSCBVH_H

#include <vector>

#include "slg/slg.h"

namespace slg {

typedef struct {
	union {
		// I can not use BBox/Point/Normal here because objects with a constructor are not
		// allowed inside an union.
		struct {
			float bboxMin[3];
			float bboxMax[3];
		} bvhNode;
		struct {
			unsigned int index;
		} entryLeaf;
	};
	// Most significant bit is used to mark leafs
	unsigned int nodeData;
} DLSCBVHArrayNode;

class DLSCBvh {
public:
	DLSCBvh(const std::vector<DLSCacheEntry *> &ae, const float r, const float na);
	virtual ~DLSCBvh();

	const DLSCacheEntry *GetEntry(const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume) const;
private:
	const std::vector<DLSCacheEntry *> &allEntries;
	float entryRadius, entryRadius2, entryNormalCosAngle;

	DLSCBVHArrayNode *arrayNodes;
	u_int nNodes;
};

}

#endif	/* _SLG_LIGHTSTRATEGY_DLSCBVH_H */
