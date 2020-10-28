/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_CUDA)

#ifndef _LUXRAYS_OPTIXACCEL_H
#define	_LUXRAYS_OPTIXACCEL_H

#include "luxrays/luxrays.h"
#include "luxrays/core/accelerator.h"

namespace luxrays {

class OptixAccel : public Accelerator {
public:
	OptixAccel(const Context *context);
	virtual ~OptixAccel();

	virtual AcceleratorType GetType() const { return ACCEL_OPTIX; }

	virtual bool HasNativeSupport(const IntersectionDevice &device) const;
	virtual bool HasHWSupport(const IntersectionDevice &device) const;

	virtual HardwareIntersectionKernel *NewHardwareIntersectionKernel(HardwareIntersectionDevice &device) const;

	virtual void Init(const std::deque<const Mesh *> &meshes,
		const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount);

	virtual bool Intersect(const Ray *ray, RayHit *hit) const;

	friend class OptixKernel;

private:
	const Context *ctx;
	std::deque<const Mesh *> meshes;
	u_longlong totalVertexCount, totalTriangleCount;

	bool initialized;
};

}

#endif

#endif
