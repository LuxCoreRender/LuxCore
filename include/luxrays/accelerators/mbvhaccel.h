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

#ifndef _LUXRAYS_MBVHACCEL_H
#define	_LUXRAYS_MBVHACCEL_H

#include <vector>

#include "luxrays/luxrays.h"
#include "luxrays/accelerators/bvhaccel.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/geometry/motionsystem.h"

namespace luxrays {

// MBVHAccel Declarations
class MBVHAccel : public Accelerator {
public:
	// MBVHAccel Public Methods
	MBVHAccel(const Context *context);
	virtual ~MBVHAccel();

	virtual AcceleratorType GetType() const { return ACCEL_MBVH; }
	virtual OpenCLKernels *NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize) const;
	virtual void Init(const std::deque<const Mesh *> &meshes,
		const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount);

	virtual bool DoesSupportUpdate() const { return true; }
	virtual void Update();

	virtual bool Intersect(const Ray *ray, RayHit *hit) const;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	friend class OpenCLMBVHKernels;
#endif

private:
	static bool MeshPtrCompare(const Mesh *, const Mesh *);

	void UpdateRootBVH();

	BVHParams params;

	std::vector<BVHTreeNode> bvhLeafs;
	std::vector<BVHTreeNode *> bvhLeafsList;

	// The root BVH tree
	unsigned int nRootNodes;
	luxrays::ocl::BVHArrayNode *bvhRootTree;

	std::vector<const BVHAccel *> uniqueLeafs;
	std::vector<const Transform *> uniqueLeafsTransform;
	std::vector<const MotionSystem *> uniqueLeafsMotionSystem;
	
	const Context *ctx;
	std::deque<const Mesh *> meshes;

	bool initialized;
};

}

#endif	/* _LUXRAYS_MBVHACCEL_H */
