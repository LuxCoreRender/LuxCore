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

#include "luxcore/cfg.h"

#include "slg/film/film.h"
#include "slg/kernels/kernels.h"

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

#if !defined(LUXRAYS_DISABLE_OPENCL)
	ctx = nullptr;
	dataSet = nullptr;
	hardwareDevice = nullptr;

	ocl_IMAGEPIPELINE = nullptr;
	ocl_ALPHA = nullptr;
	ocl_OBJECT_ID = nullptr;
	ocl_mergeBuffer = nullptr;

	mergeInitializeKernel = nullptr;
	mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel = nullptr;
	mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel = nullptr;
	mergeFinalizeKernel = nullptr;
#endif
}

#if !defined(LUXRAYS_DISABLE_OPENCL)
void Film::CreateOCLContext() {
	SLG_LOG("Film OpenCL image pipeline");

	// Create LuxRays context
	ctx = new Context(LuxRays_DebugHandler ? LuxRays_DebugHandler : NullDebugHandler,
			Properties() <<
			Property("context.opencl.platform.index")(oclPlatformIndex) <<
			Property("context.verbose")(false));

	// Select OpenCL device
	vector<DeviceDescription *> descs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_ALL_HARDWARE, descs);

	DeviceDescription *selectedDeviceDesc = nullptr;
	if (oclEnable) {
		if ((oclDeviceIndex >= 0) && (oclDeviceIndex < (int)descs.size())) {
			// I have to use specific device
			selectedDeviceDesc = (OpenCLDeviceDescription *)descs[oclDeviceIndex];
		} else if (descs.size() > 0) {
			// Look for a GPU to use
			for (size_t i = 0; i < descs.size(); ++i) {
				OpenCLDeviceDescription *desc = (OpenCLDeviceDescription *)descs[i];

				if (desc->GetType() == DEVICE_TYPE_CUDA_GPU) {
					selectedDeviceDesc = desc;
					break;

				}
				if (desc->GetType() == DEVICE_TYPE_OPENCL_GPU) {
					selectedDeviceDesc = desc;
					// I continue to scan other devices to check if there is a
					// CUDA one. CUDA is preferred over OpenCL if available.
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
		vector<HardwareDevice *> devs = ctx->AddHardwareDevices(selectedDeviceDescs);
		hardwareDevice = dynamic_cast<HardwareDevice *>(devs[0]);
		assert (hardwareDevice);
		SLG_LOG("Film OpenCL Device used: " << hardwareDevice->GetName());

		OpenCLDeviceDescription *oclDesc = dynamic_cast<OpenCLDeviceDescription *>(selectedDeviceDesc);
		if (oclDesc) {
			// Check if OpenCL 1.1 is available
			SLG_LOG("  Device OpenCL version: " << oclDesc->GetOpenCLVersion());
			if (!oclDesc->IsOpenCL_1_1()) {
				// NVIDIA drivers report OpenCL 1.0 even if they are 1.1 so I just
				// print a warning instead of throwing an exception
				SLG_LOG("WARNING: OpenCL version 1.1 or better is required. Device " + hardwareDevice->GetName() + " may not work.");
			}
		}

		// Just an empty data set
		dataSet = new DataSet(ctx);
		dataSet->Preprocess();
		ctx->SetDataSet(dataSet);
		ctx->Start();
	}
}

void Film::DeleteOCLContext() {
	if (hardwareDevice) {
		const size_t size = hardwareDevice->GetUsedMemory();
		SLG_LOG("[" << hardwareDevice->GetName() << "] Memory used for OpenCL image pipeline: " <<
				(size < 10000 ? size : (size / 1024)) << (size < 10000 ? "bytes" : "Kbytes"));

		delete mergeInitializeKernel;
		delete mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel;
		delete mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel;
		delete mergeFinalizeKernel;

		hardwareDevice->FreeBuffer(&ocl_IMAGEPIPELINE);
		hardwareDevice->FreeBuffer(&ocl_ALPHA);
		hardwareDevice->FreeBuffer(&ocl_OBJECT_ID);
		hardwareDevice->FreeBuffer(&ocl_mergeBuffer);
	}

	delete ctx;
	delete dataSet;
}

void Film::AllocateOCLBuffers() {
	ctx->SetVerbose(true);

	hardwareDevice->AllocBufferRW(&ocl_IMAGEPIPELINE, channel_IMAGEPIPELINEs[0]->GetPixels(), channel_IMAGEPIPELINEs[0]->GetSize(), "IMAGEPIPELINE");
	if (HasChannel(ALPHA))
		hardwareDevice->AllocBufferRO(&ocl_ALPHA, channel_ALPHA->GetPixels(), channel_ALPHA->GetSize(), "ALPHA");
	if (HasChannel(OBJECT_ID))
		hardwareDevice->AllocBufferRO(&ocl_OBJECT_ID, channel_OBJECT_ID->GetPixels(), channel_OBJECT_ID->GetSize(), "OBJECT_ID");
	const size_t mergeBufferSize = Max(
			HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) ? channel_RADIANCE_PER_PIXEL_NORMALIZEDs[0]->GetSize() : 0,
			HasChannel(RADIANCE_PER_SCREEN_NORMALIZED) ? channel_RADIANCE_PER_SCREEN_NORMALIZEDs[0]->GetSize() : 0);
	if (mergeBufferSize > 0)
		hardwareDevice->AllocBufferRO(&ocl_mergeBuffer, nullptr, mergeBufferSize, "Merge");

	ctx->SetVerbose(false);
}

void Film::CompileOCLKernels() {
	// Compile MergeSampleBuffersOCL() kernels
	const double tStart = WallClockTime();

	HardwareDeviceProgram *program = nullptr;
	hardwareDevice->CompileProgram(&program,
			"-D LUXRAYS_OPENCL_KERNEL -D SLG_OPENCL_KERNEL",
			slg::ocl::KernelSource_film_mergesamplebuffer_funcs,
			"MergeSampleBuffersOCL");

	//--------------------------------------------------------------------------
	// Film_ClearMergeBuffer kernel
	//--------------------------------------------------------------------------

	SLG_LOG("[MergeSampleBuffersOCL] Compiling Film_MergeBufferInitialize Kernel");
	hardwareDevice->GetKernel(program, &mergeInitializeKernel, "Film_MergeBufferInitialize");

	// Set kernel arguments
	u_int argIndex = 0;
	hardwareDevice->SetKernelArg(mergeInitializeKernel, argIndex++, width);
	hardwareDevice->SetKernelArg(mergeInitializeKernel, argIndex++, height);
	hardwareDevice->SetKernelArg(mergeInitializeKernel, argIndex++, ocl_IMAGEPIPELINE);

	//--------------------------------------------------------------------------
	// Film_MergeRADIANCE_PER_PIXEL_NORMALIZED kernel
	//--------------------------------------------------------------------------

	SLG_LOG("[MergeSampleBuffersOCL] Compiling Film_MergeRADIANCE_PER_PIXEL_NORMALIZED Kernel");
	hardwareDevice->GetKernel(program, &mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, "Film_MergeRADIANCE_PER_PIXEL_NORMALIZED");

	// Set kernel arguments
	argIndex = 0;
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, argIndex++, width);
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, argIndex++, height);
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, argIndex++, ocl_IMAGEPIPELINE);
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, argIndex++, ocl_mergeBuffer);
	// Scale RGB arguments are set at runtime

	//--------------------------------------------------------------------------
	// Film_MergeRADIANCE_PER_SCREEN_NORMALIZED kernel
	//--------------------------------------------------------------------------

	SLG_LOG("[MergeSampleBuffersOCL] Compiling Film_MergeRADIANCE_PER_SCREEN_NORMALIZED Kernel");
	hardwareDevice->GetKernel(program, &mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel, "Film_MergeRADIANCE_PER_SCREEN_NORMALIZED");

	// Set kernel arguments
	argIndex = 0;
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel, argIndex++, width);
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel, argIndex++, height);
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel, argIndex++, ocl_IMAGEPIPELINE);
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel, argIndex++, ocl_mergeBuffer);
	// Scale RGB arguments are set at runtime

	//--------------------------------------------------------------------------
	// Film_ClearMergeBuffer kernel
	//--------------------------------------------------------------------------

	SLG_LOG("[MergeSampleBuffersOCL] Compiling Film_MergeBufferFinalize Kernel");
	hardwareDevice->GetKernel(program, &mergeFinalizeKernel, "Film_MergeBufferFinalize");

	// Set kernel arguments
	argIndex = 0;
	hardwareDevice->SetKernelArg(mergeFinalizeKernel, argIndex++, width);
	hardwareDevice->SetKernelArg(mergeFinalizeKernel, argIndex++, height);
	hardwareDevice->SetKernelArg(mergeFinalizeKernel, argIndex++, ocl_IMAGEPIPELINE);

	//--------------------------------------------------------------------------

	delete program;

	const double tEnd = WallClockTime();
	SLG_LOG("[MergeSampleBuffersOCL] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void Film::WriteAllOCLBuffers() {
	if (HasChannel(ALPHA))
		hardwareDevice->EnqueueWriteBuffer(ocl_ALPHA, CL_FALSE,
				channel_ALPHA->GetSize(),
				channel_ALPHA->GetPixels());
	if (HasChannel(OBJECT_ID))
		hardwareDevice->EnqueueWriteBuffer(ocl_OBJECT_ID, CL_FALSE,
				channel_OBJECT_ID->GetSize(),
				channel_OBJECT_ID->GetPixels());
}

void Film::ReadOCLBuffer_IMAGEPIPELINE(const u_int index) {
	hardwareDevice->EnqueueReadBuffer(ocl_IMAGEPIPELINE, CL_FALSE,
			channel_IMAGEPIPELINEs[index]->GetSize(),
			channel_IMAGEPIPELINEs[index]->GetPixels());
}

void Film::WriteOCLBuffer_IMAGEPIPELINE(const u_int index) {
	hardwareDevice->EnqueueWriteBuffer(ocl_IMAGEPIPELINE, CL_FALSE,
			channel_IMAGEPIPELINEs[index]->GetSize(),
			channel_IMAGEPIPELINEs[index]->GetPixels());
}

void Film::MergeSampleBuffersOCL(const u_int imagePipelineIndex) {
	const ImagePipeline *ip = (imagePipelineIndex < imagePipelines.size()) ? imagePipelines[imagePipelineIndex] : nullptr;

	// Transfer IMAGEPIPELINEs[index]
	hardwareDevice->EnqueueWriteBuffer(ocl_IMAGEPIPELINE, CL_FALSE,
			channel_IMAGEPIPELINEs[imagePipelineIndex]->GetSize(),
			channel_IMAGEPIPELINEs[imagePipelineIndex]->GetPixels());

	// Initialize the framebuffer
	hardwareDevice->EnqueueKernel(mergeInitializeKernel,
			HardwareDeviceRange(RoundUp(pixelCount, 256u)), HardwareDeviceRange(256));

	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			if (!ip || ip->radianceChannelScales[i].enabled) {
				// Transfer RADIANCE_PER_PIXEL_NORMALIZEDs[i]
				hardwareDevice->EnqueueWriteBuffer(ocl_mergeBuffer, CL_FALSE,
						channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetSize(),
						channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixels());

				// Accumulate
				const Spectrum scale = ip ? ip->radianceChannelScales[i].GetScale() : Spectrum(1.f);
				hardwareDevice->SetKernelArg(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, 4, scale.c[0]);
				hardwareDevice->SetKernelArg(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, 5, scale.c[1]);
				hardwareDevice->SetKernelArg(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, 6, scale.c[2]);

				hardwareDevice->EnqueueKernel(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel,
						HardwareDeviceRange(RoundUp(pixelCount, 256u)), HardwareDeviceRange(256));
			}
		}
	}

	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount = samplesCounts.GetSampleCount_RADIANCE_PER_SCREEN_NORMALIZED();
		const float factor = (RADIANCE_PER_SCREEN_NORMALIZED_SampleCount > 0) ? (pixelCount / RADIANCE_PER_SCREEN_NORMALIZED_SampleCount) : 1.f;

		for (u_int i = 0; i < radianceGroupCount; ++i) {
			if (!ip || ip->radianceChannelScales[i].enabled) {
				// Transfer RADIANCE_PER_SCREEN_NORMALIZEDs[i]
				hardwareDevice->EnqueueWriteBuffer(ocl_mergeBuffer, CL_FALSE,
						channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetSize(),
						channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixels());

				// Accumulate
				const Spectrum scale = factor * (ip ? ip->radianceChannelScales[i].GetScale() : Spectrum(1.f));
				hardwareDevice->SetKernelArg(mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel, 4, scale.c[0]);
				hardwareDevice->SetKernelArg(mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel, 5, scale.c[1]);
				hardwareDevice->SetKernelArg(mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel, 6, scale.c[2]);

				hardwareDevice->EnqueueKernel(mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel,
						HardwareDeviceRange(RoundUp(pixelCount, 256u)), HardwareDeviceRange(256));
			}
		}
	}

	// Finalize the framebuffer
	hardwareDevice->EnqueueKernel(mergeFinalizeKernel,
			HardwareDeviceRange(RoundUp(pixelCount, 256u)), HardwareDeviceRange(256));

	// Transfer back the results
	hardwareDevice->EnqueueReadBuffer(ocl_IMAGEPIPELINE, CL_FALSE,
			channel_IMAGEPIPELINEs[imagePipelineIndex]->GetSize(),
			channel_IMAGEPIPELINEs[imagePipelineIndex]->GetPixels());

	hardwareDevice->FinishQueue();
}

#endif
