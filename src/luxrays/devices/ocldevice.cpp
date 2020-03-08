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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#if defined (__linux__)
#include <GL/glx.h>
#endif

#if defined(__APPLE__)
#include <OpenGL/OpenGL.h>
#endif

#include "luxrays/devices/ocldevice.h"

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCL Device Description
//------------------------------------------------------------------------------

std::string OpenCLDeviceDescription::GetDeviceType(const cl_uint type) {
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

}

#endif
