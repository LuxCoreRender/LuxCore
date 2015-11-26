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

#if !defined(LUXRAYS_DISABLE_OPENCL) && !defined(WIN32) && !defined(__APPLE__)
#include <GL/glx.h>
#endif

#ifdef LUXRAYS_DISABLE_OPENCL
#include "luxrays/core/intersectiondevice.h"
#else
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"
#endif
#include "luxrays/core/context.h"

namespace luxrays {

//------------------------------------------------------------------------------
// DeviceDescription
//------------------------------------------------------------------------------

void DeviceDescription::FilterOne(std::vector<DeviceDescription *> &deviceDescriptions)
{
	int gpuIndex = -1;
	int cpuIndex = -1;
	for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
		if ((cpuIndex == -1) && (deviceDescriptions[i]->GetType() &
			DEVICE_TYPE_NATIVE_THREAD))
			cpuIndex = (int)i;
		else if ((gpuIndex == -1) && (deviceDescriptions[i]->GetType() &
			DEVICE_TYPE_OPENCL_GPU)) {
			gpuIndex = (int)i;
			break;
		}
	}

	if (gpuIndex != -1) {
		std::vector<DeviceDescription *> selectedDev;
		selectedDev.push_back(deviceDescriptions[gpuIndex]);
		deviceDescriptions = selectedDev;
	} else if (gpuIndex != -1) {
		std::vector<DeviceDescription *> selectedDev;
		selectedDev.push_back(deviceDescriptions[cpuIndex]);
		deviceDescriptions = selectedDev;
	} else
		deviceDescriptions.clear();
}

void DeviceDescription::Filter(const DeviceType type,
	std::vector<DeviceDescription *> &deviceDescriptions)
{
	if (type == DEVICE_TYPE_ALL)
		return;
	size_t i = 0;
	while (i < deviceDescriptions.size()) {
		if ((deviceDescriptions[i]->GetType() & type) == 0) {
			// Remove the device from the list
			deviceDescriptions.erase(deviceDescriptions.begin() + i);
		} else
			++i;
	}
}

std::string DeviceDescription::GetDeviceType(const DeviceType type)
{
	switch (type) {
		case DEVICE_TYPE_ALL:
			return "ALL";
		case DEVICE_TYPE_NATIVE_THREAD:
			return "NATIVE_THREAD";
		case DEVICE_TYPE_OPENCL_ALL:
			return "OPENCL_ALL";
		case DEVICE_TYPE_OPENCL_DEFAULT:
			return "OPENCL_DEFAULT";
		case DEVICE_TYPE_OPENCL_CPU:
			return "OPENCL_CPU";
		case DEVICE_TYPE_OPENCL_GPU:
			return "OPENCL_GPU";
		case DEVICE_TYPE_OPENCL_UNKNOWN:
			return "OPENCL_UNKNOWN";
		case DEVICE_TYPE_VIRTUAL:
			return "VIRTUAL";
		default:
			return "UNKNOWN";
	}
}

//------------------------------------------------------------------------------
// Device
//------------------------------------------------------------------------------

Device::Device(const Context *context, const DeviceType type, const size_t index) :
	deviceContext(context), deviceType(type) {
	deviceIndex = index;
	started = false;
	usedMemory = 0;
}

Device::~Device() {
}

void Device::Start() {
	assert (!started);
	started = true;
}

void Device::Stop() {
	assert (started);
	started = false;
}

//------------------------------------------------------------------------------
// Native Device Description
//------------------------------------------------------------------------------

void NativeThreadDeviceDescription::AddDeviceDescs(std::vector<DeviceDescription *> &descriptions) {
	descriptions.push_back(new NativeThreadDeviceDescription("NativeThread"));
}

//------------------------------------------------------------------------------
// OpenCL Device Description
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)

std::string OpenCLDeviceDescription::GetDeviceType(const cl_int type) {
	switch (type) {
		case CL_DEVICE_TYPE_ALL:
			return "TYPE_ALL";
		case CL_DEVICE_TYPE_DEFAULT:
			return "TYPE_DEFAULT";
		case CL_DEVICE_TYPE_CPU:
			return "TYPE_CPU";
		case CL_DEVICE_TYPE_GPU:
			return "TYPE_GPU";
		default:
			return "TYPE_UNKNOWN";
	}
}

DeviceType OpenCLDeviceDescription::GetOCLDeviceType(const cl_device_type type)
{
	switch (type) {
		case CL_DEVICE_TYPE_ALL:
			return DEVICE_TYPE_OPENCL_ALL;
		case CL_DEVICE_TYPE_DEFAULT:
			return DEVICE_TYPE_OPENCL_DEFAULT;
		case CL_DEVICE_TYPE_CPU:
			return DEVICE_TYPE_OPENCL_CPU;
		case CL_DEVICE_TYPE_GPU:
			return DEVICE_TYPE_OPENCL_GPU;
		default:
			return DEVICE_TYPE_OPENCL_UNKNOWN;
	}
}

void OpenCLDeviceDescription::AddDeviceDescs(const cl::Platform &oclPlatform,
	const DeviceType filter, std::vector<DeviceDescription *> &descriptions)
{
	// Get the list of devices available on the platform
	VECTOR_CLASS<cl::Device> oclDevices;
	oclPlatform.getDevices(CL_DEVICE_TYPE_ALL, &oclDevices);

	// Build the descriptions
	for (size_t i = 0; i < oclDevices.size(); ++i) {
		DeviceType dev_type = GetOCLDeviceType(oclDevices[i].getInfo<CL_DEVICE_TYPE>());
		if (filter & dev_type)
		{
			/*if (dev_type == DEVICE_TYPE_OPENCL_CPU)
			{
				cl_device_partition_property_ext props[4] = { CL_DEVICE_PARTITION_BY_COUNTS_EXT, 1, 0, 0 };
				std::vector<cl::Device> subDevices;
				oclDevices[i].createSubDevices(props, &subDevices);
				descriptions.push_back(new OpenCLDeviceDescription(subDevices[0], i));
			} else*/
				descriptions.push_back(new OpenCLDeviceDescription(oclDevices[i], i));
		}
	}
}

cl::Context &OpenCLDeviceDescription::GetOCLContext() const {
	if (!oclContext) {
		// Allocate a context with the selected device
		VECTOR_CLASS<cl::Device> devices;
		devices.push_back(oclDevice);
		cl::Platform platform = oclDevice.getInfo<CL_DEVICE_PLATFORM>();

		if (enableOpenGLInterop) {
#if defined (__APPLE__)
			CGLContextObj kCGLContext = CGLGetCurrentContext();
			CGLShareGroupObj kCGLShareGroup = CGLGetShareGroup(kCGLContext);
			cl_context_properties cps[] = {
				CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE, (cl_context_properties)kCGLShareGroup,
				0
			};
#else
#ifdef WIN32
			cl_context_properties cps[] = {
				CL_GL_CONTEXT_KHR, (intptr_t)wglGetCurrentContext(),
				CL_WGL_HDC_KHR, (intptr_t)wglGetCurrentDC(),
				CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
				0
			};
#else
			cl_context_properties cps[] = {
				CL_GL_CONTEXT_KHR, (intptr_t)glXGetCurrentContext(),
				CL_GLX_DISPLAY_KHR, (intptr_t)glXGetCurrentDisplay(),
				CL_CONTEXT_PLATFORM, (cl_context_properties)platform(),
				0
			};
#endif
#endif
			oclContext = new cl::Context(devices, cps);
		} else {
			cl_context_properties cps[] = {
				CL_CONTEXT_PLATFORM, (cl_context_properties)platform(), 0
			};

			oclContext = new cl::Context(devices, cps);
		}
	}

	return *oclContext;
}

#endif

//------------------------------------------------------------------------------
// IntersectionDevice
//------------------------------------------------------------------------------

IntersectionDevice::IntersectionDevice(const Context *context,
	const DeviceType type, const size_t index) :
	Device(context, type, index), dataSet(NULL), queueCount(1), bufferCount(3),
	stackSize(24), dataParallelSupport(true), enableImageStorage(true) {
}

IntersectionDevice::~IntersectionDevice() {
}

void IntersectionDevice::SetDataSet(DataSet *newDataSet) {
	assert (!started);

	dataSet = newDataSet;
}

void IntersectionDevice::Start() {
	assert (dataSet != NULL);

	Device::Start();

	statsStartTime = WallClockTime();
	statsTotalSerialRayCount = 0.0;
	statsTotalDataParallelRayCount = 0.0;
	statsDeviceIdleTime = 0.0;
	statsDeviceTotalTime = 0.0;
}

void IntersectionDevice::SetQueueCount(const u_int count) {
	assert (!started);
	assert (count >= 1);

	queueCount = count;
}

}
