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

#ifndef _LUXRAYS_ACCELERATOR_H
#define	_LUXRAYS_ACCELERATOR_H

#include "luxrays/luxrays.h"
#include "luxrays/core/trianglemesh.h"

namespace luxrays {

typedef enum {
	ACCEL_BVH, ACCEL_QBVH, ACCEL_MQBVH, ACCEL_MBVH
} AcceleratorType;

class OpenCLKernels;
class OpenCLIntersectionDevice;

class Accelerator {
public:
	Accelerator() { }
	virtual ~Accelerator() { }

	virtual AcceleratorType GetType() const = 0;

	virtual OpenCLKernels *NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize, const bool disableImageStorage) const = 0;

	virtual void Init(const std::deque<const Mesh *> &meshes, const u_int totalVertexCount, const u_int totalTriangleCount) = 0;
	virtual const TriangleMeshID GetMeshID(const u_int index) const = 0;
	virtual const TriangleMeshID *GetMeshIDTable() const = 0;
	virtual const TriangleID GetMeshTriangleID(const u_int index) const = 0;
	virtual const TriangleID *GetMeshTriangleIDTable() const = 0;

	virtual bool Intersect(const Ray *ray, RayHit *hit) const = 0;
};

}

#endif	/* _LUXRAYS_ACCELERATOR_H */
