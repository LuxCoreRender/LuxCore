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

#include "luxrays/core/oclintersectiondevice.h"
#include "luxcore/cfg.h"

#include "slg/film/film.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Film OpenCL related code
//------------------------------------------------------------------------------

void Film::SetUpOCL() {
	oclEnable = true;
	oclPlatformIndex = -1;
	oclDeviceIndex = -1;

	ctx = NULL;
	oclIntersectionDevice = NULL;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	kernelCache = NULL;
	ocl_RGB_TONEMAPPED = NULL;
	ocl_FRAMEBUFFER_MASK = NULL;
#endif
}

void Film::CreateOCLContext() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	// Create LuxRays context
	ctx = new Context(LuxRays_DebugHandler ? LuxRays_DebugHandler : NullDebugHandler, oclPlatformIndex);

	// Select OpenCL device
	vector<DeviceDescription *> descs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_OPENCL_ALL, descs);

	selectedDeviceDesc = NULL;
	if (oclEnable) {
		if ((oclDeviceIndex >= 0) && (oclDeviceIndex < (int)descs.size())) {
			// I have to use specific device
			selectedDeviceDesc = (OpenCLDeviceDescription *)descs[oclDeviceIndex];
		} else if (descs.size() > 0) {
			// Look for a GPU to use
			for (size_t i = 0; i < descs.size(); ++i) {
				OpenCLDeviceDescription *desc = (OpenCLDeviceDescription *)descs[i];

				if (desc->GetType() == DEVICE_TYPE_OPENCL_GPU) {
					selectedDeviceDesc = desc;
					break;
				}
			}
		} else {
			// No OpenCL device available
		}
	}

	if (selectedDeviceDesc) {
		// Allocate the device
		vector<luxrays::DeviceDescription *> selectedDeviceDescs;
		selectedDeviceDescs.push_back(selectedDeviceDesc);
		vector<IntersectionDevice *> devs = ctx->AddIntersectionDevices(selectedDeviceDescs);
		oclIntersectionDevice = (OpenCLIntersectionDevice *)devs[0];
		SLG_LOG("Film OpenCL Device used: " << oclIntersectionDevice->GetName());

		// Disable the support for hybrid rendering
		oclIntersectionDevice->SetDataParallelSupport(false);

		// Check if OpenCL 1.1 is available
		SLG_LOG("  Device OpenCL version: " << oclIntersectionDevice->GetDeviceDesc()->GetOpenCLVersion());
		if (!oclIntersectionDevice->GetDeviceDesc()->IsOpenCL_1_1()) {
			// NVIDIA drivers report OpenCL 1.0 even if they are 1.1 so I just
			// print a warning instead of throwing an exception
			SLG_LOG("WARNING: OpenCL version 1.1 or better is required. Device " + oclIntersectionDevice->GetName() + " may not work.");
		}

		DataSet *dataSet = new DataSet(ctx);
		dataSet->Preprocess();
		ctx->SetDataSet(dataSet);
		ctx->Start();

		kernelCache = new oclKernelPersistentCache("LUXCORE_" LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR);		
	}
#endif
}

void Film::DeleteOCLContext() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete kernelCache;

	oclIntersectionDevice->FreeBuffer(&ocl_RGB_TONEMAPPED);
	oclIntersectionDevice->FreeBuffer(&ocl_FRAMEBUFFER_MASK);

	delete ctx;
#endif
}

void Film::AllocateOCLBuffers() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	oclIntersectionDevice->AllocBufferRW(&ocl_RGB_TONEMAPPED, channel_RGB_TONEMAPPED->GetPixels(), channel_RGB_TONEMAPPED->GetSize(), "RGB_TONEMAPPED");

	oclIntersectionDevice->AllocBufferRO(&ocl_FRAMEBUFFER_MASK, channel_FRAMEBUFFER_MASK->GetPixels(), channel_FRAMEBUFFER_MASK->GetSize(), "FRAMEBUFFER_MASK");
#endif
}

void Film::WriteAllOCLBuffers() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	cl::CommandQueue &oclQueue = oclIntersectionDevice->GetOpenCLQueue();
	oclQueue.enqueueWriteBuffer(*ocl_RGB_TONEMAPPED, CL_FALSE, 0, channel_RGB_TONEMAPPED->GetSize(), channel_RGB_TONEMAPPED->GetPixels());
	oclQueue.enqueueWriteBuffer(*ocl_FRAMEBUFFER_MASK, CL_FALSE, 0, channel_FRAMEBUFFER_MASK->GetSize(), channel_FRAMEBUFFER_MASK->GetPixels());
#endif
}

void Film::ReadOCLBuffer_RGB_TONEMAPPED() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	cl::CommandQueue &oclQueue = oclIntersectionDevice->GetOpenCLQueue();
	oclQueue.enqueueReadBuffer(*ocl_RGB_TONEMAPPED, CL_FALSE, 0, channel_RGB_TONEMAPPED->GetSize(), channel_RGB_TONEMAPPED->GetPixels());
#endif
}

void Film::WriteOCLBuffer_RGB_TONEMAPPED() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	cl::CommandQueue &oclQueue = oclIntersectionDevice->GetOpenCLQueue();
	oclQueue.enqueueWriteBuffer(*ocl_RGB_TONEMAPPED, CL_FALSE, 0, channel_RGB_TONEMAPPED->GetSize(), channel_RGB_TONEMAPPED->GetPixels());
#endif
}
