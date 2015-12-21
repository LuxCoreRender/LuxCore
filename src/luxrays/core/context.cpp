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

#include <cstdlib>
#include <cassert>
#include <iosfwd>
#include <sstream>
#include <stdexcept>

#include "luxrays/core/context.h"
#ifdef LUXRAYS_DISABLE_OPENCL
#include "luxrays/core/intersectiondevice.h"
#else
#include "luxrays/core/oclintersectiondevice.h"
#endif
#include "luxrays/core/virtualdevice.h"

using namespace luxrays;

Context::Context(LuxRaysDebugHandler handler, const int openclPlatformIndex) {
	debugHandler = handler;
	currentDataSet = NULL;
	started = false;

	// Get the list of devices available on the platform
	NativeThreadDeviceDescription::AddDeviceDescs(deviceDescriptions);

#if !defined(LUXRAYS_DISABLE_OPENCL)
	// Platform info
	VECTOR_CLASS<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	for (size_t i = 0; i < platforms.size(); ++i)
		LR_LOG(this, "OpenCL Platform " << i << ": " << platforms[i].getInfo<CL_PLATFORM_VENDOR>().c_str());

	if (openclPlatformIndex < 0) {
		if (platforms.size() > 0) {
			// Just use all the platforms available
			for (size_t i = 0; i < platforms.size(); ++i)
				OpenCLDeviceDescription::AddDeviceDescs(
					platforms[i], DEVICE_TYPE_OPENCL_ALL,
					deviceDescriptions);
		} else
			LR_LOG(this, "No OpenCL platform available");
	} else {
		if ((platforms.size() == 0) || (openclPlatformIndex >= (int)platforms.size()))
			throw std::runtime_error("Unable to find an appropriate OpenCL platform");
		else {
			OpenCLDeviceDescription::AddDeviceDescs(
				platforms[openclPlatformIndex],
				DEVICE_TYPE_OPENCL_ALL, deviceDescriptions);
		}
	}
#endif

	// Print device info
	for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
		DeviceDescription *desc = deviceDescriptions[i];
		LR_LOG(this, "Device " << i << " name: " <<
			desc->GetName());

		LR_LOG(this, "Device " << i << " type: " <<
			DeviceDescription::GetDeviceType(desc->GetType()));

		LR_LOG(this, "Device " << i << " compute units: " <<
			desc->GetComputeUnits());

		LR_LOG(this, "Device " << i << " preferred float vector width: " <<
			desc->GetNativeVectorWidthFloat());

		LR_LOG(this, "Device " << i << " max allocable memory: " <<
			desc->GetMaxMemory() / (1024 * 1024) << "MBytes");

		LR_LOG(this, "Device " << i << " max allocable memory block size: " <<
			desc->GetMaxMemoryAllocSize() / (1024 * 1024) << "MBytes");
	}
}

Context::~Context() {
	if (started)
		Stop();

	for (size_t i = 0; i < idevices.size(); ++i)
		delete idevices[i];
	for (size_t i = 0; i < deviceDescriptions.size(); ++i)
		delete deviceDescriptions[i];
}

void Context::SetDataSet(DataSet *dataSet) {
	assert (!started);

	currentDataSet = dataSet;

	for (size_t i = 0; i < idevices.size(); ++i)
		idevices[i]->SetDataSet(currentDataSet);
}

void Context::UpdateDataSet() {
	assert (started);

	// Update the data set
	currentDataSet->Update();

#if !defined(LUXRAYS_DISABLE_OPENCL)
	// Update all OpenCL devices
	for (u_int i = 0; i < idevices.size(); ++i) {
		OpenCLIntersectionDevice *oclDevice = dynamic_cast<OpenCLIntersectionDevice *>(idevices[i]);
		if (oclDevice)
			oclDevice->Update();
	}
#endif
}

void Context::Start() {
	assert (!started);

	for (size_t i = 0; i < idevices.size(); ++i)
		idevices[i]->Start();

	started = true;
}

void Context::Interrupt() {
	assert (started);

	for (size_t i = 0; i < idevices.size(); ++i)
		idevices[i]->Interrupt();
}

void Context::Stop() {
	assert (started);

	Interrupt();

	for (size_t i = 0; i < idevices.size(); ++i)
		idevices[i]->Stop();

	started = false;
}

const std::vector<DeviceDescription *> &Context::GetAvailableDeviceDescriptions() const {
	return deviceDescriptions;
}

const std::vector<IntersectionDevice *> &Context::GetIntersectionDevices() const {
	return idevices;
}

std::vector<IntersectionDevice *> Context::CreateIntersectionDevices(
	std::vector<DeviceDescription *> &deviceDesc, const size_t indexOffset) {
	assert (!started);

	LR_LOG(this, "Creating " << deviceDesc.size() << " intersection device(s)");

	std::vector<IntersectionDevice *> newDevices;
	for (size_t i = 0; i < deviceDesc.size(); ++i) {
		LR_LOG(this, "Allocating intersection device " << i << ": " << deviceDesc[i]->GetName() <<
				" (Type = " << DeviceDescription::GetDeviceType(deviceDesc[i]->GetType()) << ")");

		IntersectionDevice *device;
		if (deviceDesc[i]->GetType() == DEVICE_TYPE_NATIVE_THREAD) {
			// Nathive thread devices
			device = new NativeThreadIntersectionDevice(this, indexOffset + i);
		}
#if !defined(LUXRAYS_DISABLE_OPENCL)
		else if (deviceDesc[i]->GetType() & DEVICE_TYPE_OPENCL_ALL) {
			// OpenCL devices
			OpenCLDeviceDescription *oclDeviceDesc = (OpenCLDeviceDescription *)deviceDesc[i];

			device = new OpenCLIntersectionDevice(this, oclDeviceDesc, indexOffset + i);
		}
#endif
		else
			assert (false);

		newDevices.push_back(device);
	}

	return newDevices;
}

std::vector<IntersectionDevice *> Context::AddIntersectionDevices(std::vector<DeviceDescription *> &deviceDesc) {
	assert (!started);

	std::vector<IntersectionDevice *> newDevices = CreateIntersectionDevices(deviceDesc, idevices.size());
	for (size_t i = 0; i < newDevices.size(); ++i)
		idevices.push_back(newDevices[i]);

	return newDevices;
}

std::vector<IntersectionDevice *> Context::AddVirtualIntersectionDevice(
	std::vector<DeviceDescription *> &deviceDescs) {
	assert (!started);
	assert (deviceDescs.size() > 0);

	std::vector<IntersectionDevice *> realDevices = CreateIntersectionDevices(deviceDescs, idevices.size());
	VirtualIntersectionDevice *virtualDevice = new VirtualIntersectionDevice(realDevices, idevices.size());
	idevices.push_back(virtualDevice);

	return realDevices;
}