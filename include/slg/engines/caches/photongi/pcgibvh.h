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

#ifndef _SLG_LIGHTSTRATEGY_PGICBVH_H
#define	_SLG_LIGHTSTRATEGY_PGICBVH_H

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
} PGICBVHArrayNode;

template <class T>
class PGICBvh {
public:
	PGICBvh(const std::vector<T> &entries, const float radius, const float normalAngle);
	virtual ~PGICBvh();

	const T *GetNearestEntry(const luxrays::Point &p, const luxrays::Normal &n) const;

	void GetAllNearEntries(std::vector<const T *> &entries,
			const luxrays::Point &p) const;
	void GetAllNearEntries(std::vector<const T *> &entries,
			const luxrays::Point &p, const luxrays::Normal &n) const;

private:
	const std::vector<T> &allEntries;
	float entryRadius, entryRadius2, entryNormalCosAngle;

	PGICBVHArrayNode *arrayNodes;
	u_int nNodes;
};

}

#endif	/* _SLG_LIGHTSTRATEGY_PGICBVH_H */
