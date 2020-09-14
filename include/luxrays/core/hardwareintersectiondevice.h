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

#ifndef _LUXRAYS_HARDWAREINTERSECTIONDEVICE_H
#define	_LUXRAYS_HARDWAREINTERSECTIONDEVICE_H

#include "luxrays/luxrays.h"
#include "luxrays/core/hardwaredevice.h"
#include "luxrays/core/intersectiondevice.h"

namespace luxrays {

//------------------------------------------------------------------------------
// HardwareIntersectionDevice
//------------------------------------------------------------------------------

class HardwareIntersectionDevice : public IntersectionDevice, virtual public HardwareDevice {
public:
	// Returns true if it support HardwareDevice ray tracing
	virtual bool HasHWSupport() const { return true; }

	//--------------------------------------------------------------------------
	// Data parallel interface: to trace a multiple rays (i.e. on the GPU)
	//--------------------------------------------------------------------------

	virtual void EnqueueTraceRayBuffer(HardwareDeviceBuffer *rayBuff,
			HardwareDeviceBuffer *rayHitBuff, const unsigned int rayCount) {
		throw std::runtime_error("Called EnqueueTraceRayBuffer() on a device without parallel support");
	}

	friend class Context;

protected:
	virtual void Update() = 0;

	HardwareIntersectionDevice();
	virtual ~HardwareIntersectionDevice();
};

//------------------------------------------------------------------------------
// HardwareIntersectionKernel
//------------------------------------------------------------------------------

class HardwareIntersectionKernel {
public:
	HardwareIntersectionKernel(HardwareIntersectionDevice  &dev) : device(dev) {
	}
	virtual ~HardwareIntersectionKernel() {
	}

	virtual void Update(const DataSet *newDataSet) = 0;
	virtual void EnqueueTraceRayBuffer(HardwareDeviceBuffer *rayBuff,
			HardwareDeviceBuffer *rayHitBuff, const unsigned int rayCount) = 0;

protected:
	HardwareIntersectionDevice &device;
};

}

#endif	/* _LUXRAYS_HARDWAREINTERSECTIONDEVICE_H */
