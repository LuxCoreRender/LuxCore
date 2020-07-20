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

OptixDenoiserPlugin::OptixDenoiserPlugin(const float s) : cudaDevice(nullptr),
	denoiserHandle(nullptr), denoiserStateScratchBuff(nullptr),
	denoiserTmpBuff(nullptr), albedoTmpBuff(nullptr), avgShadingNormalTmpBuff(nullptr),
	bufferSetUpKernel(nullptr) {
	sharpness = s;
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
	return new OptixDenoiserPlugin(sharpness);
}

void OptixDenoiserPlugin::AddHWChannelsUsed(unordered_set<Film::FilmChannelType> &hwChannelsUsed) const {
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

	if (!cudaDevice) {
		film.ctx->SetVerbose(true);

		if (!isOptixAvilable)
			throw std::runtime_error("OptixDenoiserPlugin used while Optix is not available");
		if (!film.hardwareDevice)
			throw std::runtime_error("OptixDenoiserPlugin used while imagepipeline hardware execution is not enabled");

		cudaDevice = dynamic_cast<CUDADevice *>(film.hardwareDevice);
		if (!cudaDevice)
			throw std::runtime_error("OptixDenoiserPlugin used while imagepipeline hardware execution isn't on a CUDA device");

		OptixDeviceContext optixContext = cudaDevice->GetOptixContext();

		OptixDenoiserOptions options = {};
		// Use ALBEDO and AVG_SHADING_NORMAL AOVs if they are available
		if (film.HasChannel(Film::ALBEDO)) {
			if (film.HasChannel(Film::AVG_SHADING_NORMAL))
				options.inputKind = OPTIX_DENOISER_INPUT_RGB_ALBEDO_NORMAL;
			else
				options.inputKind = OPTIX_DENOISER_INPUT_RGB_ALBEDO;
		} else
			options.inputKind = OPTIX_DENOISER_INPUT_RGB;
		CHECK_OPTIX_ERROR(optixDenoiserCreate(optixContext, &options, &denoiserHandle));

		CHECK_OPTIX_ERROR(optixDenoiserSetModel(denoiserHandle, OPTIX_DENOISER_MODEL_KIND_HDR, nullptr, 0));

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

	OptixImage2D inputLayers[3] = {};
	u_int layersCount = 1;
	inputLayers[0].data = ((CUDADeviceBuffer *)film.hw_IMAGEPIPELINE)->GetCUDADevicePointer();
	inputLayers[0].width = film.GetWidth();
	inputLayers[0].height = film.GetHeight();
	inputLayers[0].pixelStrideInBytes = 3 * sizeof(float);
	inputLayers[0].rowStrideInBytes = 3 * sizeof(float) * film.GetWidth();
	inputLayers[0].format = OPTIX_PIXEL_FORMAT_FLOAT3;
	
	// Use ALBEDO and AVG_SHADING_NORMAL AOVs if they are available
	
	if (film.HasChannel(Film::ALBEDO)) {
		inputLayers[1].data = ((CUDADeviceBuffer *)albedoTmpBuff)->GetCUDADevicePointer();
		inputLayers[1].width = film.GetWidth();
		inputLayers[1].height = film.GetHeight();
		inputLayers[1].pixelStrideInBytes = 3 * sizeof(float);
		inputLayers[1].rowStrideInBytes = 3 * sizeof(float) * film.GetWidth();
		inputLayers[1].format = OPTIX_PIXEL_FORMAT_FLOAT3;
		layersCount = 2;

		// Setup albedoTmpBuff
		u_int argIndex = 0;
		film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, film.GetWidth());
		film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, film.GetHeight());
		film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, film.hw_ALBEDO);
		film.hardwareDevice->SetKernelArg(bufferSetUpKernel, argIndex++, albedoTmpBuff);
		
		cudaDevice->EnqueueKernel(bufferSetUpKernel, HardwareDeviceRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)),
			HardwareDeviceRange(256));
		
		if (film.HasChannel(Film::AVG_SHADING_NORMAL)) {
			inputLayers[2].data = ((CUDADeviceBuffer *)avgShadingNormalTmpBuff)->GetCUDADevicePointer();
			inputLayers[2].width = film.GetWidth();
			inputLayers[2].height = film.GetHeight();
			inputLayers[2].pixelStrideInBytes = 3 * sizeof(float);
			inputLayers[2].rowStrideInBytes = 3 * sizeof(float) * film.GetWidth();
			inputLayers[2].format = OPTIX_PIXEL_FORMAT_FLOAT3;
			layersCount = 3;
			
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
	
	OptixImage2D outputLayers[1] = {};
	outputLayers[0].data = ((CUDADeviceBuffer *)denoiserTmpBuff)->GetCUDADevicePointer();
	outputLayers[0].width = film.GetWidth();
	outputLayers[0].height = film.GetHeight();
	outputLayers[0].pixelStrideInBytes = 3 * sizeof(float);
	outputLayers[0].rowStrideInBytes = 3 * sizeof(float) * film.GetWidth();
	outputLayers[0].format = OPTIX_PIXEL_FORMAT_FLOAT3;

	// Run the denoiser
	CHECK_OPTIX_ERROR(optixDenoiserInvoke(denoiserHandle,
			0,
			&params,
			((CUDADeviceBuffer *)denoiserStateScratchBuff)->GetCUDADevicePointer(),
			denoiserSizes.stateSizeInBytes,
			inputLayers,
			layersCount,
			0,
			0,
			outputLayers,
			((CUDADeviceBuffer *)denoiserStateScratchBuff)->GetCUDADevicePointer() + denoiserSizes.stateSizeInBytes,
			denoiserSizes.withOverlapScratchSizeInBytes));
	
	// Copy back the result
	CHECK_CUDA_ERROR(cuMemcpyDtoDAsync(inputLayers[0].data, outputLayers[0].data, 3 * sizeof(float) * film.GetWidth() * film.GetHeight(), 0));

	//cudaDevice->FinishQueue();
	//SLG_LOG("OptixDenoiserPlugin execution took a total of " << (boost::format("%.3f") % (WallClockTime() - startTime)) << "secs");
}

#endif
