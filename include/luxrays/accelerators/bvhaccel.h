/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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
#include "luxrays/core/acceleretor.h"

namespace luxrays {

struct BVHAccelTreeNode {
	BBox bbox;
	unsigned int primitive;
	BVHAccelTreeNode *leftChild;
	BVHAccelTreeNode *rightSibling;
};

struct BVHAccelArrayNode {
	BBox bbox;
	unsigned int primitive;
	unsigned int skipIndex;
};

// BVHAccel Declarations
class BVHAccel : public Accelerator {
public:
	// BVHAccel Public Methods
	BVHAccel(const Context *context,
			const unsigned int treetype, const int csamples, const int icost,
			const int tcost, const float ebonus);
	virtual ~BVHAccel();

	virtual AcceleratorType GetType() const { return ACCEL_BVH; }
	virtual OpenCLKernel *NewOpenCLKernel(OpenCLIntersectionDevice *dev,
		unsigned int stackSize, bool disableImageStorage) const;
	virtual void Init(const std::deque<Mesh *> &meshes,
		const unsigned int totalVertexCount,
		const unsigned int totalTriangleCount);

	virtual const TriangleMeshID GetMeshID(const unsigned int index) const {
		return preprocessedMeshIDs[index];
	}
	virtual const TriangleMeshID *GetMeshIDTable() const {
		return preprocessedMeshIDs;
	}
	virtual const TriangleID GetMeshTriangleID(const unsigned int index) const {
		return preprocessedMeshTriangleIDs[index];
	}
	virtual const TriangleID *GetMeshTriangleIDTable() const {
		return preprocessedMeshTriangleIDs;
	}

	virtual bool Intersect(const Ray *ray, RayHit *hit) const;

	const TriangleMesh *GetPreprocessedMesh() const {
		return preprocessedMesh;
	}

private:
	// BVHAccel Private Methods
	BVHAccelTreeNode *BuildHierarchy(std::vector<BVHAccelTreeNode *> &list,
		unsigned int begin, unsigned int end, unsigned int axis);
	void FindBestSplit(std::vector<BVHAccelTreeNode *> &list,
		unsigned int begin, unsigned int end, float *splitValue,
		unsigned int *bestAxis);
	unsigned int BuildArray(BVHAccelTreeNode *node, unsigned int offset);
	void FreeHierarchy(BVHAccelTreeNode *node);

	unsigned int treeType;
	int costSamples, isectCost, traversalCost;
	float emptyBonus;
	unsigned int nNodes;
	BVHAccelArrayNode *bvhTree;

	const Context *ctx;
	TriangleMesh *preprocessedMesh;
	TriangleMeshID *preprocessedMeshIDs;
	TriangleID *preprocessedMeshTriangleIDs;

	bool initialized;
};

}

#endif	/* _LUXRAYS_BVHACCEL_H */
