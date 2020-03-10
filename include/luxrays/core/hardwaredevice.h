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

#ifndef _LUXRAYS_HARDWAREDEVICE_H
#define	_LUXRAYS_HARDWAREDEVICE_H

#include "luxrays/core/device.h"
#include "luxrays/utils/ocl.h"

namespace luxrays {

//------------------------------------------------------------------------------
// HardwareDeviceBuffer
//------------------------------------------------------------------------------

class HardwareDeviceBuffer {
public:
	virtual ~HardwareDeviceBuffer() { }

	virtual bool IsNull() const = 0;

protected:
	HardwareDeviceBuffer() { }
};

//------------------------------------------------------------------------------
// HardwareDevice
//------------------------------------------------------------------------------

class HardwareDevice : virtual public Device {
public:
	//--------------------------------------------------------------------------
	// Kernels handling of hardware (aka GPU) only applications
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// Memory management for hardware (aka GPU) only applications
	//--------------------------------------------------------------------------

	virtual size_t GetMaxMemory() const { return 0; }
	size_t GetUsedMemory() const { return usedMemory; }

	virtual void AllocBufferRO(HardwareDeviceBuffer **buff, void *src, const size_t size, const std::string &desc = "") = 0;
	virtual void AllocBufferRW(HardwareDeviceBuffer **buff, void *src, const size_t size, const std::string &desc = "") = 0;
	virtual void FreeBuffer(HardwareDeviceBuffer **buff) = 0;

protected:
	HardwareDevice();
	virtual ~HardwareDevice();

	void AllocMemory(const size_t s) { usedMemory += s; }
	void FreeMemory(const size_t s) { usedMemory -= s; }

	size_t usedMemory;
};

}

#endif	/* _LUXRAYS_INTERSECTIONDEVICE_H */
