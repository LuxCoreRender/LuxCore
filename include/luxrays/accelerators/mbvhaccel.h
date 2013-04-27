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

#ifndef _LUXRAYS_MBVHACCEL_H
#define	_LUXRAYS_MBVHACCEL_H

#include <vector>

#include "luxrays/luxrays.h"
#include "luxrays/accelerators/bvhaccel.h"

namespace luxrays {

// MBVHAccel Declarations
class MBVHAccel : public Accelerator {
public:
	// MBVHAccel Public Methods
	MBVHAccel(const Context *context,
			const unsigned int treetype, const int csamples, const int icost,
			const int tcost, const float ebonus);
	virtual ~MBVHAccel();

	virtual AcceleratorType GetType() const { return ACCEL_MBVH; }
	virtual OpenCLKernels *NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool disableImageStorage) const;
	virtual void Init(const std::deque<const Mesh *> &meshes,
		const unsigned int totalVertexCount,
		const unsigned int totalTriangleCount);

	virtual const TriangleMeshID GetMeshID(const unsigned int index) const {
		return meshIDs[index];
	}
	virtual const TriangleMeshID *GetMeshIDTable() const {
		return meshIDs;
	}
	virtual const TriangleID GetMeshTriangleID(const unsigned int index) const {
		return meshTriangleIDs[index];
	}
	virtual const TriangleID *GetMeshTriangleIDTable() const {
		return meshTriangleIDs;
	}

	virtual bool Intersect(const Ray *ray, RayHit *hit) const;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	friend class OpenCLMBVHKernels;
#endif

private:
	static bool MeshPtrCompare(const Mesh *, const Mesh *);

	BVHAccel::BVHParams params;

	// The root BVH tree
	unsigned int nRootNodes;
	BVHAccelArrayNode *bvhRootTree;

	std::vector<BVHAccel *> uniqueLeafs;
	std::vector<Transform> uniqueLeafsTransform;
	
	const Context *ctx;
	TriangleMeshID *meshIDs;
	TriangleID *meshTriangleIDs;

	bool initialized;
};

}

#endif	/* _LUXRAYS_MBVHACCEL_H */
