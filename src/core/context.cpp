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

#include <cstdlib>
#include <cassert>
#include <iosfwd>
#include <sstream>
#include <stdexcept>

#include "luxrays/core/context.h"
#include "luxrays/core/device.h"

using namespace luxrays;

Context::Context(LuxRaysDebugHandler handler, const int openclPlatformIndex) {
	debugHandler = handler;
	currentDataSet = NULL;
	started = false;

	// Platform info
	VECTOR_CLASS<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	for (size_t i = 0; i < platforms.size(); ++i)
		LR_LOG(this, "OpenCL Platform " << i << ": " << platforms[i].getInfo<CL_PLATFORM_VENDOR>().c_str());

	if (openclPlatformIndex < 0) {
		if (platforms.size() > 0) {
			// Just use the first platform available
			oclPlatform = platforms[0];
		} else
			LR_LOG(this, "No OpenCL platform available");
	} else {
		if ((platforms.size() == 0) || (openclPlatformIndex >= (int)platforms.size()))
			throw std::runtime_error("Unable to find an appropiate OpenCL platform");
		else
			oclPlatform = platforms[openclPlatformIndex];
	}

	// Get the list of devices available on the platform
	NativeThreadIntersectionDevice::AddDevices(deviceDescriptions);
	OpenCLIntersectionDevice::AddDevices(oclPlatform, OCL_DEVICE_TYPE_ALL, deviceDescriptions);

	// Print device info
	for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
		if (deviceDescriptions[i]->GetType() == DEVICE_TYPE_NATIVE_THREAD) {
			LR_LOG(this, "Device " << i << " NativeThread name: " <<
					deviceDescriptions[i]->GetName());
		} else if (deviceDescriptions[i]->GetType() == DEVICE_TYPE_OPENCL) {
			OpenCLDeviceDescription *desc = (OpenCLDeviceDescription *)deviceDescriptions[i];
			LR_LOG(this, "Device " << i << " OpenCL name: " <<
					desc->GetName());

			LR_LOG(this, "Device " << i << " OpenCL type: " <<
					OpenCLIntersectionDevice::GetDeviceType(desc->GetOpenCLType()));
			LR_LOG(this, "Device " << i << " OpenCL units: " <<
					desc->GetComputeUnits());
			LR_LOG(this, "Device " << i << " OpenCL max allocable memory: " <<
					desc->GetMaxMemory() / (1024 * 1024) << "MBytes");
		} else
			assert (false);
	}
}

Context::~Context() {
	if (started)
		Stop();

	for (size_t i = 0; i < deviceDescriptions.size(); ++i)
		delete deviceDescriptions[i];
	for (size_t i = 0; i < devices.size(); ++i)
		delete devices[i];

	if (currentDataSet)
		delete currentDataSet;
}

void Context::SetCurrentDataSet(const DataSet *dataSet) {
	assert (!started);
	assert (dataSet != NULL);
	assert (dataSet->IsPreprocessed());

	if (currentDataSet)
		delete currentDataSet;

	currentDataSet = dataSet;

	for (size_t i = 0; i < devices.size(); ++i)
		devices[i]->SetDataSet(currentDataSet);
}

void Context::Start() {
	assert (!started);

	started = true;
}

void Context::Stop() {
	assert (started);

	for (size_t i = 0; i < devices.size(); ++i) {
		if (devices[i]->IsRunning())
			devices[i]->Stop();
	}

	started = false;
}

const std::vector<DeviceDescription *> &Context::GetAvailableDeviceDescriptions() const {
	return deviceDescriptions;
}

const std::vector<IntersectionDevice *> &Context::GetIntersectionDevices() const {
	return devices;
}

std::vector<IntersectionDevice *> Context::AddIntersectionDevices(const std::vector<DeviceDescription *> &deviceDesc) {
	assert (!started);

	LR_LOG(this, "Allocating " << deviceDesc.size() << " intersection device(s)");

	// Get the list of devices available on the platform
	VECTOR_CLASS<cl::Device> oclDevices;
	oclPlatform.getDevices(CL_DEVICE_TYPE_ALL, &oclDevices);

	std::vector<IntersectionDevice *> newDevices;
	for (size_t i = 0; i < deviceDesc.size(); ++i) {
		LR_LOG(this, "Allocating intersection device " << i << ": " << deviceDesc[i]->GetName() <<
				" (Type = " << DeviceDescription::GetDeviceType(deviceDesc[i]->GetType()) << ")");

		IntersectionDevice *device;
		if (deviceDesc[i]->GetType() == DEVICE_TYPE_NATIVE_THREAD) {
			// Nathive thread devices
			const NativeThreadDeviceDescription *ntvDeviceDesc = (const NativeThreadDeviceDescription *)deviceDesc[i];
			device = new NativeThreadIntersectionDevice(this, ntvDeviceDesc->GetThreadIndex(), i);
		} else if (deviceDesc[i]->GetType() == DEVICE_TYPE_OPENCL) {
			// OpenCL devices
			const OpenCLDeviceDescription *oclDeviceDesc = (const OpenCLDeviceDescription *)deviceDesc[i];
			device = new OpenCLIntersectionDevice(this, oclDevices[oclDeviceDesc->GetDeviceIndex()], i);
		} else
			assert (false);

		devices.push_back(device);
		newDevices.push_back(device);
	}

	return newDevices;
}
