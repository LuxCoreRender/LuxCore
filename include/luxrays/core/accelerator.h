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

#ifndef _LUXRAYS_ACCELERATOR_H
#define	_LUXRAYS_ACCELERATOR_H

#include <string>
#include <deque>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/ray.h"
#include "luxrays/core/trianglemesh.h"

namespace luxrays {

typedef enum {
	ACCEL_AUTO, ACCEL_BVH, ACCEL_QBVH, ACCEL_MQBVH, ACCEL_MBVH, ACCEL_EMBREE
} AcceleratorType;

class OpenCLKernels;
class OpenCLIntersectionDevice;

class Accelerator {
public:
	Accelerator() { }
	virtual ~Accelerator() { }

	virtual AcceleratorType GetType() const = 0;

	virtual OpenCLKernels *NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize) const = 0;
	virtual bool CanRunOnOpenCLDevice(OpenCLIntersectionDevice *device) const { return true; }

	virtual void Init(const std::deque<const Mesh *> &meshes, const u_longlong totalVertexCount, const u_longlong totalTriangleCount) = 0;
	virtual bool DoesSupportUpdate() const { return false; }
	virtual void Update() { throw new std::runtime_error("Internal error in Accelerator::Update()"); }

	virtual bool Intersect(const Ray *ray, RayHit *hit) const = 0;

	static std::string AcceleratorType2String(const AcceleratorType type);
	static AcceleratorType String2AcceleratorType(const std::string &type);
};

}

#endif	/* _LUXRAYS_ACCELERATOR_H */
