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

#if !defined(WIN32) && !defined(__APPLE__)
#include <GL/glx.h>
#endif
#include <cstdio>
using std::sprintf;

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/pixeldevice.h"
#include "luxrays/core/context.h"
#include "luxrays/kernels/kernels.h"

namespace luxrays {

//------------------------------------------------------------------------------
// DeviceDescription
//------------------------------------------------------------------------------

void DeviceDescription::FilterOne(std::vector<DeviceDescription *> &deviceDescriptions) {
	int gpuIndex = -1;
	int cpuIndex = -1;
	for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
		if ((cpuIndex == -1) && (deviceDescriptions[i]->GetType() == DEVICE_TYPE_NATIVE_THREAD))
			cpuIndex = i;
#if !defined(LUXRAYS_DISABLE_OPENCL)
		else if ((gpuIndex == -1) && (deviceDescriptions[i]->GetType() == DEVICE_TYPE_OPENCL) &&
				(((OpenCLDeviceDescription *)deviceDescriptions[i])->GetOpenCLType() == OCL_DEVICE_TYPE_GPU)) {
			gpuIndex = i;
			break;
		}
#endif
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
		std::vector<DeviceDescription *> &deviceDescriptions) {
	size_t i = 0;
	while (i < deviceDescriptions.size()) {
		if ((type != DEVICE_TYPE_ALL) && (deviceDescriptions[i]->GetType() != type)) {
			// Remove the device from the list
			deviceDescriptions.erase(deviceDescriptions.begin() + i);
		} else
			++i;
	}
}

std::string DeviceDescription::GetDeviceType(const DeviceType type) {
	switch (type) {
		case DEVICE_TYPE_ALL:
			return "ALL";
		case DEVICE_TYPE_NATIVE_THREAD:
			return "NATIVE_THREAD";
		case DEVICE_TYPE_OPENCL:
			return "OPENCL";
		case DEVICE_TYPE_VIRTUAL:
			return "VIRTUAL";
		default:
			return "UNKNOWN";
	}
}

//------------------------------------------------------------------------------
// Device
//------------------------------------------------------------------------------

Device::Device(const Context *context, const DeviceType type, const unsigned int index) :
	deviceContext(context), deviceType(type) {
	deviceIndex = index;
	started = false;
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
	unsigned int count = boost::thread::hardware_concurrency();

	// Build the descriptions
	char buf[64];
	for (size_t i = 0; i < count; ++i) {
		sprintf(buf, "NativeThread-%03d", (int)i);
		std::string deviceName = std::string(buf);

		descriptions.push_back(new NativeThreadDeviceDescription(deviceName, i));
	}
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

std::string OpenCLDeviceDescription::GetDeviceType(const OpenCLDeviceType type) {
	switch (type) {
		case OCL_DEVICE_TYPE_ALL:
			return "ALL";
		case OCL_DEVICE_TYPE_DEFAULT:
			return "DEFAULT";
		case OCL_DEVICE_TYPE_CPU:
			return "CPU";
		case OCL_DEVICE_TYPE_GPU:
			return "GPU";
		default:
			return "UNKNOWN";
	}
}

OpenCLDeviceType OpenCLDeviceDescription::GetOCLDeviceType(const cl_int type) {
	switch (type) {
		case CL_DEVICE_TYPE_ALL:
			return OCL_DEVICE_TYPE_ALL;
		case CL_DEVICE_TYPE_DEFAULT:
			return OCL_DEVICE_TYPE_DEFAULT;
		case CL_DEVICE_TYPE_CPU:
			return OCL_DEVICE_TYPE_CPU;
		case CL_DEVICE_TYPE_GPU:
			return OCL_DEVICE_TYPE_GPU;
		default:
			return OCL_DEVICE_TYPE_UNKNOWN;
	}
}

void OpenCLDeviceDescription::AddDeviceDescs(
	const cl::Platform &oclPlatform, const OpenCLDeviceType filter,
	std::vector<DeviceDescription *> &descriptions) {
	// Get the list of devices available on the platform
	VECTOR_CLASS<cl::Device> oclDevices;
	oclPlatform.getDevices(CL_DEVICE_TYPE_ALL, &oclDevices);

	// Build the descriptions
	for (size_t i = 0; i < oclDevices.size(); ++i) {
		OpenCLDeviceType type = GetOCLDeviceType(oclDevices[i].getInfo<CL_DEVICE_TYPE>());

		if ((filter == OCL_DEVICE_TYPE_ALL) || (filter == type)) {
			OpenCLDeviceDescription *desc = new OpenCLDeviceDescription(oclDevices[i], i);
			descriptions.push_back(desc);
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

void OpenCLDeviceDescription::Filter(const OpenCLDeviceType type,
		std::vector<DeviceDescription *> &deviceDescriptions) {
	if (type == OCL_DEVICE_TYPE_ALL)
		return;

	size_t i = 0;
	while (i < deviceDescriptions.size()) {
		if ((deviceDescriptions[i]->GetType() == DEVICE_TYPE_OPENCL) &&
				(((OpenCLDeviceDescription *)deviceDescriptions[i])->GetOpenCLType() != type)) {
			// Remove the device from the list
			deviceDescriptions.erase(deviceDescriptions.begin() + i);
		} else
			++i;
	}
}

#endif

//------------------------------------------------------------------------------
// IntersectionDevice
//------------------------------------------------------------------------------

IntersectionDevice::IntersectionDevice(const Context *context, const DeviceType type, const unsigned int index) :
	Device(context, type, index) {
	dataSet = NULL;
}

IntersectionDevice::~IntersectionDevice() {
}

void IntersectionDevice::SetDataSet(const DataSet *newDataSet) {
	assert (!started);
	assert (newDataSet != NULL);
	assert (newDataSet->IsPreprocessed());

	dataSet = newDataSet;
}

void IntersectionDevice::Start() {
	assert (dataSet != NULL);

	Device::Start();

	statsStartTime = WallClockTime();
	statsTotalRayCount = 0.0;
	statsDeviceIdleTime = 0.0;
	statsDeviceTotalTime = 0.0;
}

//------------------------------------------------------------------------------
// PixelDevice
//------------------------------------------------------------------------------

PixelDevice::PixelDevice(const Context *context, const DeviceType type, const unsigned int index) :
	Device(context, type, index) {
}

PixelDevice::~PixelDevice() {
}

void PixelDevice::Init(const unsigned int w, const unsigned int h) {
	assert (!started);

	width = w;
	height = h;
}

void PixelDevice::Start() {
	Device::Start();

	statsTotalSampleTime = 0.0;
	statsTotalSamplesCount = 0.0;
}

}
