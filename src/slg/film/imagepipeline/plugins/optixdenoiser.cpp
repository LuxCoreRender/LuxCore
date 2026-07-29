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

#if !defined(LUXRAYS_DISABLE_CUDA)

#include <boost/format.hpp>

#include "luxrays/kernels/kernels.h"
#include "luxrays/utils/cuda.h"

#include "slg/kernels/kernels.h"
#include "slg/film/imagepipeline/plugins/optixdenoiser.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Optix Denoiser
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::OptixDenoiserPlugin)

OptixDenoiserPlugin::OptixDenoiserPlugin(const float s, const u_int minSPP) : sharpness(s),
	minSPP(minSPP), cudaDevice(nullptr),
	denoiserHandle(nullptr), denoiserStateScratchBuff(nullptr),
	denoiserTmpBuff(nullptr), albedoTmpBuff(nullptr), avgShadingNormalTmpBuff(nullptr),
	bufferSetUpKernel(nullptr) {
}

OptixDenoiserPlugin::~OptixDenoiserPlugin() {
	if (cudaDevice) {
		if (denoiserHandle)
			CHECK_OPTIX_ERROR(optixDenoiserDestroy(denoiserHandle));

		delete bufferSetUpKernel;
		cudaDevice->FreeBuffer(&denoiserStateScratchBuff);
		cudaDevice->FreeBuffer(&denoiserTmpBuff);
		cudaDevice->FreeBuffer(&albedoTmpBuff);
		cudaDevice->FreeBuffer(&avgShadingNormalTmpBuff);
	}
}

ImagePipelinePlugin *OptixDenoiserPlugin::Copy() const {
	return new OptixDenoiserPlugin(sharpness, minSPP);
}

void OptixDenoiserPlugin::AddHWChannelsUsed(unordered_set<Film::FilmChannelType, hash<int> > &hwChannelsUsed) const {
	hwChannelsUsed.insert(Film::IMAGEPIPELINE);
	hwChannelsUsed.insert(Film::ALBEDO);
	hwChannelsUsed.insert(Film::AVG_SHADING_NORMAL);
}

//------------------------------------------------------------------------------
// CUDADevice version
//------------------------------------------------------------------------------

void OptixDenoiserPlugin::ApplyHW(Film &film, const u_int index) {
	//const double startTime = WallClockTime();
	//SLG_LOG("[OptixDenoiserPlugin] Applying Optix denoiser");
	
	if (minSPP > 0) {
		const u_int *subRegion = film.GetSubRegion();
		const u_int regionPixelsCount = (subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1);
		const double spp = film.GetTotalSampleCount() / regionPixelsCount;
		
		if (spp < minSPP) {
			return;
		}
	}

	if (!cudaDevice) {
		film.ctx->SetVerbose(true);

		if (!isOptixAvilable)
			throw runtime_error("OptixDenoiserPlugin used while Optix is not available");
		if (!film.hardwareDevice)
			throw runtime_error("OptixDenoiserPlugin used while imagepipeline hardware execution is not enabled");

		cudaDevice = dynamic_cast<CUDADevice *>(film.hardwareDevice);
		if (!cudaDevice)
			throw runtime_error("OptixDenoiserPlugin used while imagepipeline hardware execution isn't on a CUDA device");

		OptixDeviceContext optixContext = cudaDevice->GetOptixContext();
		if (!optixContext)
			throw runtime_error("OptixDenoiserPlugin used on device where Optix is not available");

		OptixDenoiserOptions options = {};
		// Enable the guide layers provided to OptiX 9.
		options.guideAlbedo = film.HasChannel(Film::ALBEDO);
		options.guideNormal = options.guideAlbedo &&
				film.HasChannel(Film::AVG_SHADING_NORMAL);
		CHECK_OPTIX_ERROR(optixDenoiserCreate(optixContext,
				OPTIX_DENOISER_MODEL_KIND_HDR, &options, &denoiserHandle));

		CHECK_OPTIX_ERROR(optixDenoiserComputeMemoryResources(denoiserHandle,
				film.GetWidth(), film.GetHeight(), &denoiserSizes));

		cudaDevice->AllocBufferRW(&denoiserStateScratchBuff, nullptr,
				denoiserSizes.stateSizeInBytes + denoiserSizes.withOverlapScratchSizeInBytes,
				"Optix denoiser state and scratch buffer");
		cudaDevice->AllocBufferRW(&denoiserTmpBuff, nullptr,
				3 * sizeof(float) * film.GetWidth() * film.GetHeight(),
				"Optix denoiser temporary buffer");		
		if (film.HasChannel(Film::ALBEDO)) {
			// Allocate ALBEDO and AVG_SHADING_NORMAL temporary buffers

			cudaDevice->AllocBufferRW(&albedoTmpBuff, nullptr,
					3 * sizeof(float) * film.GetWidth() * film.GetHeight(),
					"Optix denoiser albedo temporary buffer");
			if (film.HasChannel(Film::AVG_SHADING_NORMAL))
				cudaDevice->AllocBufferRW(&avgShadingNormalTmpBuff, nullptr,
						3 * sizeof(float) * film.GetWidth() * film.GetHeight(),
						"Optix denoiser normal temporary buffer");
			
			// Compile buffer setup kernel

			vector<string> opts;
			opts.push_back("-D LUXRAYS_OPENCL_KERNEL");
			opts.push_back("-D SLG_OPENCL_KERNEL");

			HardwareDeviceProgram *program = nullptr;
			cudaDevice->CompileProgram(&program,
					opts,
					luxrays::ocl::KernelSource_utils_funcs +
					slg::ocl::KernelSource_plugin_optixdenoiser_funcs,
					"OptixDenoiserPlugin");

			SLG_LOG("[OptixDenoiserPlugin] Compiling OptixDenoiserPlugin_BufferSetUp Kernel");
			cudaDevice->GetKernel(program, &bufferSetUpKernel, "OptixDenoiserPlugin_BufferSetUp");

			delete program;
		}

		CHECK_OPTIX_ERROR(optixDenoiserSetup(denoiserHandle,
				0,
				film.GetWidth(), film.GetHeight(),
				((CUDADeviceBuffer *)denoiserStateScratchBuff)->GetCUDADevicePointer(),
				denoiserSizes.stateSizeInBytes,
				((CUDADeviceBuffer *)denoiserStateScratchBuff)->GetCUDADevicePointer() + denoiserSizes.stateSizeInBytes,
				denoiserSizes.withOverlapScratchSizeInBytes));

		film.ctx->SetVerbose(false);
	}

	OptixDenoiserParams params = {};

	OptixDenoiserGuideLayer guideLayer = {};
	OptixDenoiserLayer layers[1] = {};
	layers[0].input.data = ((CUDADeviceBuffer *)film.hw_IMAGEPIPELINE)->GetCUDADevicePointer();
	layers[0].input.width = film.GetWidth();
	layers[0].input.height = film.GetHeight();
	layers[0].input.pixelStrideInBytes = 3 * sizeof(float);
	layers[0].input.rowStrideInBytes = 3 * sizeof(float) * film.GetWidth();
	layers[0].input.format = OPTIX_PIXEL_FORMAT_FLOAT3;
	
	// Use ALBEDO and AVG_SHADING_NORMAL AOVs if they are available
	
	if (film.HasChannel(Film::ALBEDO)) {
		guideLayer.albedo.data = ((CUDADeviceBuffer *)albedoTmpBuff)->GetCUDADevicePointer();
		guideLayer.albedo.width = film.GetWidth();
		guideLayer.albedo.height = film.GetHeight();
		guideLayer.albedo.pixelStrideInBytes = 3 * sizeof(float);
		guideLayer.albedo.rowStrideInBytes = 3 * sizeof(float) * film.GetWidth();
		guideLayer.albedo.format = OPTIX_PIXEL_FORMAT_FLOAT3;

		// Setup albedoTmpBuff
		u_int argIndex = 0;
		film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, film.GetWidth());
		film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, film.GetHeight());
		film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, film.hw_ALBEDO);
		film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, albedoTmpBuff);
		
		cudaDevice->EnqueueKernel(bufferSetUpKernel, HardwareDeviceRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)),
			HardwareDeviceRange(256));
		
		if (film.HasChannel(Film::AVG_SHADING_NORMAL)) {
			guideLayer.normal.data = ((CUDADeviceBuffer *)avgShadingNormalTmpBuff)->GetCUDADevicePointer();
			guideLayer.normal.width = film.GetWidth();
			guideLayer.normal.height = film.GetHeight();
			guideLayer.normal.pixelStrideInBytes = 3 * sizeof(float);
			guideLayer.normal.rowStrideInBytes = 3 * sizeof(float) * film.GetWidth();
			guideLayer.normal.format = OPTIX_PIXEL_FORMAT_FLOAT3;
			
			// Setup albedoTmpBuff
			u_int argIndex = 0;
			film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, film.GetWidth());
			film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, film.GetHeight());
			film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, film.hw_AVG_SHADING_NORMAL);
			film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, avgShadingNormalTmpBuff);

			cudaDevice->EnqueueKernel(bufferSetUpKernel, HardwareDeviceRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)),
				HardwareDeviceRange(256));
		}
	}
	
	layers[0].output.data = ((CUDADeviceBuffer *)denoiserTmpBuff)->GetCUDADevicePointer();
	layers[0].output.width = film.GetWidth();
	layers[0].output.height = film.GetHeight();
	layers[0].output.pixelStrideInBytes = 3 * sizeof(float);
	layers[0].output.rowStrideInBytes = 3 * sizeof(float) * film.GetWidth();
	layers[0].output.format = OPTIX_PIXEL_FORMAT_FLOAT3;

	// Run the denoiser
	CHECK_OPTIX_ERROR(optixDenoiserInvoke(denoiserHandle,
			0,
			&params,
			((CUDADeviceBuffer *)denoiserStateScratchBuff)->GetCUDADevicePointer(),
			denoiserSizes.stateSizeInBytes,
			&guideLayer,
			layers,
			1,
			0,
			0,
			((CUDADeviceBuffer *)denoiserStateScratchBuff)->GetCUDADevicePointer() + denoiserSizes.stateSizeInBytes,
			denoiserSizes.withOverlapScratchSizeInBytes));
	
	// Copy back the result
	CHECK_CUDA_ERROR(cuMemcpyDtoDAsync(layers[0].input.data, layers[0].output.data, 3 * sizeof(float) * film.GetWidth() * film.GetHeight(), 0));

	//cudaDevice->FinishQueue();
	//SLG_LOG("OptixDenoiserPlugin execution took a total of " << (boost::format("%.3f") % (WallClockTime() - startTime)) << "secs");
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
