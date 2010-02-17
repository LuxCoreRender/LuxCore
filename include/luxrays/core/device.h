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

#ifndef _LUXRAYS_INTERSECTIONDEVICE_H
#define	_LUXRAYS_INTERSECTIONDEVICE_H

#include <string>
#include <cstdlib>

#define __CL_ENABLE_EXCEPTIONS
#define __NO_STD_VECTOR
#define __NO_STD_STRING

#if defined(__APPLE__)
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

#include <boost/thread/thread.hpp>

#include "luxrays/luxrays.h"
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

class IntersectionDevice {
public:
	IntersectionDevice(const Context *context, const DeviceType type, const unsigned int index);
	virtual ~IntersectionDevice();

	const std::string &GetName() const { return deviceName; }
	const Context *GetContext() const { return deviceContext; }
	const DeviceType GetType() const { return deviceType; }

	virtual void SetDataSet(const DataSet *newDataSet);
	virtual void Start();
	virtual void Interrupt() = 0;
	virtual void Stop();
	virtual bool IsRunning() const { return started; };

	virtual RayBuffer *NewRayBuffer() = 0;
	virtual void PushRayBuffer(RayBuffer *rayBuffer) = 0;
	virtual RayBuffer *PopRayBuffer() = 0;
	virtual size_t GetQueueSize() = 0;

protected:
	const Context *deviceContext;
	DeviceType deviceType;
	unsigned int deviceIndex;

	std::string deviceName;
	const DataSet *dataSet;

	double statsTotalRayTime, statsTotalRayCount, statsDeviceIdleTime, statsDeviceTotalTime;

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

protected:
	size_t threadIndex;
};

class NativeThreadIntersectionDevice : public IntersectionDevice {
public:
	NativeThreadIntersectionDevice(const Context *context, const size_t threadIndex,
			const unsigned int devIndex);
	~NativeThreadIntersectionDevice();

	void SetDataSet(const DataSet *newDataSet);
	void Start();
	void Interrupt();
	void Stop();

	RayBuffer *NewRayBuffer();
	size_t GetQueueSize() { return todoRayBufferQueue.GetSize(); }
	void PushRayBuffer(RayBuffer *rayBuffer);
	RayBuffer *PopRayBuffer();

	static void IntersectionThread(NativeThreadIntersectionDevice *renderDevice);
	static size_t RayBufferSize;

	friend class Context;

protected:
	static void AddDevices(std::vector<DeviceDescription *> &descriptions);

private:
	RayBufferQueue todoRayBufferQueue;
	RayBufferQueue doneRayBufferQueue;

	boost::thread *intersectionThread;
};

//------------------------------------------------------------------------------
// OpenCL devices
//------------------------------------------------------------------------------

typedef enum {
	OCL_DEVICE_TYPE_ALL, OCL_DEVICE_TYPE_DEFAULT, OCL_DEVICE_TYPE_CPU,
			OCL_DEVICE_TYPE_GPU, OCL_DEVICE_TYPE_UNKNOWN
} OpenCLDeviceType;

class OpenCLDeviceDescription : public DeviceDescription {
public:
	OpenCLDeviceDescription(const std::string deviceName, const OpenCLDeviceType type,
		const size_t devIndex, const int deviceComputeUnits, const size_t deviceMaxMemory) :
		DeviceDescription(deviceName, DEVICE_TYPE_OPENCL), oclType(type), deviceIndex(devIndex),
		computeUnits(deviceComputeUnits), maxMemory(deviceMaxMemory) { }

	OpenCLDeviceType GetOpenCLType() const { return oclType; }
	size_t GetDeviceIndex() const { return deviceIndex; }
	int GetComputeUnits() const { return computeUnits; }
	size_t GetMaxMemory() const { return maxMemory; }

protected:
	OpenCLDeviceType oclType;
	size_t deviceIndex;
	int computeUnits;
	size_t maxMemory;
};

class OpenCLIntersectionDevice : public IntersectionDevice {
public:
	OpenCLIntersectionDevice(const Context *context, const cl::Device &device, const unsigned int index);
	~OpenCLIntersectionDevice();

	void SetDataSet(const DataSet *newDataSet);
	void Start();
	void Interrupt();
	void Stop();

	RayBuffer *NewRayBuffer();
	size_t GetQueueSize() { return todoRayBufferQueue.GetSize(); }
	void PushRayBuffer(RayBuffer *rayBuffer);
	RayBuffer *PopRayBuffer();

	OpenCLDeviceType GetOpenCLType() const { return oclType; }

	friend class Context;

	static void Filter(const OpenCLDeviceType type, std::vector<DeviceDescription *> &deviceDescriptions);
	static void IntersectionThread(OpenCLIntersectionDevice *renderDevice);
	static size_t RayBufferSize;

protected:
	static std::string GetDeviceType(const cl_int type);
	static std::string GetDeviceType(const OpenCLDeviceType type);
	static OpenCLDeviceType GetOCLDeviceType(const cl_int type);
	static void AddDevices(const cl::Platform &oclPlatform, const OpenCLDeviceType filter,
		std::vector<DeviceDescription *> &descriptions);

private:
	void TraceRayBuffer(RayBuffer *rayBuffer);

	OpenCLDeviceType oclType;
	boost::thread *intersectionThread;

	// OpenCL items
	cl::Context *oclContext;
	cl::CommandQueue *oclQueue;
	cl::Kernel *bvhKernel;
	size_t bvhWorkGroupSize;

	cl::Buffer *raysBuff;
	cl::Buffer *hitsBuff;
	cl::Buffer *vertsBuff;
	cl::Buffer *trisBuff;
	cl::Buffer *bvhBuff;

	RayBufferQueue todoRayBufferQueue;
	RayBufferQueue doneRayBufferQueue;

	bool reportedPermissionError;
};

}

#endif	/* _LUXRAYS_INTERSECTIONDEVICE_H */
