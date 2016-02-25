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

#ifndef _LUXRAYS_BVHACCEL_H
#define	_LUXRAYS_BVHACCEL_H

#include <vector>
#include <boost/foreach.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/accelerator.h"
#include "luxrays/core/bvh/bvhbuild.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/accelerators/bvh_types.cl"
}

#define BVHNodeData_IsLeaf(nodeData) ((nodeData) & 0x80000000u)
#define BVHNodeData_GetSkipIndex(nodeData) ((nodeData) & 0x7fffffffu)

// BVHAccel Declarations
class BVHAccel : public Accelerator {
public:
	// BVHAccel Public Methods
	BVHAccel(const Context *context,
			const u_int treetype, const int csamples, const int icost,
			const int tcost, const float ebonus);
	virtual ~BVHAccel();

	virtual AcceleratorType GetType() const { return ACCEL_BVH; }
	virtual OpenCLKernels *NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize) const;
	virtual void Init(const std::deque<const Mesh *> &meshes,
		const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount);

	virtual bool Intersect(const Ray *ray, RayHit *hit) const;

	friend class MBVHAccel;
#if !defined(LUXRAYS_DISABLE_OPENCL)
	friend class OpenCLBVHKernels;
	friend class OpenCLMBVHKernels;
#endif

private:
	// BVHAccel Private Methods

	static u_int BuildArray(const std::deque<const Mesh *> *meshes, BVHTreeNode *node,
		u_int offset, luxrays::ocl::BVHAccelArrayNode *bvhTree);

	BVHParams params;

	u_int nNodes;
	luxrays::ocl::BVHAccelArrayNode *bvhTree;

	const Context *ctx;
	std::deque<const Mesh *> meshes;
	u_longlong totalVertexCount, totalTriangleCount;

	bool initialized;
};

}

#endif	/* _LUXRAYS_BVHACCEL_H */
