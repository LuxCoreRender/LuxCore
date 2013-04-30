/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _LUXRAYS_BVHACCEL_H
#define	_LUXRAYS_BVHACCEL_H

#include <vector>

#include "luxrays/luxrays.h"
#include "luxrays/core/accelerator.h"

namespace luxrays {

struct BVHAccelTreeNode {
	BBox bbox;
	union {
		struct {
			u_int index;
		} triangleLeaf;
		struct {
			u_int leafIndex;
			u_int transformIndex;
			u_int triangleOffsetIndex;
		} bvhLeaf;
	};

	BVHAccelTreeNode *leftChild;
	BVHAccelTreeNode *rightSibling;
};

struct BVHAccelArrayNode {
	union {
		struct {
			// I can not use BBox here because objects with a constructor are not
			// allowed inside an union.
			float bboxMin[3];
			float bboxMax[3];
		} bvhNode;
		struct {
			u_int v[3];
			u_int triangleIndex;
		} triangleLeaf;
		struct {
			u_int leafIndex;
			u_int transformIndex;
			u_int triangleOffsetIndex;
		} bvhLeaf; // Used by MBVH
	};
	// Most significant bit is used to mark leafs
	u_int nodeData;
	int pad0; // To align to float4
};

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
		const u_int kernelCount, const u_int stackSize, const bool enableImageStorage) const;
	virtual void Init(const std::deque<const Mesh *> &meshes,
		const u_int totalVertexCount,
		const u_int totalTriangleCount);

	virtual bool Intersect(const Ray *ray, RayHit *hit) const;

	friend class MBVHAccel;
#if !defined(LUXRAYS_DISABLE_OPENCL)
	friend class OpenCLBVHKernels;
	friend class OpenCLMBVHKernels;
#endif

private:
	typedef struct {
		u_int treeType;
		int costSamples, isectCost, traversalCost;
		float emptyBonus;
	} BVHParams;

	// BVHAccel Private Methods
	static BVHAccelTreeNode *BuildHierarchy(u_int *nNodes, const BVHParams &params,
		std::vector<BVHAccelTreeNode *> &list,
		u_int begin, u_int end, u_int axis);
	static void FreeHierarchy(BVHAccelTreeNode *node);
	static void FindBestSplit(const BVHParams &params,
		std::vector<BVHAccelTreeNode *> &list,
		u_int begin, u_int end, float *splitValue,
		u_int *bestAxis);

	static u_int BuildArray(const Triangle *triangles, BVHAccelTreeNode *node,
		u_int offset, BVHAccelArrayNode *bvhTree);

	// A special initialization method used only by MBVHAccel
	void Init(const Mesh *m);

	BVHParams params;

	u_int nNodes;
	BVHAccelArrayNode *bvhTree;

	const Context *ctx;
	TriangleMesh *preprocessedMesh;
	const Mesh *mesh;

	bool initialized;
};

}

#endif	/* _LUXRAYS_BVHACCEL_H */
