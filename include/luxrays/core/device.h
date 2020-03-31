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

#ifndef _LUXRAYS_DEVICE_H
#define	_LUXRAYS_DEVICE_H

#include <string>
#include <cstdlib>

#include <boost/thread/thread.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/dataset.h"
#include "luxrays/core/context.h"
#include "luxrays/utils/utils.h"

namespace luxrays {

//------------------------------------------------------------------------------
// DeviceDescription
//------------------------------------------------------------------------------

typedef enum {
	DEVICE_TYPE_NATIVE = 1 << 0,
	DEVICE_TYPE_OPENCL_DEFAULT = 1 << 1,
	DEVICE_TYPE_OPENCL_CPU = 1 << 2,
	DEVICE_TYPE_OPENCL_GPU = 1 << 3,
	DEVICE_TYPE_OPENCL_UNKNOWN = 1 << 4,
	DEVICE_TYPE_CUDA_GPU = 1 << 5,
	DEVICE_TYPE_OPENCL_ALL = DEVICE_TYPE_OPENCL_DEFAULT |
		DEVICE_TYPE_OPENCL_CPU | DEVICE_TYPE_OPENCL_GPU |
		DEVICE_TYPE_OPENCL_UNKNOWN,
	DEVICE_TYPE_CUDA_ALL = DEVICE_TYPE_CUDA_GPU,
	DEVICE_TYPE_ALL = DEVICE_TYPE_NATIVE | DEVICE_TYPE_OPENCL_ALL,
	DEVICE_TYPE_ALL_HARDWARE = DEVICE_TYPE_OPENCL_ALL | DEVICE_TYPE_CUDA_ALL,
	DEVICE_TYPE_ALL_INTERSECTION = DEVICE_TYPE_NATIVE | DEVICE_TYPE_OPENCL_ALL,
} DeviceType;

class DeviceDescription {
public:
	DeviceDescription(const std::string deviceName,
		const DeviceType deviceType) :
		name(deviceName), type(deviceType) { }
	virtual ~DeviceDescription() { }

	const std::string &GetName() const { return name; }
	const DeviceType GetType() const { return type; };
	virtual int GetComputeUnits() const { return 1; }
	virtual u_int GetNativeVectorWidthFloat() const { return 4; };
	virtual size_t GetMaxMemory() const { return 0; }
	virtual size_t GetMaxMemoryAllocSize() const { return 0; }

	static void FilterOne(std::vector<DeviceDescription *> &deviceDescriptions);
	static void Filter(DeviceType type, std::vector<DeviceDescription *> &deviceDescriptions);
	static std::string GetDeviceType(const DeviceType type);

protected:
	std::string name;
	DeviceType type;
};

//------------------------------------------------------------------------------
// Device
//------------------------------------------------------------------------------

/*
 * The inheritance scheme used here:
 *
 *  Device => | =>            IntersectionDevice             => | => NativeIntersectionDevice
 *  
 *            | =>   HardwareDevice   => | =>  OpenCLDevice  => |
 *  Device => |                                                 | => OpenCLIntersectionDevice
 *            | =>            IntersectionDevice             => |
 *
 *            | =>   HardwareDevice   => | =>   CudaDevice   => |
 *  Device => |                                                 | => CudaIntersectionDevice
 *            | =>            IntersectionDevice             => |
 */

class Device {
public:
	const std::string &GetName() const { return deviceName; }
	const Context *GetContext() const { return deviceContext; }
	const DeviceType GetType() const { return deviceType; }

	virtual bool IsRunning() const { return started; };

	friend class Context;
	friend class OpenCLIntersectionDevice;

protected:
	Device() { }
	Device(const Context *context, const DeviceType type, const size_t index);
	virtual ~Device();

	virtual void Start();
	virtual void Interrupt();
	virtual void Stop();

	const Context *deviceContext;
	DeviceType deviceType;
	size_t deviceIndex;

	std::string deviceName;

	bool started;
};

}

#endif	/* _LUXRAYS_DEVICE_H */
