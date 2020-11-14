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

#include <cstdlib>
#include <cassert>
#include <iosfwd>
#include <sstream>
#include <stdexcept>

#include "luxrays/core/context.h"
#include "luxrays/core/hardwaredevice.h"
#include "luxrays/devices/nativeintersectiondevice.h"
#if !defined(LUXRAYS_DISABLE_OPENCL)
#include "luxrays/devices/ocldevice.h"
#include "luxrays/devices/oclintersectiondevice.h"
#endif
#if !defined(LUXRAYS_DISABLE_CUDA)
#include "luxrays/devices/cudadevice.h"
#include "luxrays/devices/cudaintersectiondevice.h"
#endif

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// Context
//------------------------------------------------------------------------------

Context::Context(LuxRaysDebugHandler handler, const Properties &config) : cfg(config) {
	debugHandler = handler;
	currentDataSet = NULL;
	started = false;
	useOutOfCoreBuffers = false;
	verbose = cfg.Get(Property("context.verbose")(true)).Get<bool>();

	// Get the list of devices available on the platform
	
	//--------------------------------------------------------------------------
	// Add all native devices
	//--------------------------------------------------------------------------

	NativeIntersectionDeviceDescription::AddDeviceDescs(deviceDescriptions);
	
#if !defined(LUXRAYS_DISABLE_OPENCL)
	//--------------------------------------------------------------------------
	// Add all OpenCL devices
	//--------------------------------------------------------------------------

	LR_LOG(this, "OpenCL support: enabled");

	if (isOpenCLAvilable) {
		vector<cl_platform_id> platforms;
		OpenCLDeviceDescription::GetPlatformsList(platforms);

		for (size_t i = 0; i < platforms.size(); ++i)
			LR_LOG(this, "OpenCL Platform " << i << ": " << OpenCLDeviceDescription::GetOCLPlatformName(platforms[i]));

		const int openclPlatformIndex = cfg.Get(Property("context.opencl.platform.index")(-1)).Get<int>();
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
				throw runtime_error("Unable to find an appropriate OpenCL platform");
			else {
				OpenCLDeviceDescription::AddDeviceDescs(
					platforms[openclPlatformIndex],
					DEVICE_TYPE_OPENCL_ALL, deviceDescriptions);
			}
		}
	}
#else
	LR_LOG(this, "OpenCL support: disabled");
#endif

#if !defined(LUXRAYS_DISABLE_CUDA)
	//--------------------------------------------------------------------------
	// Add all CUDA devices
	//--------------------------------------------------------------------------
	
	LR_LOG(this, "CUDA support: enabled");

	if (isCudaAvilable) {
		LR_LOG(this, "CUDA support: available");

		int driverVersion;
		CHECK_CUDA_ERROR(cuDriverGetVersion(&driverVersion));
		LR_LOG(this, "CUDA driver version: " << (driverVersion / 1000) << "." << (driverVersion % 1000));

		int devCount;
		CHECK_CUDA_ERROR(cuDeviceGetCount(&devCount));
		LR_LOG(this, "CUDA device count: " << devCount);

		if (isOptixAvilable) {
			LR_LOG(this, "Optix support: available");
		} else {
			LR_LOG(this, "Optix support: not available");
		}

		CUDADeviceDescription::AddDeviceDescs(deviceDescriptions);
	}
#else
	LR_LOG(this, "CUDA support: disabled");
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

		LR_LOG(this, "Device " << i << " has out of core memory support: " <<
			desc->HasOutOfCoreMemorySupport());

#if !defined(LUXRAYS_DISABLE_CUDA)
		if (desc->GetType() & DEVICE_TYPE_CUDA_ALL) {
			const CUDADeviceDescription *cudaDesc = (CUDADeviceDescription *)desc;

			LR_LOG(this, "Device " << i << " CUDA compute capability: " <<
					cudaDesc->GetCUDAComputeCapabilityMajor() << "." << cudaDesc->GetCUDAComputeCapabilityMinor());
		}
#endif
	}
}

Context::~Context() {
	if (started)
		Stop();

	for (size_t i = 0; i < devices.size(); ++i)
		delete devices[i];

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
	currentDataSet->UpdateAccelerators();

#if !defined(LUXRAYS_DISABLE_OPENCL)
	// Update all hardware intersection devices
	for (auto device : idevices) {
		HardwareIntersectionDevice *hardwareIntersectionDevice = dynamic_cast<HardwareIntersectionDevice *>(device);
		if (hardwareIntersectionDevice)
			hardwareIntersectionDevice->Update();
	}
#endif
}

void Context::Start() {
	assert (!started);

	for (auto device : devices) {
		device->PushThreadCurrentDevice();
		device->Start();
		device->PopThreadCurrentDevice();
	}

	started = true;
}

void Context::Interrupt() {
	assert (started);

	for (auto device : devices) {
		device->PushThreadCurrentDevice();
		device->Interrupt();
		device->PopThreadCurrentDevice();
	}
}

void Context::Stop() {
	assert (started);

	Interrupt();

	for (auto device : devices) {
		device->PushThreadCurrentDevice();
		device->Stop();
		device->PopThreadCurrentDevice();
	}

	started = false;
}

const vector<DeviceDescription *> &Context::GetAvailableDeviceDescriptions() const {
	return deviceDescriptions;
}

const vector<IntersectionDevice *> &Context::GetIntersectionDevices() const {
	return idevices;
}

const vector<HardwareDevice *> &Context::GetHardwareDevices() const {
	return hdevices;
}

const vector<Device *> &Context::GetDevices() const {
	return devices;
}

vector<IntersectionDevice *> Context::CreateIntersectionDevices(
	vector<DeviceDescription *> &deviceDesc, const size_t indexOffset) {
	assert (!started);

	LR_LOG(this, "Creating " << deviceDesc.size() << " intersection device(s)");

	vector<IntersectionDevice *> newDevices;
	for (size_t i = 0; i < deviceDesc.size(); ++i) {
		LR_LOG(this, "Allocating intersection device " << i << ": " << deviceDesc[i]->GetName() <<
				" (Type = " << DeviceDescription::GetDeviceType(deviceDesc[i]->GetType()) << ")");

		const DeviceType deviceType = deviceDesc[i]->GetType();
		IntersectionDevice *device;
		if (deviceType == DEVICE_TYPE_NATIVE) {
			// Nathive thread devices
			NativeIntersectionDeviceDescription *nativeDeviceDesc = (NativeIntersectionDeviceDescription *)deviceDesc[i];
			device = new NativeIntersectionDevice(this, nativeDeviceDesc, indexOffset + i);
		}
#if !defined(LUXRAYS_DISABLE_OPENCL)
		else if (deviceType & DEVICE_TYPE_OPENCL_ALL) {
			// OpenCL devices
			OpenCLDeviceDescription *oclDeviceDesc = (OpenCLDeviceDescription *)deviceDesc[i];

			device = new OpenCLIntersectionDevice(this, oclDeviceDesc, indexOffset + i);
		}
#endif
#if !defined(LUXRAYS_DISABLE_CUDA)
		else if (deviceType & DEVICE_TYPE_CUDA_ALL) {
			// CUDA devices
			CUDADeviceDescription *cudaDeviceDesc = (CUDADeviceDescription *)deviceDesc[i];

			device = new CUDAIntersectionDevice(this, cudaDeviceDesc, indexOffset + i);
		}
#endif
		else
			throw runtime_error("Unknown device type in Context::CreateIntersectionDevices(): " + ToString(deviceType));

		newDevices.push_back(device);
	}

	return newDevices;
}

vector<IntersectionDevice *> Context::AddIntersectionDevices(vector<DeviceDescription *> &deviceDesc) {
	assert (!started);

	vector<IntersectionDevice *> newDevices = CreateIntersectionDevices(deviceDesc, idevices.size());
	for (size_t i = 0; i < newDevices.size(); ++i) {
		idevices.push_back(newDevices[i]);
		devices.push_back(newDevices[i]);
	}

	return newDevices;
}

vector<HardwareDevice *> Context::CreateHardwareDevices(
	vector<DeviceDescription *> &deviceDesc, const size_t indexOffset) {
	assert (!started);

	LR_LOG(this, "Creating " << deviceDesc.size() << " hardware device(s)");

	vector<HardwareDevice *> newDevices;
	for (size_t i = 0; i < deviceDesc.size(); ++i) {
		LR_LOG(this, "Allocating hardware device " << i << ": " << deviceDesc[i]->GetName() <<
				" (Type = " << DeviceDescription::GetDeviceType(deviceDesc[i]->GetType()) << ")");

		const DeviceType deviceType = deviceDesc[i]->GetType();
		HardwareDevice *device;
		if (deviceType == DEVICE_TYPE_NATIVE) {
			throw runtime_error("Native devices are not supported as hardware devices in Context::CreateHardwareDevices()");
		}
#if !defined(LUXRAYS_DISABLE_OPENCL)
		else if (deviceType & DEVICE_TYPE_OPENCL_ALL) {
			// OpenCL devices
			OpenCLDeviceDescription *oclDeviceDesc = (OpenCLDeviceDescription *)deviceDesc[i];

			device = new OpenCLDevice(this, oclDeviceDesc, indexOffset + i);
		}
#endif
#if !defined(LUXRAYS_DISABLE_CUDA)
		else if (deviceType & DEVICE_TYPE_CUDA_ALL) {
			// CUDA devices
			CUDADeviceDescription *cudaDeviceDesc = (CUDADeviceDescription *)deviceDesc[i];

			device = new CUDADevice(this, cudaDeviceDesc, indexOffset + i);
		}
#endif
		else
			throw runtime_error("Unknown device type in Context::CreateHardwareDevices(): " + ToString(deviceType));

		newDevices.push_back(device);
	}

	return newDevices;
}

vector<HardwareDevice *> Context::AddHardwareDevices(vector<DeviceDescription *> &deviceDesc) {
	assert (!started);

	vector<HardwareDevice *> newDevices = CreateHardwareDevices(deviceDesc, hdevices.size());
	for (size_t i = 0; i < newDevices.size(); ++i) {
		hdevices.push_back(newDevices[i]);
		devices.push_back(newDevices[i]);
	}

	return newDevices;
}
