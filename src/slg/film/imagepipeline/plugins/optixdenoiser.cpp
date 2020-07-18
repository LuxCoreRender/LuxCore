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

#include "luxrays/utils/cuda.h"

#include "slg/film/imagepipeline/plugins/optixdenoiser.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Optix Denoiser
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::OptixDenoiserPlugin)

OptixDenoiserPlugin::OptixDenoiserPlugin(const float s) : cudaDevice(nullptr),
	denoiserHandle(nullptr), denoiserStateBuff(nullptr), denoiserScratchBuff(nullptr),
	denoiserTmpBuff(nullptr) {
	sharpness = s;
}

OptixDenoiserPlugin::~OptixDenoiserPlugin() {
	if (cudaDevice) {
		if (denoiserHandle)
			CHECK_OPTIX_ERROR(optixDenoiserDestroy(denoiserHandle));

		cudaDevice->FreeBuffer(&denoiserStateBuff);
		cudaDevice->FreeBuffer(&denoiserScratchBuff);
		cudaDevice->FreeBuffer(&denoiserTmpBuff);
	}
}

ImagePipelinePlugin *OptixDenoiserPlugin::Copy() const {
	return new OptixDenoiserPlugin(sharpness);
}

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
			throw std::runtime_error("OptixDenoiserPlugin used while imagepipeline hardware execution is a CUDA device");

		OptixDeviceContext optixContext = cudaDevice->GetOptixContext();

		OptixDenoiserOptions options = {};
		options.inputKind = OPTIX_DENOISER_INPUT_RGB;
		options.pixelFormat = OPTIX_PIXEL_FORMAT_FLOAT3;
		CHECK_OPTIX_ERROR(optixDenoiserCreate(optixContext, &options, &denoiserHandle));

		CHECK_OPTIX_ERROR(optixDenoiserSetModel(denoiserHandle, OPTIX_DENOISER_MODEL_KIND_HDR, nullptr, 0));

		OptixDenoiserSizes sizes = {};
		CHECK_OPTIX_ERROR(optixDenoiserComputeMemoryResources(denoiserHandle,
				film.GetWidth(), film.GetHeight(), &sizes));

		cudaDevice->AllocBufferRW(&denoiserStateBuff, nullptr,
				sizes.stateSizeInBytes,
				"Optix denoiser state buffer");
		cudaDevice->AllocBufferRW(&denoiserScratchBuff, nullptr,
				sizes.recommendedScratchSizeInBytes,
				"Optix denoiser scratch buffer");
		cudaDevice->AllocBufferRW(&denoiserTmpBuff, nullptr,
				3 * sizeof(float) * film.GetWidth() * film.GetHeight(),
				"Optix denoiser temporary buffer");		
		CHECK_OPTIX_ERROR(optixDenoiserSetup(denoiserHandle,
				0,
				film.GetWidth(), film.GetHeight(),
				((CUDADeviceBuffer *)denoiserStateBuff)->GetCUDADevicePointer(),
				sizes.stateSizeInBytes,
				((CUDADeviceBuffer *)denoiserScratchBuff)->GetCUDADevicePointer(),
				sizes.recommendedScratchSizeInBytes));

		film.ctx->SetVerbose(false);
	}

	OptixDenoiserParams params = {};

	OptixImage2D inputLayers[1] = {};
	inputLayers[0].data = ((CUDADeviceBuffer *)film.hw_IMAGEPIPELINE)->GetCUDADevicePointer();
	inputLayers[0].width = film.GetWidth();
	inputLayers[0].height = film.GetHeight();
	inputLayers[0].pixelStrideInBytes = 3 * sizeof(float);
	inputLayers[0].rowStrideInBytes = 3 * sizeof(float) * film.GetWidth();
	inputLayers[0].format = OPTIX_PIXEL_FORMAT_FLOAT3;
	
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
			((CUDADeviceBuffer *)denoiserStateBuff)->GetCUDADevicePointer(),
			denoiserStateBuff->GetSize(),
			inputLayers,
			1,
			0,
			0,
			outputLayers,
			((CUDADeviceBuffer *)denoiserScratchBuff)->GetCUDADevicePointer(),
			denoiserScratchBuff->GetSize()));
	
	// Copy back the result
	CHECK_CUDA_ERROR(cuMemcpyDtoDAsync(inputLayers[0].data, outputLayers[0].data, 3 * sizeof(float) * film.GetWidth() * film.GetHeight(), 0));

	//cudaDevice->FinishQueue();
	//SLG_LOG("OptixDenoiserPlugin execution took a total of " << (boost::format("%.3f") % (WallClockTime() - startTime)) << "secs");
}

#endif
