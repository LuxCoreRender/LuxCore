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
	ctx = NULL;
	selectedDeviceDesc = NULL;
	oclIntersectionDevice = NULL;

	kernelCache = NULL;
	ocl_IMAGEPIPELINE = NULL;
	ocl_FRAMEBUFFER_MASK = NULL;
	ocl_ALPHA = NULL;
	ocl_OBJECT_ID = NULL;
	ocl_mergeBuffer = NULL;

	clearFRAMEBUFFER_MASKKernel = NULL;
	mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel = NULL;
	mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel = NULL;
	notOverlappedScreenBufferUpdateKernel = NULL;
#endif
}

#if !defined(LUXRAYS_DISABLE_OPENCL)
void Film::CreateOCLContext() {
	SLG_LOG("Film OpenCL image pipeline");

	// Create LuxRays context
	ctx = new Context(LuxRays_DebugHandler ? LuxRays_DebugHandler : NullDebugHandler,
			oclPlatformIndex, false);

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
}

void Film::DeleteOCLContext() {
	if (oclIntersectionDevice) {
		const size_t size = oclIntersectionDevice->GetUsedMemory();
		SLG_LOG("[" << oclIntersectionDevice->GetName() << "] Memory used for OpenCL image pipeline: " <<
				(size < 10000 ? size : (size / 1024)) << (size < 10000 ? "bytes" : "Kbytes"));

		delete clearFRAMEBUFFER_MASKKernel;
		delete mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel;
		delete mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel;
		delete notOverlappedScreenBufferUpdateKernel;

		delete kernelCache;

		oclIntersectionDevice->FreeBuffer(&ocl_IMAGEPIPELINE);
		oclIntersectionDevice->FreeBuffer(&ocl_FRAMEBUFFER_MASK);
		oclIntersectionDevice->FreeBuffer(&ocl_ALPHA);
		oclIntersectionDevice->FreeBuffer(&ocl_OBJECT_ID);
		oclIntersectionDevice->FreeBuffer(&ocl_mergeBuffer);
	}

	delete ctx;
}

void Film::AllocateOCLBuffers() {
	ctx->SetVerbose(true);

	oclIntersectionDevice->AllocBufferRW(&ocl_IMAGEPIPELINE, channel_IMAGEPIPELINEs[0]->GetPixels(), channel_IMAGEPIPELINEs[0]->GetSize(), "IMAGEPIPELINE");
	oclIntersectionDevice->AllocBufferRW(&ocl_FRAMEBUFFER_MASK, channel_FRAMEBUFFER_MASK->GetPixels(), channel_FRAMEBUFFER_MASK->GetSize(), "FRAMEBUFFER_MASK");
	if (HasChannel(ALPHA))
		oclIntersectionDevice->AllocBufferRO(&ocl_ALPHA, channel_ALPHA->GetPixels(), channel_ALPHA->GetSize(), "ALPHA");
	if (HasChannel(OBJECT_ID))
		oclIntersectionDevice->AllocBufferRO(&ocl_OBJECT_ID, channel_OBJECT_ID->GetPixels(), channel_OBJECT_ID->GetSize(), "OBJECT_ID");

	const size_t mergeBufferSize = Max(
			HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) ? channel_RADIANCE_PER_PIXEL_NORMALIZEDs[0]->GetSize() : 0,
			HasChannel(RADIANCE_PER_SCREEN_NORMALIZED) ? channel_RADIANCE_PER_SCREEN_NORMALIZEDs[0]->GetSize() : 0);
	if (mergeBufferSize > 0)
		oclIntersectionDevice->AllocBufferRO(&ocl_mergeBuffer, mergeBufferSize, "Merge buffer");

	ctx->SetVerbose(false);
}

void Film::CompileOCLKernels() {
	// Compile MergeSampleBuffersOCL() kernels
	const double tStart = WallClockTime();

	cl::Program *program = ImagePipelinePlugin::CompileProgram(
			*this,
			"-D LUXRAYS_OPENCL_KERNEL -D SLG_OPENCL_KERNEL",
			slg::ocl::KernelSource_film_mergesamplebuffer_funcs,
			"MergeSampleBuffersOCL");

	//--------------------------------------------------------------------------
	// Film_ClearMergeBuffer kernel
	//--------------------------------------------------------------------------

	SLG_LOG("[MergeSampleBuffersOCL] Compiling Film_ClearMergeBuffer Kernel");
	clearFRAMEBUFFER_MASKKernel = new cl::Kernel(*program, "Film_ClearMergeBuffer");

	// Set kernel arguments
	u_int argIndex = 0;
	clearFRAMEBUFFER_MASKKernel->setArg(argIndex++, width);
	clearFRAMEBUFFER_MASKKernel->setArg(argIndex++, height);
	clearFRAMEBUFFER_MASKKernel->setArg(argIndex++, *ocl_FRAMEBUFFER_MASK);

	//--------------------------------------------------------------------------
	// Film_MergeRADIANCE_PER_PIXEL_NORMALIZED kernel
	//--------------------------------------------------------------------------

	SLG_LOG("[MergeSampleBuffersOCL] Compiling Film_MergeRADIANCE_PER_PIXEL_NORMALIZED Kernel");
	mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel = new cl::Kernel(*program, "Film_MergeRADIANCE_PER_PIXEL_NORMALIZED");

	// Set kernel arguments
	argIndex = 0;
	mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel->setArg(argIndex++, width);
	mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel->setArg(argIndex++, height);
	mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel->setArg(argIndex++, *ocl_IMAGEPIPELINE);
	mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel->setArg(argIndex++, *ocl_FRAMEBUFFER_MASK);
	mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel->setArg(argIndex++, *ocl_mergeBuffer);
	// Scale RGB arguments are set at runtime

	//--------------------------------------------------------------------------
	// Film_MergeRADIANCE_PER_SCREEN_NORMALIZED kernel
	//--------------------------------------------------------------------------

	SLG_LOG("[MergeSampleBuffersOCL] Compiling Film_MergeRADIANCE_PER_SCREEN_NORMALIZED Kernel");
	mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel = new cl::Kernel(*program, "Film_MergeRADIANCE_PER_SCREEN_NORMALIZED");

	// Set kernel arguments
	argIndex = 0;
	mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel->setArg(argIndex++, width);
	mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel->setArg(argIndex++, height);
	mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel->setArg(argIndex++, *ocl_IMAGEPIPELINE);
	mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel->setArg(argIndex++, *ocl_FRAMEBUFFER_MASK);
	mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel->setArg(argIndex++, *ocl_mergeBuffer);
	// Scale RGB arguments are set at runtime

	//--------------------------------------------------------------------------
	// Film_NotOverlappedScreenBufferUpdate kernel
	//--------------------------------------------------------------------------

	SLG_LOG("[MergeSampleBuffersOCL] Compiling Film_NotOverlappedScreenBufferUpdate Kernel");
	notOverlappedScreenBufferUpdateKernel = new cl::Kernel(*program, "Film_NotOverlappedScreenBufferUpdate");

	// Set kernel arguments
	argIndex = 0;
	notOverlappedScreenBufferUpdateKernel->setArg(argIndex++, width);
	notOverlappedScreenBufferUpdateKernel->setArg(argIndex++, height);
	notOverlappedScreenBufferUpdateKernel->setArg(argIndex++, *ocl_IMAGEPIPELINE);
	notOverlappedScreenBufferUpdateKernel->setArg(argIndex++, *ocl_FRAMEBUFFER_MASK);

	//--------------------------------------------------------------------------

	delete program;

	const double tEnd = WallClockTime();
	SLG_LOG("[MergeSampleBuffersOCL] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void Film::WriteAllOCLBuffers() {
	cl::CommandQueue &oclQueue = oclIntersectionDevice->GetOpenCLQueue();
	// The only channels used at the moment by image pipeline plugins
	oclQueue.enqueueWriteBuffer(*ocl_FRAMEBUFFER_MASK, CL_FALSE, 0, channel_FRAMEBUFFER_MASK->GetSize(), channel_FRAMEBUFFER_MASK->GetPixels());
	if (HasChannel(ALPHA))
		oclQueue.enqueueWriteBuffer(*ocl_ALPHA, CL_FALSE, 0, channel_ALPHA->GetSize(), channel_ALPHA->GetPixels());
	if (HasChannel(OBJECT_ID))
		oclQueue.enqueueWriteBuffer(*ocl_OBJECT_ID, CL_FALSE, 0, channel_OBJECT_ID->GetSize(), channel_OBJECT_ID->GetPixels());
}

void Film::ReadOCLBuffer_IMAGEPIPELINE(const u_int index) {
	cl::CommandQueue &oclQueue = oclIntersectionDevice->GetOpenCLQueue();
	oclQueue.enqueueReadBuffer(*ocl_IMAGEPIPELINE, CL_FALSE, 0, channel_IMAGEPIPELINEs[index]->GetSize(), channel_IMAGEPIPELINEs[index]->GetPixels());
}

void Film::WriteOCLBuffer_IMAGEPIPELINE(const u_int index) {
	cl::CommandQueue &oclQueue = oclIntersectionDevice->GetOpenCLQueue();
	oclQueue.enqueueWriteBuffer(*ocl_IMAGEPIPELINE, CL_FALSE, 0, channel_IMAGEPIPELINEs[index]->GetSize(), channel_IMAGEPIPELINEs[index]->GetPixels());
}

void Film::MergeSampleBuffersOCL(const u_int index) {
	cl::CommandQueue &oclQueue = oclIntersectionDevice->GetOpenCLQueue();

	// Transfer IMAGEPIPELINEs[index] and FRAMEBUFFER_MASK channels
	oclQueue.enqueueWriteBuffer(*ocl_IMAGEPIPELINE, CL_FALSE, 0, channel_IMAGEPIPELINEs[index]->GetSize(), channel_IMAGEPIPELINEs[index]->GetPixels());
	oclQueue.enqueueWriteBuffer(*ocl_FRAMEBUFFER_MASK, CL_FALSE, 0, channel_FRAMEBUFFER_MASK->GetSize(), channel_FRAMEBUFFER_MASK->GetPixels());

	// Clear the FRAMEBUFFER_MASK
	oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*clearFRAMEBUFFER_MASKKernel,
			cl::NullRange, cl::NDRange(RoundUp(pixelCount, 256u)), cl::NDRange(256));

	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			if (radianceChannelScales[i].enabled) {
				// Transfer RADIANCE_PER_PIXEL_NORMALIZEDs[i]
				oclQueue.enqueueWriteBuffer(*ocl_mergeBuffer, CL_FALSE, 0, channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetSize(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixels());

				// Accumulate
				const Spectrum &scale = radianceChannelScales[i].GetScale();
				mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel->setArg(5, scale.c[0]);
				mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel->setArg(6, scale.c[1]);
				mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel->setArg(7, scale.c[2]);

				oclQueue.enqueueNDRangeKernel(*mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel,
						cl::NullRange, cl::NDRange(RoundUp(pixelCount, 256u)), cl::NDRange(256));
			}
		}
	}

	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		const float factor = pixelCount / statsTotalSampleCount;

		for (u_int i = 0; i < radianceGroupCount; ++i) {
			if (radianceChannelScales[i].enabled) {
				// Transfer RADIANCE_PER_SCREEN_NORMALIZEDs[i]
				oclQueue.enqueueWriteBuffer(*ocl_mergeBuffer, CL_FALSE, 0, channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetSize(), channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixels());

				// Accumulate
				const Spectrum scale = factor * radianceChannelScales[i].GetScale();
				mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel->setArg(5, scale.c[0]);
				mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel->setArg(6, scale.c[1]);
				mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel->setArg(7, scale.c[2]);

				oclQueue.enqueueNDRangeKernel(*mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel,
						cl::NullRange, cl::NDRange(RoundUp(pixelCount, 256u)), cl::NDRange(256));
			}
		}
	}

	if (!enabledOverlappedScreenBufferUpdate) {
		oclQueue.enqueueNDRangeKernel(*notOverlappedScreenBufferUpdateKernel,
				cl::NullRange, cl::NDRange(RoundUp(pixelCount, 256u)), cl::NDRange(256));
	}

	// Transfer back the results
	oclQueue.enqueueReadBuffer(*ocl_IMAGEPIPELINE, CL_FALSE, 0, channel_IMAGEPIPELINEs[index]->GetSize(), channel_IMAGEPIPELINEs[index]->GetPixels());
	oclQueue.enqueueReadBuffer(*ocl_FRAMEBUFFER_MASK, CL_FALSE, 0, channel_FRAMEBUFFER_MASK->GetSize(), channel_FRAMEBUFFER_MASK->GetPixels());

	oclQueue.finish();
}

#endif
