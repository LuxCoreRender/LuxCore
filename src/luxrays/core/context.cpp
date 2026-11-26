/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
#include <functional>
#include <iosfwd>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "luxrays/core/device.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/usings.h"
#include "luxrays/core/context.h"
#include "cuew.h"
#include "luxrays/core/hardwaredevice.h"
#include "luxrays/core/hardwareintersectiondevice.h"
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

Context::Context(LuxRaysDebugHandler handler, PropertiesUPtr&& config)
	: cfg(config ? std::move(config) : std::make_unique<Properties>())
{
	debugHandler = handler;
	currentDataSet = NULL;
	started = false;
	useOutOfCoreBuffers = false;
	verbose = cfg->Get(Property("context.verbose")(true)).Get<bool>();

	// Get the list of devices available on the platform

	//--------------------------------------------------------------------------
	// Add all native devices
	//--------------------------------------------------------------------------

	NativeIntersectionDeviceDescription::AddDeviceDescs(deviceDescriptions);

#if !defined(LUXRAYS_DISABLE_OPENCL)
	//--------------------------------------------------------------------------
	// Add all OpenCL devices
	//--------------------------------------------------------------------------

	LR_LOG((*this), "OpenCL support: enabled");

	if (isOpenCLAvilable) {
		vector<cl_platform_id> platforms;
		OpenCLDeviceDescription::GetPlatformsList(platforms);

		for (size_t i = 0; i < platforms.size(); ++i)
			LR_LOG((*this), "OpenCL Platform " << i << ": " << OpenCLDeviceDescription::GetOCLPlatformName(platforms[i]));

		const int openclPlatformIndex = cfg->Get(
			Property("context.opencl.platform.index")(-1)
		).Get<int>();

		if (openclPlatformIndex < 0) {
			if (platforms.size() > 0) {
				// Just use all the platforms available
				for (size_t i = 0; i < platforms.size(); ++i)
					OpenCLDeviceDescription::AddDeviceDescs(
						platforms[i], DEVICE_TYPE_OPENCL_ALL,
						deviceDescriptions);
			} else
				LR_LOG((*this), "No OpenCL platform available");
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
	LR_LOG((*this), "OpenCL support: disabled");
#endif

#if !defined(LUXRAYS_DISABLE_CUDA)
	//--------------------------------------------------------------------------
	// Add all CUDA devices
	//--------------------------------------------------------------------------
	
	LR_LOG((*this), "CUDA support: enabled");

	if (isCudaAvilable) {
		if(!cuDriverGetVersion) {
			LR_LOG((*this), "Warning: No CUDA API available");
			return;
		}
		LR_LOG((*this), "CUDA support: available");

		int driverVersion;
		CHECK_CUDA_ERROR(cuDriverGetVersion(&driverVersion));
		LR_LOG((*this), "CUDA driver version: " << (driverVersion / 1000) << "." << (driverVersion % 1000));

		int devCount;
		CHECK_CUDA_ERROR(cuDeviceGetCount(&devCount));
		LR_LOG((*this), "CUDA device count: " << devCount);

		if (isOptixAvilable) {
			LR_LOG((*this), "Optix support: available");
		} else {
			LR_LOG((*this), "Optix support: not available");
		}

		CUDADeviceDescription::AddDeviceDescs(deviceDescriptions);
	}
#else
	LR_LOG((*this), "CUDA support: disabled");
#endif

	// Print device info
	for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
		DeviceDescriptionRPtr desc = deviceDescriptions[i];
		LR_LOG((*this), "Device " << i << " name: " <<
			desc->GetName());

		LR_LOG((*this), "Device " << i << " type: " <<
			DeviceDescription::GetDeviceType(desc->GetType()));

		LR_LOG((*this), "Device " << i << " compute units: " <<
			desc->GetComputeUnits());

		LR_LOG((*this), "Device " << i << " preferred float vector width: " <<
			desc->GetNativeVectorWidthFloat());

		LR_LOG((*this), "Device " << i << " max allocable memory: " <<
			desc->GetMaxMemory() / (1024 * 1024) << "MBytes");

		LR_LOG((*this), "Device " << i << " max allocable memory block size: " <<
			desc->GetMaxMemoryAllocSize() / (1024 * 1024) << "MBytes");

		LR_LOG((*this), "Device " << i << " has out of core memory support: " <<
			desc->HasOutOfCoreMemorySupport());

#if !defined(LUXRAYS_DISABLE_CUDA)
		if (desc->GetType() & DEVICE_TYPE_CUDA_ALL) {
			const auto& cudaDesc = static_cast<CUDADeviceDescriptionConstRef>(*desc);

			LR_LOG((*this), "Device " << i << " CUDA compute capability: " <<
					cudaDesc.GetCUDAComputeCapabilityMajor() << "." <<
					cudaDesc.GetCUDAComputeCapabilityMinor());
		}
#endif
	}
}

Context::~Context() {
	if (started) Stop();
}

void Context::SetDataSet(DataSetSPtr dataSet) {
	assert (!started);

	currentDataSet = dataSet;

	for (IntersectionDeviceRef dev : idevices)
		dev.SetDataSet(currentDataSet);
}

void Context::UpdateDataSet() {
	assert (started);

	// Update the data set
	currentDataSet->UpdateAccelerators();

#if !defined(LUXRAYS_DISABLE_OPENCL)
	// Update all hardware intersection devices
	for (IntersectionDeviceRef device : idevices) {

		try {

			auto& hardwareIntersectionDevice =
				dynamic_cast<HardwareIntersectionDeviceRef>(device);

			hardwareIntersectionDevice.Update();

		}
		catch(std::bad_cast&) {
			continue;
		}

	}
#endif
}

void Context::Start() {
	assert (!started);

	for (auto& device : devices) {
		device->PushThreadCurrentDevice();
		device->Start();
		device->PopThreadCurrentDevice();
	}

	started = true;
}

void Context::Interrupt() {
	assert (started);

	for (auto& device : devices) {
		device->PushThreadCurrentDevice();
		device->Interrupt();
		device->PopThreadCurrentDevice();
	}
}

void Context::Stop() {
	assert (started);

	Interrupt();

	for (auto& device : devices) {
		device->PushThreadCurrentDevice();
		device->Stop();
		device->PopThreadCurrentDevice();
	}

	started = false;
}

DeviceDescriptions
Context::GetAvailableDeviceDescriptions() const {
	DeviceDescriptions res;
	for (auto& desc : deviceDescriptions) {
		res.push_back(std::ref(*desc));
	}
	return res;
}


const vector<DeviceUPtr> & Context::GetDevices() const {
	return devices;
}

std::vector<IntersectionDeviceUPtr> Context::CreateIntersectionDevices(
	const DeviceDescriptions &deviceDesc,
	const size_t indexOffset
) {
	assert (!started);

	LR_LOG((*this), "Creating " << deviceDesc.size() << " intersection device(s)");

	std::vector<IntersectionDeviceUPtr> newDevices;
	for (size_t i = 0; i < deviceDesc.size(); ++i) {
		DeviceDescriptionRef devDesc = deviceDesc[i];
		LR_LOG(
			(*this),
			"Allocating intersection device " << i << ": "
			<< devDesc.GetName()
			<< " (Type = "
			<< DeviceDescription::GetDeviceType(devDesc.GetType())
			<< ")"
		);

		const DeviceType deviceType = devDesc.GetType();
		IntersectionDeviceUPtr device;

		if (deviceType == DEVICE_TYPE_NATIVE) {
			// Native thread devices
			const auto& nativeDeviceDesc =
				static_cast<NativeIntersectionDeviceDescriptionConstRef>(devDesc);
			device = std::make_unique<NativeIntersectionDevice>(
				*this,
				nativeDeviceDesc,
				indexOffset + i
			);
		}
#if !defined(LUXRAYS_DISABLE_OPENCL)
		else if (deviceType & DEVICE_TYPE_OPENCL_ALL) {
			// OpenCL devices
			const auto& oclDeviceDesc =
				static_cast<OpenCLDeviceDescriptionConstRef>(devDesc);

			device = std::make_unique<OpenCLIntersectionDevice>(
				*this, oclDeviceDesc, indexOffset + i
			);
		}
#endif
#if !defined(LUXRAYS_DISABLE_CUDA)
		else if (deviceType & DEVICE_TYPE_CUDA_ALL) {
			// CUDA devices
			const auto& cudaDeviceDesc =
				static_cast<CUDADeviceDescriptionConstRef>(devDesc);

			device = std::make_unique<CUDAIntersectionDevice>(
				*this, cudaDeviceDesc, indexOffset + i
			);
		}
#endif
		else {
			throw runtime_error(
				"Unknown device type in Context::CreateIntersectionDevices(): "
				+ ToString(deviceType)
			);
		}

		newDevices.push_back(std::move(device));
	}

	return newDevices;
}

std::vector<std::reference_wrapper<IntersectionDevice>>
Context::AddIntersectionDevices(
	const DeviceDescriptions & deviceDesc
) {
	assert (!started);

	std::vector<std::reference_wrapper<IntersectionDevice>> res;

	auto newDevices = CreateIntersectionDevices(deviceDesc, idevices.size());

	for (auto& dev : newDevices) {
		devices.push_back(std::move(dev));
		DeviceRef back = *devices.back();
		auto newdev = std::ref<IntersectionDevice>(
			dynamic_cast<IntersectionDeviceRef>(back)
		);
		idevices.push_back(newdev);
		res.push_back(newdev);
	}
	return res;
}

std::vector<HardwareDeviceUPtr> Context::CreateHardwareDevices(
	const DeviceDescriptions &deviceDesc,
	const size_t indexOffset
) {
	assert (!started);

	LR_LOG((*this), "Creating " << deviceDesc.size() << " hardware device(s)");

	std::vector<HardwareDeviceUPtr> newDevices;
	for (size_t i = 0; i < deviceDesc.size(); ++i) {
		DeviceDescriptionRef devDesc = deviceDesc[i];
		LR_LOG(
			(*this),
			"Allocating hardware device " << i << ": "
			<< devDesc.GetName()
			<< " (Type = "
			<< DeviceDescription::GetDeviceType(devDesc.GetType())
			<< ")"
		);

		const DeviceType deviceType = devDesc.GetType();
		HardwareDeviceUPtr device;
		if (deviceType == DEVICE_TYPE_NATIVE) {
			throw runtime_error(
				"Native devices are not supported as hardware devices"
				"in Context::CreateHardwareDevices()"
			);
		}
#if !defined(LUXRAYS_DISABLE_OPENCL)
		else if (deviceType & DEVICE_TYPE_OPENCL_ALL) {
			// OpenCL devices
			const auto& oclDeviceDesc =
				static_cast<OpenCLDeviceDescriptionConstRef>(devDesc);

			device = std::make_unique<OpenCLDevice>(
				*this, oclDeviceDesc, indexOffset + i
			);
		}
#endif
#if !defined(LUXRAYS_DISABLE_CUDA)
		else if (deviceType & DEVICE_TYPE_CUDA_ALL) {
			// CUDA devices
			const auto& cudaDeviceDesc =
				static_cast<CUDADeviceDescriptionConstRef>(devDesc);

			device = std::make_unique<CUDADevice>(
				*this, cudaDeviceDesc, indexOffset + i
			);
		}
#endif
		else
			throw runtime_error(
				"Unknown device type in Context::CreateHardwareDevices(): "
				+ ToString(deviceType)
			);

		newDevices.push_back(std::move(device));
	}

	return newDevices;
}

std::vector<std::reference_wrapper<HardwareDevice>>
Context::AddHardwareDevices(
	const DeviceDescriptions & deviceDesc
) {
	assert (!started);
	std::vector<std::reference_wrapper<HardwareDevice>> res;

	auto newDevices = CreateHardwareDevices(deviceDesc, hdevices.size());

	for (auto& dev : newDevices) {
		devices.push_back(std::move(dev));
		DeviceRef back = *devices.back();
		auto newdev = std::ref<HardwareDevice>(
			dynamic_cast<HardwareDeviceRef>(back)
		);
		hdevices.push_back(newdev);
		res.push_back(newdev);
	}
	return res;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
