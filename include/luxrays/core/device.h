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
	DEVICE_TYPE_NATIVE_THREAD = 1 << 0,
	DEVICE_TYPE_OPENCL_DEFAULT = 1 << 1,
	DEVICE_TYPE_OPENCL_CPU = 1 << 2,
	DEVICE_TYPE_OPENCL_GPU = 1 << 3,
	DEVICE_TYPE_OPENCL_UNKNOWN = 1 << 4,
	DEVICE_TYPE_VIRTUAL = 1 << 5,
	DEVICE_TYPE_OPENCL_ALL = DEVICE_TYPE_OPENCL_DEFAULT |
		DEVICE_TYPE_OPENCL_CPU | DEVICE_TYPE_OPENCL_GPU |
		DEVICE_TYPE_OPENCL_UNKNOWN,
	DEVICE_TYPE_ALL = DEVICE_TYPE_NATIVE_THREAD | DEVICE_TYPE_OPENCL_ALL |
		DEVICE_TYPE_VIRTUAL
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
	virtual size_t GetMaxMemory() const { return 0; }
	virtual size_t GetMaxMemoryAllocSize() const { return 0; }

	unsigned int GetForceWorkGroupSize() const { return forceWorkGroupSize; }
	void SetForceWorkGroupSize(const unsigned int size) const { forceWorkGroupSize = size; }

	static void FilterOne(std::vector<DeviceDescription *> &deviceDescriptions);
	static void Filter(DeviceType type, std::vector<DeviceDescription *> &deviceDescriptions);
	static std::string GetDeviceType(const DeviceType type);

protected:
	std::string name;
	DeviceType type;

	mutable unsigned int forceWorkGroupSize;
};

class Device {
public:
	const std::string &GetName() const { return deviceName; }
	const Context *GetContext() const { return deviceContext; }
	const DeviceType GetType() const { return deviceType; }

	virtual bool IsRunning() const { return started; };

	virtual size_t GetMaxMemory() const { return 0; }
	size_t GetUsedMemory() const { return usedMemory; }
	void AllocMemory(size_t s) const { usedMemory += s; }
	void FreeMemory(size_t s) const { usedMemory -= s; }

	friend class Context;
	friend class VirtualM2OHardwareIntersectionDevice;
	friend class VirtualM2MHardwareIntersectionDevice;

protected:
	Device(const Context *context, const DeviceType type, const size_t index);
	virtual ~Device();

	virtual void Start();
	virtual void Interrupt() = 0;
	virtual void Stop();

	const Context *deviceContext;
	DeviceType deviceType;
	size_t deviceIndex;

	std::string deviceName;

	bool started;

	mutable size_t usedMemory;
};

//------------------------------------------------------------------------------
// Native thread devices
//------------------------------------------------------------------------------

class NativeThreadDeviceDescription : public DeviceDescription {
public:
	NativeThreadDeviceDescription(const std::string deviceName, const size_t threadIdx) :
		DeviceDescription(deviceName, DEVICE_TYPE_NATIVE_THREAD), threadIndex(threadIdx) { }

	size_t GetThreadIndex() const { return threadIndex; };

	friend class Context;

protected:
	static void AddDeviceDescs(std::vector<DeviceDescription *> &descriptions);

	size_t threadIndex;
};

}

#endif	/* _LUXRAYS_DEVICE_H */
