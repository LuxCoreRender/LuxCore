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

#ifndef __SLG_INDEXBVH_H
#define	__SLG_INDEXBVH_H

#include <vector>

#include "slg/slg.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/core/indexbvh_types.cl"
}

//------------------------------------------------------------------------------
// Index BVH
//------------------------------------------------------------------------------

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
	int pad; // To align to float4
} IndexBVHArrayNode;

template <class T>
class IndexBvh {
public:
	IndexBvh(const std::vector<T> &entries, const float entryRadius);
	virtual ~IndexBvh();

	float GetEntryRadius() const { return entryRadius; }

	size_t GetMemoryUsage() const { return nNodes * sizeof(IndexBVHArrayNode); }

protected:
	const std::vector<T> &allEntries;
	float entryRadius, entryRadius2;

	IndexBVHArrayNode *arrayNodes;
	u_int nNodes;
};

}

#endif	/* __SLG_INDEXBVH_H */
