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

#ifndef _LUXRAYS_MQBVHACCEL_H
#define	_LUXRAYS_MQBVHACCEL_H

#include <xmmintrin.h>

#include "luxrays/luxrays.h"
#include "luxrays/core/accelerator.h"
#include "luxrays/accelerators/qbvhaccel.h"

namespace luxrays {

class MQBVHAccel : public Accelerator {
public:
	MQBVHAccel(const Context *context, u_int fst, u_int sf);
	virtual ~MQBVHAccel();

	virtual AcceleratorType GetType() const { return ACCEL_QBVH; }
	virtual OpenCLKernels *NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize) const;
	virtual bool CanRunOnOpenCLDevice(OpenCLIntersectionDevice *device) const;
	virtual void Init(const std::deque<const Mesh *> &meshes,
		const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount);

	unsigned int GetNNodes() const { return nNodes; }
	QBVHNode *GetTree() const { return nodes; }
	unsigned int GetNLeafs() const { return nLeafs; }
	const vector<const Transform *> &GetTransforms() const { return leafsTransform; }
	const vector<const MotionSystem *> &GetMotionSystem() const { return leafsMotionSystem; }

	virtual bool Intersect(const Ray *ray, RayHit *hit) const;

	virtual bool DoesSupportUpdate() const { return true; }
	virtual void Update();

#if !defined(LUXRAYS_DISABLE_OPENCL)
	friend class OpenCLMQBVHKernels;
#endif

private:
	static bool MeshPtrCompare(const Mesh *, const Mesh *);

	void BuildTree(u_int start, u_int end, u_int *primsIndexes,
		BBox *primsBboxes, Point *primsCentroids, const BBox &nodeBbox,
		const BBox &centroidsBbox, int32_t parentIndex,
		int32_t childIndex, int depth);

	void CreateLeaf(int32_t parentIndex, int32_t childIndex,
		u_int start, const BBox &nodeBbox);

	int32_t CreateNode(int32_t parentIndex, int32_t childIndex,
		const BBox &nodeBbox);

	std::deque<const Mesh *> meshList;

	QBVHNode *nodes;
	u_int nNodes, maxNodes;
	BBox worldBound;

	u_int fullSweepThreshold;
	u_int skipFactor;

	u_int nLeafs;
	// Not using boost::unordered_map because because the key is a Mesh pointer
	std::map<const Mesh *, QBVHAccel *, bool (*)(const Mesh *, const Mesh *)> accels;
	QBVHAccel **leafs;
	vector<const Transform *> leafsTransform;
	vector<const MotionSystem *> leafsMotionSystem;

	const Context *ctx;
	bool initialized;
};

}

#endif	/* _LUXRAYS_MQBVHACCEL_H */
