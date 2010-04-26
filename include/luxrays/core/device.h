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

#ifndef _LUXRAYS_DEVICE_H
#define	_LUXRAYS_DEVICE_H

#include <string>
#include <cstdlib>

#include <boost/thread/thread.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/utils.h"
#include "luxrays/core/dataset.h"
#include "luxrays/core/context.h"

namespace luxrays {

typedef enum {
	DEVICE_TYPE_ALL, DEVICE_TYPE_OPENCL, DEVICE_TYPE_NATIVE_THREAD, DEVICE_TYPE_VIRTUAL
} DeviceType;

class DeviceDescription {
public:
	DeviceDescription() { }
	DeviceDescription(const std::string deviceName, const DeviceType deviceType) :
		name(deviceName), type(deviceType) { }

	const std::string &GetName() const { return name; }
	const DeviceType GetType() const { return type; };

	static void FilterOne(std::vector<DeviceDescription *> &deviceDescriptions);
	static void Filter(DeviceType type, std::vector<DeviceDescription *> &deviceDescriptions);
	static std::string GetDeviceType(const DeviceType type);

protected:
	std::string name;
	DeviceType type;
};

class Device {
public:
	const std::string &GetName() const { return deviceName; }
	const Context *GetContext() const { return deviceContext; }
	const DeviceType GetType() const { return deviceType; }

	virtual bool IsRunning() const { return started; };

	friend class Context;

protected:
	Device(const Context *context, const DeviceType type, const unsigned int index);
	virtual ~Device();

	virtual void Start();
	virtual void Interrupt() = 0;
	virtual void Stop();

	const Context *deviceContext;
	DeviceType deviceType;
	unsigned int deviceIndex;

	std::string deviceName;

	bool started;
};

//------------------------------------------------------------------------------
// Native thread devices
//------------------------------------------------------------------------------

class NativeThreadDeviceDescription : public DeviceDescription {
public:
	NativeThreadDeviceDescription(const std::string deviceName, const unsigned int threadIdx) :
		DeviceDescription(deviceName, DEVICE_TYPE_NATIVE_THREAD), threadIndex(threadIdx) { }

	size_t GetThreadIndex() const { return threadIndex; };

	friend class Context;

protected:
	static void AddDeviceDescs(std::vector<DeviceDescription *> &descriptions);

	size_t threadIndex;
};

//------------------------------------------------------------------------------
// OpenCL devices
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)

class OpenCLIntersectionDevice;

typedef enum {
	OCL_DEVICE_TYPE_ALL, OCL_DEVICE_TYPE_DEFAULT, OCL_DEVICE_TYPE_CPU,
			OCL_DEVICE_TYPE_GPU, OCL_DEVICE_TYPE_UNKNOWN
} OpenCLDeviceType;

class OpenCLDeviceDescription : public DeviceDescription {
public:
	OpenCLDeviceDescription(cl::Device &device, const size_t devIndex) :
		DeviceDescription(device.getInfo<CL_DEVICE_NAME>().c_str(), DEVICE_TYPE_OPENCL),
		oclType(GetOCLDeviceType(device.getInfo<CL_DEVICE_TYPE>())),
		deviceIndex(devIndex),
		computeUnits(device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>()),
		maxMemory(device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>()),
		usedMemory(0),
		forceWorkGroupSize(0),
		oclDevice(device),
		oclContext(NULL) { }

	~OpenCLDeviceDescription() {
		delete oclContext;
	}

	OpenCLDeviceType GetOpenCLType() const { return oclType; }
	size_t GetDeviceIndex() const { return deviceIndex; }
	int GetComputeUnits() const { return computeUnits; }
	size_t GetMaxMemory() const { return maxMemory; }
	size_t GetUsedMemory() const { return usedMemory; }
	unsigned int GetForceWorkGroupSize() const { return forceWorkGroupSize; }

	void SetForceWorkGroupSize(const unsigned int size) const { forceWorkGroupSize = size; }

	static void Filter(const OpenCLDeviceType type, std::vector<DeviceDescription *> &deviceDescriptions);

	friend class Context;
	friend class OpenCLIntersectionDevice;
	friend class OpenCLPixelDevice;

protected:
	static std::string GetDeviceType(const cl_int type);
	static std::string GetDeviceType(const OpenCLDeviceType type);
	static OpenCLDeviceType GetOCLDeviceType(const cl_int type);
	static void AddDeviceDescs(const cl::Platform &oclPlatform, const OpenCLDeviceType filter,
		std::vector<DeviceDescription *> &descriptions);

	cl::Context &GetOCLContext() {
		if (!oclContext) {
			// Allocate a context with the selected device
			VECTOR_CLASS<cl::Device> devices;
			devices.push_back(oclDevice);
			cl::Platform platform = oclDevice.getInfo<CL_DEVICE_PLATFORM>();
			cl_context_properties cps[3] = {
				CL_CONTEXT_PLATFORM, (cl_context_properties)platform(), 0
			};

			oclContext = new cl::Context(devices, cps);
		}

		return *oclContext;
	}

	cl::Device &GetOCLDevice() {
		return oclDevice;
	}

	OpenCLDeviceType oclType;
	size_t deviceIndex;
	int computeUnits;
	size_t maxMemory, usedMemory;

	// The use of this field is not multi-thread safe (i.e. OpenCLDeviceDescription
	// is shared among all threads)
	mutable unsigned int forceWorkGroupSize;

private:
	cl::Device oclDevice;
	mutable cl::Context *oclContext;
};

#endif

}

#endif	/* _LUXRAYS_DEVICE_H */
