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

#include "luxrays/devices/ocldevice.h"

#include "slg/film/film.h"
#include "slg/film/imagepipeline/imagepipeline.h"
#include "slg/film/imagepipeline/radiancechannelscale.h"
#include "slg/kernels/kernels.h"
#include "luxrays/devices/cudadevice.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Film HardwareDevice related code
//------------------------------------------------------------------------------

void Film::SetUpHW() {
	hwEnable = true;

	hwDeviceIndex = -1;

	ctx = nullptr;
	dataSet = nullptr;
	hardwareDevice = nullptr;

	hw_IMAGEPIPELINE = nullptr;
	hw_ALPHA = nullptr;
	hw_OBJECT_ID = nullptr;
	hw_ALBEDO = nullptr;
	hw_AVG_SHADING_NORMAL = nullptr;
	hw_mergeBuffer = nullptr;

	mergeInitializeKernel = nullptr;
	mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel = nullptr;
	mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel = nullptr;
	mergeFinalizeKernel = nullptr;
}

void Film::CreateHWContext() {
	SLG_LOG("Film hardware image pipeline");

	// Create LuxRays context
	ctx = new Context(LuxRays_DebugHandler ? LuxRays_DebugHandler : NullDebugHandler,
			Properties() <<
			Property("context.verbose")(false));

	// Select OpenCL device
	vector<DeviceDescription *> descs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_ALL_HARDWARE, descs);

	DeviceDescription *selectedDeviceDesc = nullptr;
	if (hwEnable) {
		if ((hwDeviceIndex >= 0) && (hwDeviceIndex < (int)descs.size())) {
			// I have to use specific device
			selectedDeviceDesc = descs[hwDeviceIndex];
		} else if (descs.size() > 0) {
			// Look for a GPU to use
			for (size_t i = 0; i < descs.size(); ++i) {
				DeviceDescription *desc = descs[i];

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
			// No hardware device device available
		}
	}

	if (selectedDeviceDesc) {
#if !defined(LUXRAYS_DISABLE_CUDA)
		// Force the Optix usage also on no-RTX GPUs for Optix denoiser plugin
		if (selectedDeviceDesc->GetType() == DEVICE_TYPE_CUDA_GPU) {
			CUDADeviceDescription *cudaDeviceDesc = (CUDADeviceDescription *)selectedDeviceDesc;
			cudaDeviceDesc->SetCUDAUseOptix(true);
		}
#endif

		// Allocate the device
		vector<luxrays::DeviceDescription *> selectedDeviceDescs;
		selectedDeviceDescs.push_back(selectedDeviceDesc);
		vector<HardwareDevice *> devs = ctx->AddHardwareDevices(selectedDeviceDescs);
		hardwareDevice = dynamic_cast<HardwareDevice *>(devs[0]);
		assert (hardwareDevice);
		SLG_LOG("Film hardware device used: " << hardwareDevice->GetName() << " (Type: " << DeviceDescription::GetDeviceType(hardwareDevice->GetDeviceDesc()->GetType()) << ")");

		hardwareDevice->PushThreadCurrentDevice();

#if !defined(LUXRAYS_DISABLE_OPENCL)
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
#endif

		if (hardwareDevice->GetDeviceDesc()->GetType() & DEVICE_TYPE_CUDA_ALL) {
			// Suggested compiler options: --use_fast_math
			vector<string> compileOpts;
			compileOpts.push_back("--use_fast_math");

			hardwareDevice->SetAdditionalCompileOpts(compileOpts);
		}

		if (hardwareDevice->GetDeviceDesc()->GetType() & DEVICE_TYPE_OPENCL_ALL) {
			// Suggested compiler options: -cl-fast-relaxed-math -cl-mad-enable

			vector<string> compileOpts;
			compileOpts.push_back("-cl-fast-relaxed-math");
			compileOpts.push_back("-cl-mad-enable");

			hardwareDevice->SetAdditionalCompileOpts(compileOpts);
		}

		// Just an empty data set
		dataSet = new DataSet(ctx);
		dataSet->Preprocess();
		ctx->SetDataSet(dataSet);
		ctx->Start();
		
		hardwareDevice->PopThreadCurrentDevice();
	}
}

void Film::DeleteHWContext() {
	if (hardwareDevice) {
		hardwareDevice->PushThreadCurrentDevice();

		const size_t size = hardwareDevice->GetUsedMemory();
		SLG_LOG("[" << hardwareDevice->GetName() << "] Memory used for hardware image pipeline: " <<
				(size < 10000 ? size : (size / 1024)) << (size < 10000 ? "bytes" : "Kbytes"));

		delete mergeInitializeKernel;
		delete mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel;
		delete mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel;
		delete mergeFinalizeKernel;

		hardwareDevice->FreeBuffer(&hw_IMAGEPIPELINE);
		hardwareDevice->FreeBuffer(&hw_ALPHA);
		hardwareDevice->FreeBuffer(&hw_OBJECT_ID);
		hardwareDevice->FreeBuffer(&hw_ALBEDO);
		hardwareDevice->FreeBuffer(&hw_AVG_SHADING_NORMAL);
		hardwareDevice->FreeBuffer(&hw_mergeBuffer);

		hardwareDevice->PopThreadCurrentDevice();
		hardwareDevice = nullptr;
	}

	delete ctx;
	ctx = nullptr;
	delete dataSet;
	dataSet = nullptr;
}

void Film::AllocateHWBuffers() {
	ctx->SetVerbose(true);
	hardwareDevice->PushThreadCurrentDevice();

	FilmChannels hwChannelsUsed;
	for (auto const ip : imagePipelines)
		ip->AddHWChannelsUsed(hwChannelsUsed);
	
	if (hwChannelsUsed.count(IMAGEPIPELINE))
		hardwareDevice->AllocBufferRW(&hw_IMAGEPIPELINE, channel_IMAGEPIPELINEs[0]->GetPixels(), channel_IMAGEPIPELINEs[0]->GetSize(), "IMAGEPIPELINE");

	if (HasChannel(ALPHA) && hwChannelsUsed.count(ALPHA))
		hardwareDevice->AllocBufferRO(&hw_ALPHA, channel_ALPHA->GetPixels(), channel_ALPHA->GetSize(), "ALPHA");
	if (HasChannel(OBJECT_ID) && hwChannelsUsed.count(OBJECT_ID))
		hardwareDevice->AllocBufferRO(&hw_OBJECT_ID, channel_OBJECT_ID->GetPixels(), channel_OBJECT_ID->GetSize(), "OBJECT_ID");
	if (HasChannel(ALBEDO) && hwChannelsUsed.count(ALBEDO))
		hardwareDevice->AllocBufferRO(&hw_ALBEDO, channel_ALBEDO->GetPixels(), channel_ALBEDO->GetSize(), "ALBEDO");
	if (HasChannel(AVG_SHADING_NORMAL) && hwChannelsUsed.count(AVG_SHADING_NORMAL))
		hardwareDevice->AllocBufferRO(&hw_AVG_SHADING_NORMAL, channel_AVG_SHADING_NORMAL->GetPixels(), channel_AVG_SHADING_NORMAL->GetSize(), "AVG_SHADING_NORMAL");

	const size_t mergeBufferSize = Max(
			HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) ? channel_RADIANCE_PER_PIXEL_NORMALIZEDs[0]->GetSize() : 0,
			HasChannel(RADIANCE_PER_SCREEN_NORMALIZED) ? channel_RADIANCE_PER_SCREEN_NORMALIZEDs[0]->GetSize() : 0);
	if (mergeBufferSize > 0)
		hardwareDevice->AllocBufferRO(&hw_mergeBuffer, nullptr, mergeBufferSize, "Merge");

	hardwareDevice->PopThreadCurrentDevice();
	ctx->SetVerbose(false);
}

void Film::CompileHWKernels() {
	ctx->SetVerbose(true);
	hardwareDevice->PushThreadCurrentDevice();

	// Compile MergeSampleBuffersOCL() kernels
	const double tStart = WallClockTime();

	vector<string> opts;
	opts.push_back("-D LUXRAYS_OPENCL_KERNEL");
	opts.push_back("-D SLG_OPENCL_KERNEL");

	HardwareDeviceProgram *program = nullptr;
	hardwareDevice->CompileProgram(&program,
			opts,
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
	hardwareDevice->SetKernelArg(mergeInitializeKernel, argIndex++, hw_IMAGEPIPELINE);

	//--------------------------------------------------------------------------
	// Film_MergeRADIANCE_PER_PIXEL_NORMALIZED kernel
	//--------------------------------------------------------------------------

	SLG_LOG("[MergeSampleBuffersOCL] Compiling Film_MergeRADIANCE_PER_PIXEL_NORMALIZED Kernel");
	hardwareDevice->GetKernel(program, &mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, "Film_MergeRADIANCE_PER_PIXEL_NORMALIZED");

	// Set kernel arguments
	argIndex = 0;
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, argIndex++, width);
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, argIndex++, height);
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, argIndex++, hw_IMAGEPIPELINE);
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_PIXEL_NORMALIZEDKernel, argIndex++, hw_mergeBuffer);
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
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel, argIndex++, hw_IMAGEPIPELINE);
	hardwareDevice->SetKernelArg(mergeRADIANCE_PER_SCREEN_NORMALIZEDKernel, argIndex++, hw_mergeBuffer);
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
	hardwareDevice->SetKernelArg(mergeFinalizeKernel, argIndex++, hw_IMAGEPIPELINE);

	//--------------------------------------------------------------------------

	delete program;

	const double tEnd = WallClockTime();
	SLG_LOG("[MergeSampleBuffersOCL] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

	hardwareDevice->PopThreadCurrentDevice();
	ctx->SetVerbose(false);
}

void Film::WriteAllHWBuffers() {
	// hardwareDevice->Push/PopThreadCurrentDevice() is done by the caller

	if (HasChannel(ALPHA) && hw_ALPHA)
		hardwareDevice->EnqueueWriteBuffer(hw_ALPHA, false,
				channel_ALPHA->GetSize(),
				channel_ALPHA->GetPixels());
	if (HasChannel(OBJECT_ID) && hw_OBJECT_ID)
		hardwareDevice->EnqueueWriteBuffer(hw_OBJECT_ID, false,
				channel_OBJECT_ID->GetSize(),
				channel_OBJECT_ID->GetPixels());
	if (HasChannel(ALBEDO) && hw_ALBEDO)
		hardwareDevice->EnqueueWriteBuffer(hw_ALBEDO, false,
				channel_ALBEDO->GetSize(),
				channel_ALBEDO->GetPixels());
	if (HasChannel(AVG_SHADING_NORMAL) && hw_AVG_SHADING_NORMAL)
		hardwareDevice->EnqueueWriteBuffer(hw_AVG_SHADING_NORMAL, false,
				channel_AVG_SHADING_NORMAL->GetSize(),
				channel_AVG_SHADING_NORMAL->GetPixels());
}

void Film::ReadHWBuffer_IMAGEPIPELINE(const u_int index) {
	// hardwareDevice->Push/PopThreadCurrentDevice() is done by the caller

	hardwareDevice->EnqueueReadBuffer(hw_IMAGEPIPELINE, false,
			channel_IMAGEPIPELINEs[index]->GetSize(),
			channel_IMAGEPIPELINEs[index]->GetPixels());
}

void Film::WriteHWBuffer_IMAGEPIPELINE(const u_int index) {
	// hardwareDevice->Push/PopThreadCurrentDevice() is done by the caller

	hardwareDevice->EnqueueWriteBuffer(hw_IMAGEPIPELINE, false,
			channel_IMAGEPIPELINEs[index]->GetSize(),
			channel_IMAGEPIPELINEs[index]->GetPixels());
}

void Film::MergeSampleBuffersHW(const u_int imagePipelineIndex) {
	// hardwareDevice->Push/PopThreadCurrentDevice() is done by the caller

	const ImagePipeline *ip = (imagePipelineIndex < imagePipelines.size()) ? imagePipelines[imagePipelineIndex] : nullptr;
	// Transfer IMAGEPIPELINEs[index]
	hardwareDevice->EnqueueWriteBuffer(hw_IMAGEPIPELINE, false,
			channel_IMAGEPIPELINEs[imagePipelineIndex]->GetSize(),
			channel_IMAGEPIPELINEs[imagePipelineIndex]->GetPixels());

	// Initialize the framebuffer
	hardwareDevice->EnqueueKernel(mergeInitializeKernel,
			HardwareDeviceRange(RoundUp(pixelCount, 256u)), HardwareDeviceRange(256));

	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			if (!ip || ip->radianceChannelScales[i].enabled) {
				// Transfer RADIANCE_PER_PIXEL_NORMALIZEDs[i]
				hardwareDevice->EnqueueWriteBuffer(hw_mergeBuffer, false,
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
				hardwareDevice->EnqueueWriteBuffer(hw_mergeBuffer, false,
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
	hardwareDevice->EnqueueReadBuffer(hw_IMAGEPIPELINE, false,
			channel_IMAGEPIPELINEs[imagePipelineIndex]->GetSize(),
			channel_IMAGEPIPELINEs[imagePipelineIndex]->GetPixels());

	hardwareDevice->FinishQueue();
}
