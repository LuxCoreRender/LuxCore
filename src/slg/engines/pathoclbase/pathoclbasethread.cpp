/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"

#include "luxcore/cfg.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathoclbase/pathoclbase.h"

#if defined(__APPLE__)
//OSX version detection
#include <sys/sysctl.h>
#endif

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ThreadFilm
//------------------------------------------------------------------------------

PathOCLBaseRenderThread::ThreadFilm::ThreadFilm(PathOCLBaseRenderThread *thread) {
	film = NULL;

	// Film buffers
	channel_ALPHA_Buff = NULL;
	channel_DEPTH_Buff = NULL;
	channel_POSITION_Buff = NULL;
	channel_GEOMETRY_NORMAL_Buff = NULL;
	channel_SHADING_NORMAL_Buff = NULL;
	channel_MATERIAL_ID_Buff = NULL;
	channel_DIRECT_DIFFUSE_Buff = NULL;
	channel_DIRECT_GLOSSY_Buff = NULL;
	channel_EMISSION_Buff = NULL;
	channel_INDIRECT_DIFFUSE_Buff = NULL;
	channel_INDIRECT_GLOSSY_Buff = NULL;
	channel_INDIRECT_SPECULAR_Buff = NULL;
	channel_MATERIAL_ID_MASK_Buff = NULL;
	channel_DIRECT_SHADOW_MASK_Buff = NULL;
	channel_INDIRECT_SHADOW_MASK_Buff = NULL;
	channel_UV_Buff = NULL;
	channel_RAYCOUNT_Buff = NULL;
	channel_BY_MATERIAL_ID_Buff = NULL;
	channel_IRRADIANCE_Buff = NULL;
	channel_OBJECT_ID_Buff = NULL;
	channel_OBJECT_ID_MASK_Buff = NULL;
	channel_BY_OBJECT_ID_Buff = NULL;
	channel_SAMPLECOUNT_Buff = NULL;
	channel_CONVERGENCE_Buff = NULL;

	renderThread = thread;
}

PathOCLBaseRenderThread::ThreadFilm::~ThreadFilm() {
	delete film;

	FreeAllOCLBuffers();
}

void PathOCLBaseRenderThread::ThreadFilm::Init(Film *engineFlm,
		const u_int threadFilmWidth, const u_int threadFilmHeight,
		const u_int *threadFilmSubRegion) {
	engineFilm = engineFlm;

	const u_int filmPixelCount = threadFilmWidth * threadFilmHeight;

	// Delete previous allocated Film
	delete film;

	// Allocate the new Film
	film = new Film(threadFilmWidth, threadFilmHeight, threadFilmSubRegion);
	film->CopyDynamicSettings(*engineFilm);
	film->Init();

	//--------------------------------------------------------------------------
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		renderThread->FreeOCLBuffer(&channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]);

	if (film->GetRadianceGroupCount() > 8)
		throw runtime_error("PathOCL supports only up to 8 Radiance Groups");

	channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.resize(film->GetRadianceGroupCount(), NULL);
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i) {
		renderThread->AllocOCLBufferRW(&channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i],
				sizeof(float[4]) * filmPixelCount, "RADIANCE_PER_PIXEL_NORMALIZEDs[" + ToString(i) + "]");
	}
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::ALPHA))
		renderThread->AllocOCLBufferRW(&channel_ALPHA_Buff, sizeof(float[2]) * filmPixelCount, "ALPHA");
	else
		renderThread->FreeOCLBuffer(&channel_ALPHA_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DEPTH))
		renderThread->AllocOCLBufferRW(&channel_DEPTH_Buff, sizeof(float) * filmPixelCount, "DEPTH");
	else
		renderThread->FreeOCLBuffer(&channel_DEPTH_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::POSITION))
		renderThread->AllocOCLBufferRW(&channel_POSITION_Buff, sizeof(float[3]) * filmPixelCount, "POSITION");
	else
		renderThread->FreeOCLBuffer(&channel_POSITION_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::GEOMETRY_NORMAL))
		renderThread->AllocOCLBufferRW(&channel_GEOMETRY_NORMAL_Buff, sizeof(float[3]) * filmPixelCount, "GEOMETRY_NORMAL");
	else
		renderThread->FreeOCLBuffer(&channel_GEOMETRY_NORMAL_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::SHADING_NORMAL))
		renderThread->AllocOCLBufferRW(&channel_SHADING_NORMAL_Buff, sizeof(float[3]) * filmPixelCount, "SHADING_NORMAL");
	else
		renderThread->FreeOCLBuffer(&channel_SHADING_NORMAL_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::MATERIAL_ID))
		renderThread->AllocOCLBufferRW(&channel_MATERIAL_ID_Buff, sizeof(u_int) * filmPixelCount, "MATERIAL_ID");
	else
		renderThread->FreeOCLBuffer(&channel_MATERIAL_ID_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_DIFFUSE))
		renderThread->AllocOCLBufferRW(&channel_DIRECT_DIFFUSE_Buff, sizeof(float[4]) * filmPixelCount, "DIRECT_DIFFUSE");
	else
		renderThread->FreeOCLBuffer(&channel_DIRECT_DIFFUSE_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_GLOSSY))
		renderThread->AllocOCLBufferRW(&channel_DIRECT_GLOSSY_Buff, sizeof(float[4]) * filmPixelCount, "DIRECT_GLOSSY");
	else
		renderThread->FreeOCLBuffer(&channel_DIRECT_GLOSSY_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::EMISSION))
		renderThread->AllocOCLBufferRW(&channel_EMISSION_Buff, sizeof(float[4]) * filmPixelCount, "EMISSION");
	else
		renderThread->FreeOCLBuffer(&channel_EMISSION_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_DIFFUSE))
		renderThread->AllocOCLBufferRW(&channel_INDIRECT_DIFFUSE_Buff, sizeof(float[4]) * filmPixelCount, "INDIRECT_DIFFUSE");
	else
		renderThread->FreeOCLBuffer(&channel_INDIRECT_DIFFUSE_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_GLOSSY))
		renderThread->AllocOCLBufferRW(&channel_INDIRECT_GLOSSY_Buff, sizeof(float[4]) * filmPixelCount, "INDIRECT_GLOSSY");
	else
		renderThread->FreeOCLBuffer(&channel_INDIRECT_GLOSSY_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_SPECULAR))
		renderThread->AllocOCLBufferRW(&channel_INDIRECT_SPECULAR_Buff, sizeof(float[4]) * filmPixelCount, "INDIRECT_SPECULAR");
	else
		renderThread->FreeOCLBuffer(&channel_INDIRECT_SPECULAR_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::MATERIAL_ID_MASK)) {
		if (film->GetMaskMaterialIDCount() > 1)
			throw runtime_error("PathOCL supports only 1 MATERIAL_ID_MASK");
		else
			renderThread->AllocOCLBufferRW(&channel_MATERIAL_ID_MASK_Buff,
					sizeof(float[2]) * filmPixelCount, "MATERIAL_ID_MASK");
	} else
		renderThread->FreeOCLBuffer(&channel_MATERIAL_ID_MASK_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_SHADOW_MASK))
		renderThread->AllocOCLBufferRW(&channel_DIRECT_SHADOW_MASK_Buff, sizeof(float[2]) * filmPixelCount, "DIRECT_SHADOW_MASK");
	else
		renderThread->FreeOCLBuffer(&channel_DIRECT_SHADOW_MASK_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_SHADOW_MASK))
		renderThread->AllocOCLBufferRW(&channel_INDIRECT_SHADOW_MASK_Buff, sizeof(float[2]) * filmPixelCount, "INDIRECT_SHADOW_MASK");
	else
		renderThread->FreeOCLBuffer(&channel_INDIRECT_SHADOW_MASK_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::UV))
		renderThread->AllocOCLBufferRW(&channel_UV_Buff, sizeof(float[2]) * filmPixelCount, "UV");
	else
		renderThread->FreeOCLBuffer(&channel_UV_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::RAYCOUNT))
		renderThread->AllocOCLBufferRW(&channel_RAYCOUNT_Buff, sizeof(float) * filmPixelCount, "RAYCOUNT");
	else
		renderThread->FreeOCLBuffer(&channel_RAYCOUNT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::BY_MATERIAL_ID)) {
		if (film->GetByMaterialIDCount() > 1)
			throw runtime_error("PathOCL supports only 1 BY_MATERIAL_ID");
		else
			renderThread->AllocOCLBufferRW(&channel_BY_MATERIAL_ID_Buff,
					sizeof(float[4]) * filmPixelCount, "BY_MATERIAL_ID");
	} else
		renderThread->FreeOCLBuffer(&channel_BY_MATERIAL_ID_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::IRRADIANCE))
		renderThread->AllocOCLBufferRW(&channel_IRRADIANCE_Buff, sizeof(float[4]) * filmPixelCount, "IRRADIANCE");
	else
		renderThread->FreeOCLBuffer(&channel_IRRADIANCE_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::OBJECT_ID))
		renderThread->AllocOCLBufferRW(&channel_OBJECT_ID_Buff, sizeof(u_int) * filmPixelCount, "OBJECT_ID");
	else
		renderThread->FreeOCLBuffer(&channel_OBJECT_ID_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::OBJECT_ID_MASK)) {
		if (film->GetMaskMaterialIDCount() > 1)
			throw runtime_error("PathOCL supports only 1 OBJECT_ID_MASK");
		else
			renderThread->AllocOCLBufferRW(&channel_OBJECT_ID_MASK_Buff,
					sizeof(float[2]) * filmPixelCount, "OBJECT_ID_MASK");
	} else
		renderThread->FreeOCLBuffer(&channel_OBJECT_ID_MASK_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::BY_OBJECT_ID)) {
		if (film->GetByMaterialIDCount() > 1)
			throw runtime_error("PathOCL supports only 1 BY_OBJECT_ID");
		else
			renderThread->AllocOCLBufferRW(&channel_BY_OBJECT_ID_Buff,
					sizeof(float[4]) * filmPixelCount, "BY_OBJECT_ID");
	} else
		renderThread->FreeOCLBuffer(&channel_BY_OBJECT_ID_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::SAMPLECOUNT))
		renderThread->AllocOCLBufferRW(&channel_SAMPLECOUNT_Buff, sizeof(u_int) * filmPixelCount, "SAMPLECOUNT");
	else
		renderThread->FreeOCLBuffer(&channel_SAMPLECOUNT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::CONVERGENCE))
		renderThread->AllocOCLBufferRW(&channel_CONVERGENCE_Buff, sizeof(float) * filmPixelCount, "CONVERGENCE");
	else
		renderThread->FreeOCLBuffer(&channel_CONVERGENCE_Buff);
}

void PathOCLBaseRenderThread::ThreadFilm::FreeAllOCLBuffers() {
	// Film buffers
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		renderThread->FreeOCLBuffer(&channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]);
	channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.clear();
	renderThread->FreeOCLBuffer(&channel_ALPHA_Buff);
	renderThread->FreeOCLBuffer(&channel_DEPTH_Buff);
	renderThread->FreeOCLBuffer(&channel_POSITION_Buff);
	renderThread->FreeOCLBuffer(&channel_GEOMETRY_NORMAL_Buff);
	renderThread->FreeOCLBuffer(&channel_SHADING_NORMAL_Buff);
	renderThread->FreeOCLBuffer(&channel_MATERIAL_ID_Buff);
	renderThread->FreeOCLBuffer(&channel_DIRECT_DIFFUSE_Buff);
	renderThread->FreeOCLBuffer(&channel_DIRECT_GLOSSY_Buff);
	renderThread->FreeOCLBuffer(&channel_EMISSION_Buff);
	renderThread->FreeOCLBuffer(&channel_INDIRECT_DIFFUSE_Buff);
	renderThread->FreeOCLBuffer(&channel_INDIRECT_GLOSSY_Buff);
	renderThread->FreeOCLBuffer(&channel_INDIRECT_SPECULAR_Buff);
	renderThread->FreeOCLBuffer(&channel_MATERIAL_ID_MASK_Buff);
	renderThread->FreeOCLBuffer(&channel_DIRECT_SHADOW_MASK_Buff);
	renderThread->FreeOCLBuffer(&channel_INDIRECT_SHADOW_MASK_Buff);
	renderThread->FreeOCLBuffer(&channel_UV_Buff);
	renderThread->FreeOCLBuffer(&channel_RAYCOUNT_Buff);
	renderThread->FreeOCLBuffer(&channel_BY_MATERIAL_ID_Buff);
	renderThread->FreeOCLBuffer(&channel_IRRADIANCE_Buff);
	renderThread->FreeOCLBuffer(&channel_OBJECT_ID_Buff);
	renderThread->FreeOCLBuffer(&channel_OBJECT_ID_MASK_Buff);
	renderThread->FreeOCLBuffer(&channel_BY_OBJECT_ID_Buff);
	renderThread->FreeOCLBuffer(&channel_SAMPLECOUNT_Buff);
	renderThread->FreeOCLBuffer(&channel_CONVERGENCE_Buff);
}

u_int PathOCLBaseRenderThread::ThreadFilm::SetFilmKernelArgs(cl::Kernel &filmClearKernel,
		u_int argIndex) const {
	// Film parameters
	filmClearKernel.setArg(argIndex++, film->GetWidth());
	filmClearKernel.setArg(argIndex++, film->GetHeight());

	const u_int *filmSubRegion = film->GetSubRegion();
	filmClearKernel.setArg(argIndex++, filmSubRegion[0]);
	filmClearKernel.setArg(argIndex++, filmSubRegion[1]);
	filmClearKernel.setArg(argIndex++, filmSubRegion[2]);
	filmClearKernel.setArg(argIndex++, filmSubRegion[3]);

	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]);
	if (film->HasChannel(Film::ALPHA))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_ALPHA_Buff);
	if (film->HasChannel(Film::DEPTH))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_DEPTH_Buff);
	if (film->HasChannel(Film::POSITION))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_POSITION_Buff);
	if (film->HasChannel(Film::GEOMETRY_NORMAL))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_GEOMETRY_NORMAL_Buff);
	if (film->HasChannel(Film::SHADING_NORMAL))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_SHADING_NORMAL_Buff);
	if (film->HasChannel(Film::MATERIAL_ID))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_MATERIAL_ID_Buff);
	if (film->HasChannel(Film::DIRECT_DIFFUSE))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_DIRECT_DIFFUSE_Buff);
	if (film->HasChannel(Film::DIRECT_GLOSSY))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_DIRECT_GLOSSY_Buff);
	if (film->HasChannel(Film::EMISSION))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_EMISSION_Buff);
	if (film->HasChannel(Film::INDIRECT_DIFFUSE))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_INDIRECT_DIFFUSE_Buff);
	if (film->HasChannel(Film::INDIRECT_GLOSSY))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_INDIRECT_GLOSSY_Buff);
	if (film->HasChannel(Film::INDIRECT_SPECULAR))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_INDIRECT_SPECULAR_Buff);
	if (film->HasChannel(Film::MATERIAL_ID_MASK))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_MATERIAL_ID_MASK_Buff);
	if (film->HasChannel(Film::DIRECT_SHADOW_MASK))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_DIRECT_SHADOW_MASK_Buff);
	if (film->HasChannel(Film::INDIRECT_SHADOW_MASK))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_INDIRECT_SHADOW_MASK_Buff);
	if (film->HasChannel(Film::UV))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_UV_Buff);
	if (film->HasChannel(Film::RAYCOUNT))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_RAYCOUNT_Buff);
	if (film->HasChannel(Film::BY_MATERIAL_ID))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_BY_MATERIAL_ID_Buff);
	if (film->HasChannel(Film::IRRADIANCE))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_IRRADIANCE_Buff);
	if (film->HasChannel(Film::OBJECT_ID))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_OBJECT_ID_Buff);
	if (film->HasChannel(Film::OBJECT_ID_MASK))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_OBJECT_ID_MASK_Buff);
	if (film->HasChannel(Film::BY_OBJECT_ID))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_BY_OBJECT_ID_Buff);
	if (film->HasChannel(Film::SAMPLECOUNT))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_SAMPLECOUNT_Buff);
	if (film->HasChannel(Film::CONVERGENCE))
		filmClearKernel.setArg(argIndex++, sizeof(cl::Buffer), channel_CONVERGENCE_Buff);

	return argIndex;
}

void PathOCLBaseRenderThread::ThreadFilm::RecvFilm(cl::CommandQueue &oclQueue) {
	// Async. transfer of the Film buffers

	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i) {
		if (channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]) {
			oclQueue.enqueueReadBuffer(
				*(channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]),
				CL_FALSE,
				0,
				channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]->getInfo<CL_MEM_SIZE>(),
				film->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixels());
		}
	}

	if (channel_ALPHA_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_ALPHA_Buff,
			CL_FALSE,
			0,
			channel_ALPHA_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_ALPHA->GetPixels());
	}
	if (channel_DEPTH_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DEPTH_Buff,
			CL_FALSE,
			0,
			channel_DEPTH_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DEPTH->GetPixels());
	}
	if (channel_POSITION_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_POSITION_Buff,
			CL_FALSE,
			0,
			channel_POSITION_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_POSITION->GetPixels());
	}
	if (channel_GEOMETRY_NORMAL_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_GEOMETRY_NORMAL_Buff,
			CL_FALSE,
			0,
			channel_GEOMETRY_NORMAL_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_GEOMETRY_NORMAL->GetPixels());
	}
	if (channel_SHADING_NORMAL_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_SHADING_NORMAL_Buff,
			CL_FALSE,
			0,
			channel_SHADING_NORMAL_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_SHADING_NORMAL->GetPixels());
	}
	if (channel_MATERIAL_ID_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_MATERIAL_ID_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_MATERIAL_ID->GetPixels());
	}
	if (channel_DIRECT_DIFFUSE_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DIRECT_DIFFUSE_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_DIFFUSE_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DIRECT_DIFFUSE->GetPixels());
	}
	if (channel_MATERIAL_ID_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_MATERIAL_ID_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_MATERIAL_ID->GetPixels());
	}
	if (channel_DIRECT_GLOSSY_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DIRECT_GLOSSY_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_GLOSSY_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DIRECT_GLOSSY->GetPixels());
	}
	if (channel_EMISSION_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_EMISSION_Buff,
			CL_FALSE,
			0,
			channel_EMISSION_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_EMISSION->GetPixels());
	}
	if (channel_INDIRECT_DIFFUSE_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_DIFFUSE_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_DIFFUSE_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_DIFFUSE->GetPixels());
	}
	if (channel_INDIRECT_GLOSSY_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_GLOSSY_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_GLOSSY_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_GLOSSY->GetPixels());
	}
	if (channel_INDIRECT_SPECULAR_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_SPECULAR_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_SPECULAR_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_SPECULAR->GetPixels());
	}
	if (channel_MATERIAL_ID_MASK_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_MATERIAL_ID_MASK_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_MATERIAL_ID_MASKs[0]->GetPixels());
	}
	if (channel_DIRECT_SHADOW_MASK_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_DIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_SHADOW_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_INDIRECT_SHADOW_MASK_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_INDIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_SHADOW_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_UV_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_UV_Buff,
			CL_FALSE,
			0,
			channel_UV_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_UV->GetPixels());
	}
	if (channel_RAYCOUNT_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_RAYCOUNT_Buff,
			CL_FALSE,
			0,
			channel_RAYCOUNT_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_RAYCOUNT->GetPixels());
	}
	if (channel_BY_MATERIAL_ID_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_BY_MATERIAL_ID_Buff,
			CL_FALSE,
			0,
			channel_BY_MATERIAL_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_BY_MATERIAL_IDs[0]->GetPixels());
	}
	if (channel_IRRADIANCE_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_IRRADIANCE_Buff,
			CL_FALSE,
			0,
			channel_IRRADIANCE_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_IRRADIANCE->GetPixels());
	}
	if (channel_OBJECT_ID_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_OBJECT_ID_Buff,
			CL_FALSE,
			0,
			channel_OBJECT_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_OBJECT_ID->GetPixels());
	}
	if (channel_OBJECT_ID_MASK_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_OBJECT_ID_MASK_Buff,
			CL_FALSE,
			0,
			channel_OBJECT_ID_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_OBJECT_ID_MASKs[0]->GetPixels());
	}
	if (channel_BY_OBJECT_ID_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_BY_OBJECT_ID_Buff,
			CL_FALSE,
			0,
			channel_BY_OBJECT_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_BY_OBJECT_IDs[0]->GetPixels());
	}
	if (channel_SAMPLECOUNT_Buff) {
		oclQueue.enqueueReadBuffer(
			*channel_SAMPLECOUNT_Buff,
			CL_FALSE,
			0,
			channel_SAMPLECOUNT_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_SAMPLECOUNT->GetPixels());
	}
	if (channel_CONVERGENCE_Buff) {
		// This may look wrong but CONVERGENCE channel is compute by the engine
		// film convergence test on the CPU so I write instead of read (to
		// synchronize the content).
		oclQueue.enqueueWriteBuffer(
			*channel_CONVERGENCE_Buff,
			CL_FALSE,
			0,
			channel_CONVERGENCE_Buff->getInfo<CL_MEM_SIZE>(),
			engineFilm->channel_CONVERGENCE->GetPixels());
	}
}

void PathOCLBaseRenderThread::ThreadFilm::SendFilm(cl::CommandQueue &oclQueue) {
	// Async. transfer of the Film buffers

	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i) {
		if (channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]) {
			oclQueue.enqueueWriteBuffer(
				*(channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]),
				CL_FALSE,
				0,
				channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]->getInfo<CL_MEM_SIZE>(),
				film->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixels());
		}
	}

	if (channel_ALPHA_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_ALPHA_Buff,
			CL_FALSE,
			0,
			channel_ALPHA_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_ALPHA->GetPixels());
	}
	if (channel_DEPTH_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_DEPTH_Buff,
			CL_FALSE,
			0,
			channel_DEPTH_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DEPTH->GetPixels());
	}
	if (channel_POSITION_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_POSITION_Buff,
			CL_FALSE,
			0,
			channel_POSITION_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_POSITION->GetPixels());
	}
	if (channel_GEOMETRY_NORMAL_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_GEOMETRY_NORMAL_Buff,
			CL_FALSE,
			0,
			channel_GEOMETRY_NORMAL_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_GEOMETRY_NORMAL->GetPixels());
	}
	if (channel_SHADING_NORMAL_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_SHADING_NORMAL_Buff,
			CL_FALSE,
			0,
			channel_SHADING_NORMAL_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_SHADING_NORMAL->GetPixels());
	}
	if (channel_MATERIAL_ID_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_MATERIAL_ID_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_MATERIAL_ID->GetPixels());
	}
	if (channel_DIRECT_DIFFUSE_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_DIRECT_DIFFUSE_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_DIFFUSE_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DIRECT_DIFFUSE->GetPixels());
	}
	if (channel_MATERIAL_ID_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_MATERIAL_ID_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_MATERIAL_ID->GetPixels());
	}
	if (channel_DIRECT_GLOSSY_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_DIRECT_GLOSSY_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_GLOSSY_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DIRECT_GLOSSY->GetPixels());
	}
	if (channel_EMISSION_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_EMISSION_Buff,
			CL_FALSE,
			0,
			channel_EMISSION_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_EMISSION->GetPixels());
	}
	if (channel_INDIRECT_DIFFUSE_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_INDIRECT_DIFFUSE_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_DIFFUSE_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_DIFFUSE->GetPixels());
	}
	if (channel_INDIRECT_GLOSSY_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_INDIRECT_GLOSSY_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_GLOSSY_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_GLOSSY->GetPixels());
	}
	if (channel_INDIRECT_SPECULAR_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_INDIRECT_SPECULAR_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_SPECULAR_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_SPECULAR->GetPixels());
	}
	if (channel_MATERIAL_ID_MASK_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_MATERIAL_ID_MASK_Buff,
			CL_FALSE,
			0,
			channel_MATERIAL_ID_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_MATERIAL_ID_MASKs[0]->GetPixels());
	}
	if (channel_DIRECT_SHADOW_MASK_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_DIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			0,
			channel_DIRECT_SHADOW_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_DIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_INDIRECT_SHADOW_MASK_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_INDIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			0,
			channel_INDIRECT_SHADOW_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_INDIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_UV_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_UV_Buff,
			CL_FALSE,
			0,
			channel_UV_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_UV->GetPixels());
	}
	if (channel_RAYCOUNT_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_RAYCOUNT_Buff,
			CL_FALSE,
			0,
			channel_RAYCOUNT_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_RAYCOUNT->GetPixels());
	}
	if (channel_BY_MATERIAL_ID_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_BY_MATERIAL_ID_Buff,
			CL_FALSE,
			0,
			channel_BY_MATERIAL_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_BY_MATERIAL_IDs[0]->GetPixels());
	}
	if (channel_IRRADIANCE_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_IRRADIANCE_Buff,
			CL_FALSE,
			0,
			channel_IRRADIANCE_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_IRRADIANCE->GetPixels());
	}
	if (channel_OBJECT_ID_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_OBJECT_ID_Buff,
			CL_FALSE,
			0,
			channel_OBJECT_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_OBJECT_ID->GetPixels());
	}
	if (channel_OBJECT_ID_MASK_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_OBJECT_ID_MASK_Buff,
			CL_FALSE,
			0,
			channel_OBJECT_ID_MASK_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_OBJECT_ID_MASKs[0]->GetPixels());
	}
	if (channel_BY_OBJECT_ID_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_BY_OBJECT_ID_Buff,
			CL_FALSE,
			0,
			channel_BY_OBJECT_ID_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_BY_OBJECT_IDs[0]->GetPixels());
	}
	if (channel_SAMPLECOUNT_Buff) {
		oclQueue.enqueueWriteBuffer(
			*channel_SAMPLECOUNT_Buff,
			CL_FALSE,
			0,
			channel_SAMPLECOUNT_Buff->getInfo<CL_MEM_SIZE>(),
			film->channel_SAMPLECOUNT->GetPixels());
	}
	if (channel_CONVERGENCE_Buff) {
		// The CONVERGENCE channel is compute by the engine
		// film convergence test on the CPU so I write the engine film, not the
		// thread film.
		oclQueue.enqueueWriteBuffer(
			*channel_CONVERGENCE_Buff,
			CL_FALSE,
			0,
			channel_CONVERGENCE_Buff->getInfo<CL_MEM_SIZE>(),
			engineFilm->channel_CONVERGENCE->GetPixels());
	}
}

void PathOCLBaseRenderThread::ThreadFilm::ClearFilm(cl::CommandQueue &oclQueue,
		cl::Kernel &filmClearKernel, const size_t filmClearWorkGroupSize) {
	// Set kernel arguments
	SetFilmKernelArgs(filmClearKernel, 0);
	
	// Clear the film
	const u_int filmPixelCount = film->GetWidth() * film->GetHeight();
	oclQueue.enqueueNDRangeKernel(filmClearKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
			cl::NDRange(filmClearWorkGroupSize));
}

//------------------------------------------------------------------------------
// PathOCLBaseRenderThread
//------------------------------------------------------------------------------

PathOCLBaseRenderThread::PathOCLBaseRenderThread(const u_int index,
		OpenCLIntersectionDevice *device, PathOCLBaseRenderEngine *re) {
	intersectionDevice = device;

	renderThread = NULL;

	threadIndex = index;
	renderEngine = re;
	started = false;
	editMode = false;

	kernelSrcHash = "";
	filmClearKernel = NULL;

	// Scene buffers
	materialsBuff = NULL;
	texturesBuff = NULL;
	meshDescsBuff = NULL;
	scnObjsBuff = NULL;
	lightsBuff = NULL;
	envLightIndicesBuff = NULL;
	lightsDistributionBuff = NULL;
	infiniteLightSourcesDistributionBuff = NULL;
	infiniteLightDistributionsBuff = NULL;
	vertsBuff = NULL;
	normalsBuff = NULL;
	uvsBuff = NULL;
	colsBuff = NULL;
	alphasBuff = NULL;
	trianglesBuff = NULL;
	cameraBuff = NULL;
	triLightDefsBuff = NULL;
	meshTriLightDefsOffsetBuff = NULL;
	imageMapDescsBuff = NULL;

	// Check the kind of kernel cache to use
	string type = renderEngine->renderConfig->cfg.Get(Property("opencl.kernelcache")("PERSISTENT")).Get<string>();
	if (type == "PERSISTENT")
		kernelCache = new oclKernelPersistentCache("LUXCORE_" LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR);
	else if (type == "VOLATILE")
		kernelCache = new oclKernelVolatileCache();
	else if (type == "NONE")
		kernelCache = new oclKernelDummyCache();
	else
		throw runtime_error("Unknown opencl.kernelcache type: " + type);
}

PathOCLBaseRenderThread::~PathOCLBaseRenderThread() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();

	FreeThreadFilms();

	delete filmClearKernel;
	delete kernelCache;
}

size_t PathOCLBaseRenderThread::GetOpenCLHitPointSize() const {
	// HitPoint memory size
	size_t hitPointSize = sizeof(Vector) + sizeof(Point) + sizeof(UV) +
			2 * sizeof(Normal) + sizeof(Matrix4x4);
	if (renderEngine->compiledScene->IsTextureCompiled(HITPOINTCOLOR) ||
			renderEngine->compiledScene->IsTextureCompiled(HITPOINTGREY) ||
			renderEngine->compiledScene->hasTriangleLightWithVertexColors)
		hitPointSize += sizeof(Spectrum);
	if (renderEngine->compiledScene->IsTextureCompiled(HITPOINTALPHA))
		hitPointSize += sizeof(float);
	if (renderEngine->compiledScene->RequiresPassThrough())
		hitPointSize += sizeof(float);
	// Fields dpdu, dpdv, dndu, dndv
	if (renderEngine->compiledScene->HasBumpMaps())
		hitPointSize += 2 * sizeof(Vector) + 2 * sizeof(Normal);
	// Volume fields
	if (renderEngine->compiledScene->HasVolumes())
		hitPointSize += 2 * sizeof(u_int) + 2 * sizeof(u_int) +
				sizeof(int);

	return hitPointSize;
}

size_t PathOCLBaseRenderThread::GetOpenCLBSDFSize() const {
	// Add BSDF memory size
	size_t bsdfSize = GetOpenCLHitPointSize();
	// Add BSDF.materialIndex memory size
	bsdfSize += sizeof(u_int);
	// Add BSDF.sceneObjectIndex memory size
	bsdfSize += sizeof(u_int);
	// Add BSDF.triangleLightSourceIndex memory size
	bsdfSize += sizeof(u_int);
	// Add BSDF.Frame memory size
	bsdfSize += sizeof(slg::ocl::Frame);
	// Add BSDF.isVolume memory size
	if (renderEngine->compiledScene->HasVolumes())
		bsdfSize += sizeof(int);
	// Add BSDF.isShadowCatcher
	bsdfSize += sizeof(int);

	return bsdfSize;
}

size_t PathOCLBaseRenderThread::GetOpenCLSampleResultSize() const {
	//--------------------------------------------------------------------------
	// SampleResult size
	//--------------------------------------------------------------------------

	// All thread films are supposed to have the same parameters
	const Film *threadFilm = threadFilms[0]->film;

	// SampleResult.filmX and SampleResult.filmY
	size_t sampleResultSize = 2 * sizeof(float);
	// SampleResult.radiancePerPixelNormalized[PARAM_FILM_RADIANCE_GROUP_COUNT]
	sampleResultSize += sizeof(slg::ocl::Spectrum) * threadFilm->GetRadianceGroupCount();
	if (threadFilm->HasChannel(Film::ALPHA))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::DEPTH))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::POSITION))
		sampleResultSize += sizeof(Point);
	if (threadFilm->HasChannel(Film::GEOMETRY_NORMAL))
		sampleResultSize += sizeof(Normal);
	if (threadFilm->HasChannel(Film::SHADING_NORMAL))
		sampleResultSize += sizeof(Normal);
	if (threadFilm->HasChannel(Film::MATERIAL_ID))
		sampleResultSize += sizeof(u_int);
	if (threadFilm->HasChannel(Film::OBJECT_ID))
		sampleResultSize += sizeof(u_int);
	if (threadFilm->HasChannel(Film::DIRECT_DIFFUSE))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::DIRECT_GLOSSY))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::EMISSION))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::INDIRECT_DIFFUSE))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::INDIRECT_GLOSSY))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::INDIRECT_SPECULAR))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::DIRECT_SHADOW_MASK))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::INDIRECT_SHADOW_MASK))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::UV))
		sampleResultSize += sizeof(UV);
	if (threadFilm->HasChannel(Film::RAYCOUNT))
		sampleResultSize += sizeof(Film::RAYCOUNT);
	if (threadFilm->HasChannel(Film::IRRADIANCE))
		sampleResultSize += 2 * sizeof(Spectrum);

	sampleResultSize += sizeof(BSDFEvent) +
			3 * sizeof(int) +
			// pixelX and pixelY fields
			sizeof(u_int) * 2;

	return sampleResultSize;
}

void PathOCLBaseRenderThread::AllocOCLBuffer(const cl_mem_flags clFlags, cl::Buffer **buff,
		void *src, const size_t size, const string &desc) {
	intersectionDevice->AllocBuffer(clFlags, buff, src, size, desc);
}

void PathOCLBaseRenderThread::AllocOCLBufferRO(cl::Buffer **buff, void *src, const size_t size, const string &desc) {
	intersectionDevice->AllocBufferRO(buff, src, size, desc);
}

void PathOCLBaseRenderThread::AllocOCLBufferRW(cl::Buffer **buff, const size_t size, const string &desc) {
	intersectionDevice->AllocBufferRW(buff, size, desc);
}

void PathOCLBaseRenderThread::FreeOCLBuffer(cl::Buffer **buff) {
	intersectionDevice->FreeBuffer(buff);
}

void PathOCLBaseRenderThread::InitFilm() {
	if (threadFilms.size() == 0)
		IncThreadFilms();

	u_int threadFilmWidth, threadFilmHeight, threadFilmSubRegion[4];
	GetThreadFilmSize(&threadFilmWidth, &threadFilmHeight, threadFilmSubRegion);

	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->Init(renderEngine->film, threadFilmWidth, threadFilmHeight,
			threadFilmSubRegion);
}

void PathOCLBaseRenderThread::InitCamera() {
	AllocOCLBufferRO(&cameraBuff, &renderEngine->compiledScene->camera,
			sizeof(slg::ocl::Camera), "Camera");
}

void PathOCLBaseRenderThread::InitGeometry() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->normals.size() > 0)
		AllocOCLBufferRO(&normalsBuff, &cscene->normals[0],
				sizeof(Normal) * cscene->normals.size(), "Normals");
	else
		FreeOCLBuffer(&normalsBuff);

	if (cscene->uvs.size() > 0)
		AllocOCLBufferRO(&uvsBuff, &cscene->uvs[0],
			sizeof(UV) * cscene->uvs.size(), "UVs");
	else
		FreeOCLBuffer(&uvsBuff);

	if (cscene->cols.size() > 0)
		AllocOCLBufferRO(&colsBuff, &cscene->cols[0],
			sizeof(Spectrum) * cscene->cols.size(), "Colors");
	else
		FreeOCLBuffer(&colsBuff);

	if (cscene->alphas.size() > 0)
		AllocOCLBufferRO(&alphasBuff, &cscene->alphas[0],
			sizeof(float) * cscene->alphas.size(), "Alphas");
	else
		FreeOCLBuffer(&alphasBuff);

	AllocOCLBufferRO(&vertsBuff, &cscene->verts[0],
		sizeof(Point) * cscene->verts.size(), "Vertices");

	AllocOCLBufferRO(&trianglesBuff, &cscene->tris[0],
		sizeof(Triangle) * cscene->tris.size(), "Triangles");

	AllocOCLBufferRO(&meshDescsBuff, &cscene->meshDescs[0],
			sizeof(slg::ocl::Mesh) * cscene->meshDescs.size(), "Mesh description");
}

void PathOCLBaseRenderThread::InitMaterials() {
	const size_t materialsCount = renderEngine->compiledScene->mats.size();
	AllocOCLBufferRO(&materialsBuff, &renderEngine->compiledScene->mats[0],
			sizeof(slg::ocl::Material) * materialsCount, "Materials");
}

void PathOCLBaseRenderThread::InitSceneObjects() {
	const u_int sceneObjsCount = renderEngine->compiledScene->sceneObjs.size();
	AllocOCLBufferRO(&scnObjsBuff, &renderEngine->compiledScene->sceneObjs[0],
			sizeof(slg::ocl::SceneObject) * sceneObjsCount, "Scene objects");
}

void PathOCLBaseRenderThread::InitTextures() {
	const size_t texturesCount = renderEngine->compiledScene->texs.size();
	AllocOCLBufferRO(&texturesBuff, &renderEngine->compiledScene->texs[0],
			sizeof(slg::ocl::Texture) * texturesCount, "Textures");
}

void PathOCLBaseRenderThread::InitLights() {
	CompiledScene *cscene = renderEngine->compiledScene;

	AllocOCLBufferRO(&lightsBuff, &cscene->lightDefs[0],
		sizeof(slg::ocl::LightSource) * cscene->lightDefs.size(), "Lights");
	if (cscene->envLightIndices.size() > 0) {
		AllocOCLBufferRO(&envLightIndicesBuff, &cscene->envLightIndices[0],
				sizeof(u_int) * cscene->envLightIndices.size(), "Env. light indices");
	} else
		FreeOCLBuffer(&envLightIndicesBuff);

	AllocOCLBufferRO(&meshTriLightDefsOffsetBuff, &cscene->meshTriLightDefsOffset[0],
		sizeof(u_int) * cscene->meshTriLightDefsOffset.size(), "Light offsets");

	if (cscene->infiniteLightDistributions.size() > 0) {
		AllocOCLBufferRO(&infiniteLightDistributionsBuff, &cscene->infiniteLightDistributions[0],
			sizeof(float) * cscene->infiniteLightDistributions.size(), "InfiniteLight distributions");
	} else
		FreeOCLBuffer(&infiniteLightDistributionsBuff);

	AllocOCLBufferRO(&lightsDistributionBuff, cscene->lightsDistribution,
		cscene->lightsDistributionSize, "LightsDistribution");
	AllocOCLBufferRO(&infiniteLightSourcesDistributionBuff, cscene->infiniteLightSourcesDistribution,
		cscene->infiniteLightSourcesDistributionSize, "InfiniteLightSourcesDistribution");
}

void PathOCLBaseRenderThread::InitImageMaps() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->imageMapDescs.size() > 0) {
		AllocOCLBufferRO(&imageMapDescsBuff, &cscene->imageMapDescs[0],
				sizeof(slg::ocl::ImageMap) * cscene->imageMapDescs.size(), "ImageMap descriptions");

		// Free unused pages
		for (u_int i = cscene->imageMapMemBlocks.size(); i < imageMapsBuff.size(); ++i)
			FreeOCLBuffer(&imageMapsBuff[i]);
		imageMapsBuff.resize(cscene->imageMapMemBlocks.size(), NULL);

		for (u_int i = 0; i < imageMapsBuff.size(); ++i) {
			AllocOCLBufferRO(&(imageMapsBuff[i]), &(cscene->imageMapMemBlocks[i][0]),
					sizeof(float) * cscene->imageMapMemBlocks[i].size(), "ImageMaps");
		}
	} else {
		FreeOCLBuffer(&imageMapDescsBuff);
		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			FreeOCLBuffer(&imageMapsBuff[i]);
		imageMapsBuff.resize(0);
	}
}

void PathOCLBaseRenderThread::CompileKernel(cl::Program *program, cl::Kernel **kernel,
		size_t *workgroupSize, const string &name) {
	delete *kernel;
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling " << name << " Kernel");
	*kernel = new cl::Kernel(*program, name.c_str());

	if (intersectionDevice->GetDeviceDesc()->GetForceWorkGroupSize() > 0)
		*workgroupSize = intersectionDevice->GetDeviceDesc()->GetForceWorkGroupSize();
	else {
		cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();
		(*kernel)->getWorkGroupInfo<size_t>(oclDevice, CL_KERNEL_WORK_GROUP_SIZE, workgroupSize);
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] " << name << " workgroup size: " << *workgroupSize);
	}
}

void PathOCLBaseRenderThread::InitKernels() {
	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	CompiledScene *cscene = renderEngine->compiledScene;
	cl::Context &oclContext = intersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = intersectionDevice->GetOpenCLDevice();

	// Set #define symbols
	stringstream ssParams;
	ssParams.precision(6);
	ssParams << scientific <<
			" -D LUXRAYS_OPENCL_KERNEL" <<
			" -D SLG_OPENCL_KERNEL" <<
			" -D RENDER_ENGINE_" << RenderEngine::RenderEngineType2String(renderEngine->GetType()) <<
			" -D PARAM_RAY_EPSILON_MIN=" << MachineEpsilon::GetMin() << "f"
			" -D PARAM_RAY_EPSILON_MAX=" << MachineEpsilon::GetMax() << "f"
			" -D PARAM_LIGHT_WORLD_RADIUS_SCALE=" << InfiniteLightSource::LIGHT_WORLD_RADIUS_SCALE << "f"
			;

	if (cscene->hasTriangleLightWithVertexColors)
		ssParams << " -D PARAM_TRIANGLE_LIGHT_HAS_VERTEX_COLOR";

	switch (intersectionDevice->GetAccelerator()->GetType()) {
		case ACCEL_BVH:
			ssParams << " -D PARAM_ACCEL_BVH";
			break;
		case ACCEL_MBVH:
			ssParams << " -D PARAM_ACCEL_MBVH";
			break;
		case ACCEL_EMBREE:
			throw runtime_error("EMBREE accelerator is not supported in PathOCLBaseRenderThread::InitKernels()");
		default:
			throw runtime_error("Unknown accelerator in PathOCLBaseRenderThread::InitKernels()");
	}

	// Film related parameters

	// All thread films are supposed to have the same parameters
	const Film *threadFilm = threadFilms[0]->film;

	for (u_int i = 0; i < threadFilms[0]->channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		ssParams << " -D PARAM_FILM_RADIANCE_GROUP_" << i;
	ssParams << " -D PARAM_FILM_RADIANCE_GROUP_COUNT=" << threadFilms[0]->channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size();
	if (threadFilm->HasChannel(Film::ALPHA))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_ALPHA";
	if (threadFilm->HasChannel(Film::DEPTH))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_DEPTH";
	if (threadFilm->HasChannel(Film::POSITION))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_POSITION";
	if (threadFilm->HasChannel(Film::GEOMETRY_NORMAL))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL";
	if (threadFilm->HasChannel(Film::SHADING_NORMAL))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL";
	if (threadFilm->HasChannel(Film::MATERIAL_ID))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_MATERIAL_ID";
	if (threadFilm->HasChannel(Film::DIRECT_DIFFUSE))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE";
	if (threadFilm->HasChannel(Film::DIRECT_GLOSSY))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY";
	if (threadFilm->HasChannel(Film::EMISSION))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_EMISSION";
	if (threadFilm->HasChannel(Film::INDIRECT_DIFFUSE))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE";
	if (threadFilm->HasChannel(Film::INDIRECT_GLOSSY))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY";
	if (threadFilm->HasChannel(Film::INDIRECT_SPECULAR))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR";
	if (threadFilm->HasChannel(Film::MATERIAL_ID_MASK)) {
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK" <<
				" -D PARAM_FILM_MASK_MATERIAL_ID=" << threadFilm->GetMaskMaterialID(0);
	}
	if (threadFilm->HasChannel(Film::DIRECT_SHADOW_MASK))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK";
	if (threadFilm->HasChannel(Film::INDIRECT_SHADOW_MASK))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK";
	if (threadFilm->HasChannel(Film::UV))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_UV";
	if (threadFilm->HasChannel(Film::RAYCOUNT))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_RAYCOUNT";
	if (threadFilm->HasChannel(Film::BY_MATERIAL_ID)) {
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID" <<
				" -D PARAM_FILM_BY_MATERIAL_ID=" << threadFilm->GetByMaterialID(0);
	}
	if (threadFilm->HasChannel(Film::IRRADIANCE))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_IRRADIANCE";
	if (threadFilm->HasChannel(Film::OBJECT_ID))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_OBJECT_ID";
	if (threadFilm->HasChannel(Film::OBJECT_ID_MASK)) {
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK" <<
				" -D PARAM_FILM_MASK_OBJECT_ID=" << threadFilm->GetMaskObjectID(0);
	}
	if (threadFilm->HasChannel(Film::BY_OBJECT_ID)) {
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID" <<
				" -D PARAM_FILM_BY_OBJECT_ID=" << threadFilm->GetMaskObjectID(0);
	}
	if (threadFilm->HasChannel(Film::SAMPLECOUNT))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_SAMPLECOUNT";
	if (threadFilm->HasChannel(Film::CONVERGENCE))
		ssParams << " -D PARAM_FILM_CHANNELS_HAS_CONVERGENCE";

	if (cscene->IsTextureCompiled(CONST_FLOAT))
		ssParams << " -D PARAM_ENABLE_TEX_CONST_FLOAT";
	if (cscene->IsTextureCompiled(CONST_FLOAT3))
		ssParams << " -D PARAM_ENABLE_TEX_CONST_FLOAT3";
	if (cscene->IsTextureCompiled(IMAGEMAP))
		ssParams << " -D PARAM_ENABLE_TEX_IMAGEMAP";
	if (cscene->IsTextureCompiled(SCALE_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_SCALE";
	if (cscene->IsTextureCompiled(FRESNEL_APPROX_N))
		ssParams << " -D PARAM_ENABLE_FRESNEL_APPROX_N";
	if (cscene->IsTextureCompiled(FRESNEL_APPROX_K))
		ssParams << " -D PARAM_ENABLE_FRESNEL_APPROX_K";
	if (cscene->IsTextureCompiled(CHECKERBOARD2D))
		ssParams << " -D PARAM_ENABLE_CHECKERBOARD2D";
	if (cscene->IsTextureCompiled(CHECKERBOARD3D))
		ssParams << " -D PARAM_ENABLE_CHECKERBOARD3D";
	if (cscene->IsTextureCompiled(MIX_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_MIX";
	if (cscene->IsTextureCompiled(CLOUD_TEX))
		ssParams << " -D PARAM_ENABLE_CLOUD_TEX";
	if (cscene->IsTextureCompiled(FBM_TEX))
		ssParams << " -D PARAM_ENABLE_FBM_TEX";
	if (cscene->IsTextureCompiled(MARBLE))
		ssParams << " -D PARAM_ENABLE_MARBLE";
	if (cscene->IsTextureCompiled(DOTS))
		ssParams << " -D PARAM_ENABLE_DOTS";
	if (cscene->IsTextureCompiled(BRICK))
		ssParams << " -D PARAM_ENABLE_BRICK";
	if (cscene->IsTextureCompiled(ADD_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_ADD";
	if (cscene->IsTextureCompiled(SUBTRACT_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_SUBTRACT";
	if (cscene->IsTextureCompiled(WINDY))
		ssParams << " -D PARAM_ENABLE_WINDY";
	if (cscene->IsTextureCompiled(WRINKLED))
		ssParams << " -D PARAM_ENABLE_WRINKLED";
	if (cscene->IsTextureCompiled(BLENDER_BLEND))
		ssParams << " -D PARAM_ENABLE_BLENDER_BLEND";
 	if (cscene->IsTextureCompiled(BLENDER_CLOUDS))
 		ssParams << " -D PARAM_ENABLE_BLENDER_CLOUDS";
	if (cscene->IsTextureCompiled(BLENDER_DISTORTED_NOISE))
		ssParams << " -D PARAM_ENABLE_BLENDER_DISTORTED_NOISE";
	if (cscene->IsTextureCompiled(BLENDER_MAGIC))
		ssParams << " -D PARAM_ENABLE_BLENDER_MAGIC";
	if (cscene->IsTextureCompiled(BLENDER_MARBLE))
		ssParams << " -D PARAM_ENABLE_BLENDER_MARBLE";
	if (cscene->IsTextureCompiled(BLENDER_MUSGRAVE))
		ssParams << " -D PARAM_ENABLE_BLENDER_MUSGRAVE";
	if (cscene->IsTextureCompiled(BLENDER_STUCCI))
		ssParams << " -D PARAM_ENABLE_BLENDER_STUCCI";
 	if (cscene->IsTextureCompiled(BLENDER_WOOD))
 		ssParams << " -D PARAM_ENABLE_BLENDER_WOOD";
	if (cscene->IsTextureCompiled(BLENDER_VORONOI))
		ssParams << " -D PARAM_ENABLE_BLENDER_VORONOI";
    if (cscene->IsTextureCompiled(UV_TEX))
        ssParams << " -D PARAM_ENABLE_TEX_UV";
	if (cscene->IsTextureCompiled(BAND_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_BAND";
	if (cscene->IsTextureCompiled(HITPOINTCOLOR))
		ssParams << " -D PARAM_ENABLE_TEX_HITPOINTCOLOR";
	if (cscene->IsTextureCompiled(HITPOINTALPHA))
		ssParams << " -D PARAM_ENABLE_TEX_HITPOINTALPHA";
	if (cscene->IsTextureCompiled(HITPOINTGREY))
		ssParams << " -D PARAM_ENABLE_TEX_HITPOINTGREY";
	if (cscene->IsTextureCompiled(NORMALMAP_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_NORMALMAP";
	if (cscene->IsTextureCompiled(BLACKBODY_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_BLACKBODY";
	if (cscene->IsTextureCompiled(IRREGULARDATA_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_IRREGULARDATA";
	if (cscene->IsTextureCompiled(FRESNELCOLOR_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_FRESNELCOLOR";
	if (cscene->IsTextureCompiled(FRESNELCONST_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_FRESNELCONST";
	if (cscene->IsTextureCompiled(ABS_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_ABS";
	if (cscene->IsTextureCompiled(CLAMP_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_CLAMP";
	if (cscene->IsTextureCompiled(BILERP_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_BILERP";
	if (cscene->IsTextureCompiled(COLORDEPTH_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_COLORDEPTH";
	if (cscene->IsTextureCompiled(HSV_TEX))
		ssParams << " -D PARAM_ENABLE_TEX_HSV";

	if (cscene->IsMaterialCompiled(MATTE))
		ssParams << " -D PARAM_ENABLE_MAT_MATTE";
	if (cscene->IsMaterialCompiled(ROUGHMATTE))
		ssParams << " -D PARAM_ENABLE_MAT_ROUGHMATTE";
	if (cscene->IsMaterialCompiled(VELVET))
		ssParams << " -D PARAM_ENABLE_MAT_VELVET";
	if (cscene->IsMaterialCompiled(MIRROR))
		ssParams << " -D PARAM_ENABLE_MAT_MIRROR";
	if (cscene->IsMaterialCompiled(GLASS))
		ssParams << " -D PARAM_ENABLE_MAT_GLASS";
	if (cscene->IsMaterialCompiled(ARCHGLASS))
		ssParams << " -D PARAM_ENABLE_MAT_ARCHGLASS";
	if (cscene->IsMaterialCompiled(MIX))
		ssParams << " -D PARAM_ENABLE_MAT_MIX";
	if (cscene->IsMaterialCompiled(NULLMAT))
		ssParams << " -D PARAM_ENABLE_MAT_NULL";
	if (cscene->IsMaterialCompiled(MATTETRANSLUCENT))
		ssParams << " -D PARAM_ENABLE_MAT_MATTETRANSLUCENT";
	if (cscene->IsMaterialCompiled(ROUGHMATTETRANSLUCENT))
		ssParams << " -D PARAM_ENABLE_MAT_ROUGHMATTETRANSLUCENT";
	if (cscene->IsMaterialCompiled(GLOSSY2)) {
		ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2";

		if (cscene->IsMaterialCompiled(GLOSSY2_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC";
		if (cscene->IsMaterialCompiled(GLOSSY2_ABSORPTION))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION";
		if (cscene->IsMaterialCompiled(GLOSSY2_INDEX))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_INDEX";
		if (cscene->IsMaterialCompiled(GLOSSY2_MULTIBOUNCE))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE";
	}
	if (cscene->IsMaterialCompiled(METAL2)) {
		ssParams << " -D PARAM_ENABLE_MAT_METAL2";
		if (cscene->IsMaterialCompiled(METAL2_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_METAL2_ANISOTROPIC";
	}
	if (cscene->IsMaterialCompiled(ROUGHGLASS)) {
		ssParams << " -D PARAM_ENABLE_MAT_ROUGHGLASS";
		if (cscene->IsMaterialCompiled(ROUGHGLASS_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC";
	}
	if (cscene->IsMaterialCompiled(CLOTH))
		ssParams << " -D PARAM_ENABLE_MAT_CLOTH";
	if (cscene->IsMaterialCompiled(CARPAINT))
		ssParams << " -D PARAM_ENABLE_MAT_CARPAINT";
	if (cscene->IsMaterialCompiled(CLEAR_VOL))
		ssParams << " -D PARAM_ENABLE_MAT_CLEAR_VOL";
	if (cscene->IsMaterialCompiled(HOMOGENEOUS_VOL))
		ssParams << " -D PARAM_ENABLE_MAT_HOMOGENEOUS_VOL";
	if (cscene->IsMaterialCompiled(HETEROGENEOUS_VOL))
		ssParams << " -D PARAM_ENABLE_MAT_HETEROGENEOUS_VOL";
	if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT)) {
		ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT";

		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC";
		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_ABSORPTION))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION";
		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_INDEX))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX";
		if (cscene->IsMaterialCompiled(GLOSSYTRANSLUCENT_MULTIBOUNCE))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE";
	}
	if (cscene->IsMaterialCompiled(GLOSSYCOATING)) {
		ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING";

		if (cscene->IsMaterialCompiled(GLOSSYCOATING_ANISOTROPIC))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_ANISOTROPIC";
		if (cscene->IsMaterialCompiled(GLOSSYCOATING_ABSORPTION))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_ABSORPTION";
		if (cscene->IsMaterialCompiled(GLOSSYCOATING_INDEX))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_INDEX";
		if (cscene->IsMaterialCompiled(GLOSSYCOATING_MULTIBOUNCE))
			ssParams << " -D PARAM_ENABLE_MAT_GLOSSYCOATING_MULTIBOUNCE";
	}

	if (cscene->RequiresPassThrough())
		ssParams << " -D PARAM_HAS_PASSTHROUGH";

	switch (cscene->cameraType) {
		case slg::ocl::PERSPECTIVE:
			ssParams << " -D PARAM_CAMERA_TYPE=0";
			break;
		case slg::ocl::ORTHOGRAPHIC:
			ssParams << " -D PARAM_CAMERA_TYPE=1";
			break;
		case slg::ocl::STEREO:
			ssParams << " -D PARAM_CAMERA_TYPE=2";
			break;
		case slg::ocl::ENVIRONMENT:
			ssParams << " -D PARAM_CAMERA_TYPE=3";
			break;
		default:
			throw runtime_error("Unknown camera type in PathOCLBaseRenderThread::InitKernels()");
	}

	if (cscene->enableCameraClippingPlane)
		ssParams << " -D PARAM_CAMERA_ENABLE_CLIPPING_PLANE";
	if (cscene->enableCameraOculusRiftBarrel)
		ssParams << " -D PARAM_CAMERA_ENABLE_OCULUSRIFT_BARREL";

	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_IL) > 0)
		ssParams << " -D PARAM_HAS_INFINITELIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_IL_CONSTANT) > 0)
		ssParams << " -D PARAM_HAS_CONSTANTINFINITELIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_IL_SKY) > 0)
		ssParams << " -D PARAM_HAS_SKYLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_IL_SKY2) > 0)
		ssParams << " -D PARAM_HAS_SKYLIGHT2";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_SUN) > 0)
		ssParams << " -D PARAM_HAS_SUNLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_SHARPDISTANT) > 0)
		ssParams << " -D PARAM_HAS_SHARPDISTANTLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_DISTANT) > 0)
		ssParams << " -D PARAM_HAS_DISTANTLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_POINT) > 0)
		ssParams << " -D PARAM_HAS_POINTLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_MAPPOINT) > 0)
		ssParams << " -D PARAM_HAS_MAPPOINTLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_SPOT) > 0)
		ssParams << " -D PARAM_HAS_SPOTLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_PROJECTION) > 0)
		ssParams << " -D PARAM_HAS_PROJECTIONLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_LASER) > 0)
		ssParams << " -D PARAM_HAS_LASERLIGHT";
	if (renderEngine->compiledScene->IsLightSourceCompiled(TYPE_TRIANGLE) > 0)
		ssParams << " -D PARAM_HAS_TRIANGLELIGHT";

	if (renderEngine->compiledScene->hasEnvLights)
		ssParams << " -D PARAM_HAS_ENVLIGHTS";

	if (imageMapDescsBuff) {
		ssParams << " -D PARAM_HAS_IMAGEMAPS";
		if (imageMapsBuff.size() > 8)
			throw runtime_error("Too many memory pages required for image maps");
		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			ssParams << " -D PARAM_IMAGEMAPS_PAGE_" << i;
		ssParams << " -D PARAM_IMAGEMAPS_COUNT=" << imageMapsBuff.size();

		if (renderEngine->compiledScene->IsImageMapFormatCompiled(ImageMapStorage::BYTE))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_BYTE_FORMAT";
		if (renderEngine->compiledScene->IsImageMapFormatCompiled(ImageMapStorage::HALF))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_HALF_FORMAT";
		if (renderEngine->compiledScene->IsImageMapFormatCompiled(ImageMapStorage::FLOAT))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_FLOAT_FORMAT";

		if (renderEngine->compiledScene->IsImageMapChannelCountCompiled(1))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_1xCHANNELS";
		if (renderEngine->compiledScene->IsImageMapChannelCountCompiled(2))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_2xCHANNELS";
		if (renderEngine->compiledScene->IsImageMapChannelCountCompiled(3))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_3xCHANNELS";
		if (renderEngine->compiledScene->IsImageMapChannelCountCompiled(4))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_4xCHANNELS";

		if (renderEngine->compiledScene->IsImageMapWrapCompiled(ImageMapStorage::REPEAT))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_WRAP_REPEAT";
		if (renderEngine->compiledScene->IsImageMapWrapCompiled(ImageMapStorage::BLACK))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_WRAP_BLACK";
		if (renderEngine->compiledScene->IsImageMapWrapCompiled(ImageMapStorage::WHITE))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_WRAP_WHITE";
		if (renderEngine->compiledScene->IsImageMapWrapCompiled(ImageMapStorage::CLAMP))
			ssParams << " -D PARAM_HAS_IMAGEMAPS_WRAP_CLAMP";
	}
	
	if (renderEngine->compiledScene->HasBumpMaps())
		ssParams << " -D PARAM_HAS_BUMPMAPS";

	if (renderEngine->compiledScene->HasVolumes()) {
		ssParams << " -D PARAM_HAS_VOLUMES";
		ssParams << " -D SCENE_DEFAULT_VOLUME_INDEX=" << renderEngine->compiledScene->defaultWorldVolumeIndex;
	}

	ssParams << " " << renderEngine->additionalKernelOptions;

	ssParams << AdditionalKernelOptions();

	//--------------------------------------------------------------------------

	// Check the OpenCL vendor and use some specific compiler options

#if defined(__APPLE__)
	// Starting with 10.10 (darwin 14.x.x), opencl mix() function is fixed
	char darwin_ver[10];
	size_t len = sizeof(darwin_ver);
	sysctlbyname("kern.osrelease", &darwin_ver, &len, NULL, 0);
	if(darwin_ver[0] == '1' && darwin_ver[1] < '4') {
		ssParams << " -D __APPLE_CL__";
	}
#endif
	
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	kernelsParameters = ssParams.str();
	// This is a workaround for an Apple OpenCL by Arve Nygard. The double space
	// causes clBuildProgram() to fail with CL_INVALID_BUILD_OPTIONS on OSX.
	boost::replace_all(kernelsParameters, "  ", " ");

	// Compile sources
	stringstream ssKernel;
	ssKernel <<
		AdditionalKernelDefinitions() <<
		// OpenCL LuxRays Types
		luxrays::ocl::KernelSource_luxrays_types <<
		luxrays::ocl::KernelSource_randomgen_types <<
		luxrays::ocl::KernelSource_uv_types <<
		luxrays::ocl::KernelSource_point_types <<
		luxrays::ocl::KernelSource_vector_types <<
		luxrays::ocl::KernelSource_normal_types <<
		luxrays::ocl::KernelSource_triangle_types <<
		luxrays::ocl::KernelSource_ray_types <<
		luxrays::ocl::KernelSource_bbox_types <<
		luxrays::ocl::KernelSource_epsilon_types <<
		luxrays::ocl::KernelSource_color_types <<
		luxrays::ocl::KernelSource_frame_types <<
		luxrays::ocl::KernelSource_matrix4x4_types <<
		luxrays::ocl::KernelSource_quaternion_types <<
		luxrays::ocl::KernelSource_transform_types <<
		luxrays::ocl::KernelSource_motionsystem_types <<
		luxrays::ocl::KernelSource_trianglemesh_types <<
		// OpenCL LuxRays Funcs
		luxrays::ocl::KernelSource_randomgen_funcs <<
		luxrays::ocl::KernelSource_atomic_funcs <<
		luxrays::ocl::KernelSource_epsilon_funcs <<
		luxrays::ocl::KernelSource_utils_funcs <<
		luxrays::ocl::KernelSource_mc_funcs <<
		luxrays::ocl::KernelSource_vector_funcs <<
		luxrays::ocl::KernelSource_ray_funcs <<
		luxrays::ocl::KernelSource_bbox_funcs <<
		luxrays::ocl::KernelSource_color_funcs <<
		luxrays::ocl::KernelSource_frame_funcs <<
		luxrays::ocl::KernelSource_matrix4x4_funcs <<
		luxrays::ocl::KernelSource_quaternion_funcs <<
		luxrays::ocl::KernelSource_transform_funcs <<
		luxrays::ocl::KernelSource_motionsystem_funcs <<
		luxrays::ocl::KernelSource_triangle_funcs <<
		luxrays::ocl::KernelSource_trianglemesh_funcs <<
		// OpenCL SLG Types
		slg::ocl::KernelSource_hitpoint_types <<
		slg::ocl::KernelSource_mapping_types <<
		slg::ocl::KernelSource_texture_types <<
		slg::ocl::KernelSource_bsdf_types <<
		slg::ocl::KernelSource_material_types <<
		slg::ocl::KernelSource_volume_types <<
		slg::ocl::KernelSource_film_types <<
		slg::ocl::KernelSource_filter_types <<
		slg::ocl::KernelSource_sampler_types <<
		slg::ocl::KernelSource_camera_types <<
		slg::ocl::KernelSource_light_types <<
		slg::ocl::KernelSource_sceneobject_types <<
		// OpenCL SLG Funcs
		slg::ocl::KernelSource_mapping_funcs <<
		slg::ocl::KernelSource_imagemap_types <<
		slg::ocl::KernelSource_imagemap_funcs <<
		slg::ocl::KernelSource_texture_noise_funcs <<
		slg::ocl::KernelSource_texture_blender_noise_funcs <<
		slg::ocl::KernelSource_texture_blender_noise_funcs2 <<
		slg::ocl::KernelSource_texture_blender_funcs <<
		slg::ocl::KernelSource_texture_abs_funcs <<
		slg::ocl::KernelSource_texture_bilerp_funcs <<
		slg::ocl::KernelSource_texture_blackbody_funcs <<
		slg::ocl::KernelSource_texture_clamp_funcs <<
		slg::ocl::KernelSource_texture_colordepth_funcs <<
		slg::ocl::KernelSource_texture_fresnelcolor_funcs <<
		slg::ocl::KernelSource_texture_fresnelconst_funcs <<
		slg::ocl::KernelSource_texture_hsv_funcs <<
		slg::ocl::KernelSource_texture_irregulardata_funcs <<
		slg::ocl::KernelSource_texture_funcs;

	// Generate the code to evaluate the textures
	ssKernel <<
		"#line 2 \"Texture evaluation code form CompiledScene::GetTexturesEvaluationSourceCode()\"\n" <<
		cscene->GetTexturesEvaluationSourceCode() <<
		"\n";

	ssKernel <<
		slg::ocl::KernelSource_materialdefs_funcs_generic <<
		slg::ocl::KernelSource_materialdefs_funcs_default <<
		slg::ocl::KernelSource_materialdefs_funcs_archglass <<
		slg::ocl::KernelSource_materialdefs_funcs_carpaint <<
		slg::ocl::KernelSource_materialdefs_funcs_clearvol <<
		slg::ocl::KernelSource_materialdefs_funcs_cloth <<
		slg::ocl::KernelSource_materialdefs_funcs_glass <<
		slg::ocl::KernelSource_materialdefs_funcs_glossy2 <<
		slg::ocl::KernelSource_materialdefs_funcs_heterogeneousvol <<
		slg::ocl::KernelSource_materialdefs_funcs_homogeneousvol <<
		slg::ocl::KernelSource_materialdefs_funcs_matte <<
		slg::ocl::KernelSource_materialdefs_funcs_matte_translucent <<
		slg::ocl::KernelSource_materialdefs_funcs_metal2 <<
		slg::ocl::KernelSource_materialdefs_funcs_mirror <<
		slg::ocl::KernelSource_materialdefs_funcs_null <<
		slg::ocl::KernelSource_materialdefs_funcs_roughglass <<
		slg::ocl::KernelSource_materialdefs_funcs_roughmatte_translucent <<
		slg::ocl::KernelSource_materialdefs_funcs_velvet <<
		slg::ocl::KernelSource_materialdefs_funcs_glossytranslucent <<
		slg::ocl::KernelSource_material_main_withoutdynamic;

	// Generate the code to evaluate the materials
	ssKernel <<
		// This is the dynamic generated code (aka "WithDynamic")
		"#line 2 \"Material evaluation code form CompiledScene::GetMaterialsEvaluationSourceCode()\"\n" <<
		cscene->GetMaterialsEvaluationSourceCode() <<
		"\n" <<
		slg::ocl::KernelSource_material_main;

	ssKernel <<
		slg::ocl::KernelSource_bsdfutils_funcs << // Must be before volumeinfo_funcs
		slg::ocl::KernelSource_volume_funcs <<
		slg::ocl::KernelSource_volumeinfo_funcs <<
		slg::ocl::KernelSource_camera_funcs <<
		slg::ocl::KernelSource_light_funcs <<
		slg::ocl::KernelSource_filter_funcs <<
		slg::ocl::KernelSource_sampleresult_funcs <<
		slg::ocl::KernelSource_film_funcs <<
		slg::ocl::KernelSource_varianceclamping_funcs <<
		slg::ocl::KernelSource_sampler_random_funcs <<
		slg::ocl::KernelSource_sampler_sobol_funcs <<
		slg::ocl::KernelSource_sampler_metropolis_funcs <<
		slg::ocl::KernelSource_sampler_tilepath_funcs <<
		slg::ocl::KernelSource_bsdf_funcs <<
		slg::ocl::KernelSource_scene_funcs <<
		// PathOCL Funcs
		slg::ocl::KernelSource_pathoclbase_funcs;

	ssKernel << AdditionalKernelSources();

	string kernelSource = ssKernel.str();

	// Build the kernel source/parameters hash
	const string newKernelSrcHash = oclKernelPersistentCache::HashString(kernelsParameters) + "-" +
			oclKernelPersistentCache::HashString(kernelSource);
	if (newKernelSrcHash == kernelSrcHash) {
		// There is no need to re-compile the kernel
		return;
	} else
		kernelSrcHash = newKernelSrcHash;

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Defined symbols: " << kernelsParameters);
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Compiling kernels ");

	if (renderEngine->writeKernelsToFile) {
		// Some debug code to write the OpenCL kernel source to a file
		const string kernelFileName = "kernel_source_device_" + ToString(threadIndex) + ".cl";
		ofstream kernelFile(kernelFileName.c_str());
		string kernelDefs = kernelsParameters;
		boost::replace_all(kernelDefs, "-D", "\n#define");
		boost::replace_all(kernelDefs, "=", " ");
		kernelFile << kernelDefs << endl << endl << kernelSource << endl;
		kernelFile.close();
	}

	bool cached;
	cl::STRING_CLASS error;
	cl::Program *program = kernelCache->Compile(oclContext, oclDevice,
			kernelsParameters, kernelSource,
			&cached, &error);

	if (!program) {
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] PathOCL kernel compilation error" << endl << error);

		throw runtime_error("PathOCLBase kernel compilation error");
	}

	if (cached) {
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels cached");
	} else {
		SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels not cached");
	}

	// Film clear kernel
	CompileKernel(program, &filmClearKernel, &filmClearWorkGroupSize, "Film_Clear");

	// Additional kernels
	CompileAdditionalKernels(program);

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

	delete program;
}

void PathOCLBaseRenderThread::InitRender() {
	//--------------------------------------------------------------------------
	// Film definition
	//--------------------------------------------------------------------------

	InitFilm();

	//--------------------------------------------------------------------------
	// Camera definition
	//--------------------------------------------------------------------------

	InitCamera();

	//--------------------------------------------------------------------------
	// Scene geometry
	//--------------------------------------------------------------------------

	InitGeometry();

	//--------------------------------------------------------------------------
	// Image maps
	//--------------------------------------------------------------------------

	InitImageMaps();

	//--------------------------------------------------------------------------
	// Texture definitions
	//--------------------------------------------------------------------------

	InitTextures();

	//--------------------------------------------------------------------------
	// Material definitions
	//--------------------------------------------------------------------------

	InitMaterials();

	//--------------------------------------------------------------------------
	// Mesh <=> Material links
	//--------------------------------------------------------------------------

	InitSceneObjects();

	//--------------------------------------------------------------------------
	// Light definitions
	//--------------------------------------------------------------------------

	InitLights();

	AdditionalInit();

	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	InitKernels();

	//--------------------------------------------------------------------------
	// Initialize
	//--------------------------------------------------------------------------

	// Set kernel arguments
	SetKernelArgs();

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	// Clear all thread films
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->ClearFilm(oclQueue, *filmClearKernel, filmClearWorkGroupSize);

	oclQueue.finish();

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

void PathOCLBaseRenderThread::SetKernelArgs() {
	// Set OpenCL kernel arguments

	SetAdditionalKernelArgs();
}

void PathOCLBaseRenderThread::Start() {
	started = true;

	InitRender();
	StartRenderThread();
}

void PathOCLBaseRenderThread::Interrupt() {
	if (renderThread)
		renderThread->interrupt();
}

void PathOCLBaseRenderThread::Stop() {
	StopRenderThread();

	// Transfer the films
	TransferThreadFilms(intersectionDevice->GetOpenCLQueue());
	FreeThreadFilmsOCLBuffers();

	// Scene buffers
	FreeOCLBuffer(&materialsBuff);
	FreeOCLBuffer(&texturesBuff);
	FreeOCLBuffer(&meshDescsBuff);
	FreeOCLBuffer(&scnObjsBuff);
	FreeOCLBuffer(&normalsBuff);
	FreeOCLBuffer(&uvsBuff);
	FreeOCLBuffer(&colsBuff);
	FreeOCLBuffer(&alphasBuff);
	FreeOCLBuffer(&trianglesBuff);
	FreeOCLBuffer(&vertsBuff);
	FreeOCLBuffer(&lightsBuff);
	FreeOCLBuffer(&envLightIndicesBuff);
	FreeOCLBuffer(&lightsDistributionBuff);
	FreeOCLBuffer(&infiniteLightSourcesDistributionBuff);
	FreeOCLBuffer(&infiniteLightDistributionsBuff);
	FreeOCLBuffer(&cameraBuff);
	FreeOCLBuffer(&triLightDefsBuff);
	FreeOCLBuffer(&meshTriLightDefsOffsetBuff);
	FreeOCLBuffer(&imageMapDescsBuff);
	for (u_int i = 0; i < imageMapsBuff.size(); ++i)
		FreeOCLBuffer(&imageMapsBuff[i]);
	imageMapsBuff.resize(0);

	started = false;

	// Film is deleted in the destructor to allow image saving after
	// the rendering is finished
}

void PathOCLBaseRenderThread::StartRenderThread() {
	// Create the thread for the rendering
	renderThread = new boost::thread(&PathOCLBaseRenderThread::RenderThreadImpl, this);
}

void PathOCLBaseRenderThread::StopRenderThread() {
	if (renderThread) {
		renderThread->interrupt();
		renderThread->join();
		delete renderThread;
		renderThread = NULL;
	}
}

void PathOCLBaseRenderThread::BeginSceneEdit() {
	StopRenderThread();
}

void PathOCLBaseRenderThread::EndSceneEdit(const EditActionList &editActions) {
	//--------------------------------------------------------------------------
	// Update OpenCL buffers
	//
	// Note: if you edit this, you have probably to edit
	// RTPathOCLRenderThread::UpdateOCLBuffers().
	//--------------------------------------------------------------------------

	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->wasCameraCompiled) {
		// Update Camera
		InitCamera();
	}

	if (cscene->wasGeometryCompiled) {
		// Update Scene Geometry
		InitGeometry();
	}

	if (cscene->wasImageMapsCompiled) {
		// Update Image Maps
		InitImageMaps();
	}

	if (cscene->wasMaterialsCompiled) {
		// Update Scene Textures and Materials
		InitTextures();
		InitMaterials();
	}

	if (cscene->wasSceneObjectsCompiled) {
		// Update Mesh <=> Material relation
		InitSceneObjects();
	}

	if  (cscene->wasLightsCompiled) {
		// Update Scene Lights
		InitLights();
	}

	// A material types edit can enable/disable PARAM_HAS_PASSTHROUGH parameter
	// and change the size of the structure allocated
	if (editActions.Has(MATERIAL_TYPES_EDIT))
		AdditionalInit();

	//--------------------------------------------------------------------------
	// Recompile Kernels if required
	//--------------------------------------------------------------------------

	// The following actions can require a kernel re-compilation:
	// - Dynamic code generation of textures and materials;
	// - Material types edit;
	// - Light types edit;
	// - Image types edit;
	// - Geometry type edit;
	// - etc.
	InitKernels();

	if (editActions.HasAnyAction()) {
		SetKernelArgs();

		//----------------------------------------------------------------------
		// Execute initialization kernels
		//----------------------------------------------------------------------

		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

		// Clear the frame buffers
		ClearThreadFilms(oclQueue);
	}

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();

	StartRenderThread();
}

bool PathOCLBaseRenderThread::HasDone() const {
	return (renderThread == NULL) || (renderThread->timed_join(boost::posix_time::seconds(0)));
}

void PathOCLBaseRenderThread::WaitForDone() const {
	if (renderThread)
		renderThread->join();
}

void PathOCLBaseRenderThread::IncThreadFilms() {
	threadFilms.push_back(new ThreadFilm(this));

	// Initialize the new thread film
	u_int threadFilmWidth, threadFilmHeight, threadFilmSubRegion[4];
	GetThreadFilmSize(&threadFilmWidth, &threadFilmHeight, threadFilmSubRegion);

	threadFilms.back()->Init(renderEngine->film, threadFilmWidth, threadFilmHeight,
			threadFilmSubRegion);
}

void PathOCLBaseRenderThread::ClearThreadFilms(cl::CommandQueue &oclQueue) {
	// Clear all thread films
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->ClearFilm(oclQueue, *filmClearKernel, filmClearWorkGroupSize);
}

void PathOCLBaseRenderThread::TransferThreadFilms(cl::CommandQueue &oclQueue) {
	// Clear all thread films
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->RecvFilm(oclQueue);
}

void PathOCLBaseRenderThread::FreeThreadFilmsOCLBuffers() {
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->FreeAllOCLBuffers();
}

void PathOCLBaseRenderThread::FreeThreadFilms() {
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		delete threadFilm;
	threadFilms.clear();
}

#endif
