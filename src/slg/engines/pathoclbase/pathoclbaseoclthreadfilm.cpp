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

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/devices/ocldevice.h"
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

PathOCLBaseOCLRenderThread::ThreadFilm::ThreadFilm(PathOCLBaseOCLRenderThread *thread) {
	film = NULL;

	// Film buffers
	channel_ALPHA_Buff = NULL;
	channel_DEPTH_Buff = NULL;
	channel_POSITION_Buff = NULL;
	channel_GEOMETRY_NORMAL_Buff = NULL;
	channel_SHADING_NORMAL_Buff = NULL;
	channel_MATERIAL_ID_Buff = NULL;
	channel_DIRECT_DIFFUSE_Buff = NULL;
	channel_DIRECT_DIFFUSE_REFLECT_Buff = NULL;
	channel_DIRECT_DIFFUSE_TRANSMIT_Buff = NULL;
	channel_DIRECT_GLOSSY_Buff = NULL;
	channel_DIRECT_GLOSSY_REFLECT_Buff = NULL;
	channel_DIRECT_GLOSSY_TRANSMIT_Buff = NULL;
	channel_EMISSION_Buff = NULL;
	channel_INDIRECT_DIFFUSE_Buff = NULL;
	channel_INDIRECT_DIFFUSE_REFLECT_Buff = NULL;
	channel_INDIRECT_DIFFUSE_TRANSMIT_Buff = NULL;
	channel_INDIRECT_GLOSSY_Buff = NULL;
	channel_INDIRECT_GLOSSY_REFLECT_Buff = NULL;
	channel_INDIRECT_GLOSSY_TRANSMIT_Buff = NULL;
	channel_INDIRECT_SPECULAR_Buff = NULL;
	channel_INDIRECT_SPECULAR_REFLECT_Buff = NULL;
	channel_INDIRECT_SPECULAR_TRANSMIT_Buff = NULL;
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
	channel_MATERIAL_ID_COLOR_Buff = NULL;
	channel_ALBEDO_Buff = NULL;
	channel_AVG_SHADING_NORMAL_Buff = NULL;
	channel_NOISE_Buff = NULL;
	channel_USER_IMPORTANCE_Buff = NULL;
	
	// Denoiser sample accumulator buffers
	denoiser_NbOfSamplesImage_Buff = NULL;
	denoiser_SquaredWeightSumsImage_Buff = NULL;
	denoiser_MeanImage_Buff = NULL;
	denoiser_CovarImage_Buff = NULL;
	denoiser_HistoImage_Buff = NULL;

	renderThread = thread;
}

PathOCLBaseOCLRenderThread::ThreadFilm::~ThreadFilm() {
	delete film;

	FreeAllOCLBuffers();
}

void PathOCLBaseOCLRenderThread::ThreadFilm::Init(Film *engineFlm,
		const u_int threadFilmWidth, const u_int threadFilmHeight,
		const u_int *threadFilmSubRegion) {
	engineFilm = engineFlm;

	const u_int filmPixelCount = threadFilmWidth * threadFilmHeight;

	// Delete previous allocated Film
	delete film;

	// Allocate the new Film
	film = new Film(threadFilmWidth, threadFilmHeight, threadFilmSubRegion);
	film->CopyDynamicSettings(*engineFilm);
	// Engine film may have RADIANCE_PER_SCREEN_NORMALIZED channel because of
	// hybrid back/forward path tracing
	film->RemoveChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	film->Init();

	//--------------------------------------------------------------------------
	// Film channel buffers
	//--------------------------------------------------------------------------

	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		renderThread->intersectionDevice->FreeBuffer(&channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]);

	if (film->GetRadianceGroupCount() > 8)
		throw runtime_error("PathOCL supports only up to 8 Radiance Groups");

	const BufferType memTypeFlags = (renderThread->intersectionDevice->GetContext()->GetUseOutOfCoreBuffers() ||
				renderThread->renderEngine->useFilmOutOfCoreMemory) ?
		((BufferType)(BUFFER_TYPE_READ_WRITE | BUFFER_TYPE_OUT_OF_CORE)) :
		BUFFER_TYPE_READ_WRITE;

	channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.resize(film->GetRadianceGroupCount(), NULL);
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i) {
		renderThread->intersectionDevice->AllocBuffer(&channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i], memTypeFlags,
				nullptr, sizeof(float[4]) * filmPixelCount, "RADIANCE_PER_PIXEL_NORMALIZEDs[" + ToString(i) + "]");
	}
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::ALPHA))
		renderThread->intersectionDevice->AllocBuffer(&channel_ALPHA_Buff, memTypeFlags, nullptr, sizeof(float[2]) * filmPixelCount, "ALPHA");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_ALPHA_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DEPTH))
		renderThread->intersectionDevice->AllocBuffer(&channel_DEPTH_Buff, memTypeFlags, nullptr, sizeof(float) * filmPixelCount, "DEPTH");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_DEPTH_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::POSITION))
		renderThread->intersectionDevice->AllocBuffer(&channel_POSITION_Buff, memTypeFlags, nullptr, sizeof(float[3]) * filmPixelCount, "POSITION");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_POSITION_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::GEOMETRY_NORMAL))
		renderThread->intersectionDevice->AllocBuffer(&channel_GEOMETRY_NORMAL_Buff, memTypeFlags, nullptr, sizeof(float[3]) * filmPixelCount, "GEOMETRY_NORMAL");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_GEOMETRY_NORMAL_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::SHADING_NORMAL))
		renderThread->intersectionDevice->AllocBuffer(&channel_SHADING_NORMAL_Buff, memTypeFlags, nullptr, sizeof(float[3]) * filmPixelCount, "SHADING_NORMAL");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_SHADING_NORMAL_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::MATERIAL_ID))
		renderThread->intersectionDevice->AllocBuffer(&channel_MATERIAL_ID_Buff, memTypeFlags, nullptr, sizeof(u_int) * filmPixelCount, "MATERIAL_ID");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_MATERIAL_ID_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_DIFFUSE))
		renderThread->intersectionDevice->AllocBuffer(&channel_DIRECT_DIFFUSE_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "DIRECT_DIFFUSE");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_DIFFUSE_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_DIFFUSE_REFLECT))
		renderThread->intersectionDevice->AllocBuffer(&channel_DIRECT_DIFFUSE_REFLECT_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "DIRECT_DIFFUSE_REFLECT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_DIFFUSE_REFLECT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_DIFFUSE_TRANSMIT))
		renderThread->intersectionDevice->AllocBuffer(&channel_DIRECT_DIFFUSE_TRANSMIT_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "DIRECT_DIFFUSE_TRANSMIT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_DIFFUSE_TRANSMIT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_GLOSSY))
		renderThread->intersectionDevice->AllocBuffer(&channel_DIRECT_GLOSSY_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "DIRECT_GLOSSY");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_GLOSSY_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_GLOSSY_REFLECT))
		renderThread->intersectionDevice->AllocBuffer(&channel_DIRECT_GLOSSY_REFLECT_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "DIRECT_GLOSSY_REFLECT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_GLOSSY_REFLECT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_GLOSSY_TRANSMIT))
		renderThread->intersectionDevice->AllocBuffer(&channel_DIRECT_GLOSSY_TRANSMIT_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "DIRECT_GLOSSY_TRANSMIT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_GLOSSY_TRANSMIT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::EMISSION))
		renderThread->intersectionDevice->AllocBuffer(&channel_EMISSION_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "EMISSION");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_EMISSION_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_DIFFUSE))
		renderThread->intersectionDevice->AllocBuffer(&channel_INDIRECT_DIFFUSE_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "INDIRECT_DIFFUSE");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_DIFFUSE_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_DIFFUSE_REFLECT))
		renderThread->intersectionDevice->AllocBuffer(&channel_INDIRECT_DIFFUSE_REFLECT_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "INDIRECT_DIFFUSE_REFLECT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_DIFFUSE_REFLECT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_DIFFUSE_TRANSMIT))
		renderThread->intersectionDevice->AllocBuffer(&channel_INDIRECT_DIFFUSE_TRANSMIT_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "INDIRECT_DIFFUSE_TRANSMIT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_DIFFUSE_TRANSMIT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_GLOSSY))
		renderThread->intersectionDevice->AllocBuffer(&channel_INDIRECT_GLOSSY_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "INDIRECT_GLOSSY");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_GLOSSY_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_GLOSSY_REFLECT))
		renderThread->intersectionDevice->AllocBuffer(&channel_INDIRECT_GLOSSY_REFLECT_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "INDIRECT_GLOSSY_REFLECT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_GLOSSY_REFLECT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_GLOSSY_TRANSMIT))
		renderThread->intersectionDevice->AllocBuffer(&channel_INDIRECT_GLOSSY_TRANSMIT_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "INDIRECT_GLOSSY_TRANSMIT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_GLOSSY_TRANSMIT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_SPECULAR))
		renderThread->intersectionDevice->AllocBuffer(&channel_INDIRECT_SPECULAR_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "INDIRECT_SPECULAR");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_SPECULAR_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_SPECULAR_REFLECT))
		renderThread->intersectionDevice->AllocBuffer(&channel_INDIRECT_SPECULAR_REFLECT_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "INDIRECT_SPECULAR_REFLECT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_SPECULAR_REFLECT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_SPECULAR_TRANSMIT))
		renderThread->intersectionDevice->AllocBuffer(&channel_INDIRECT_SPECULAR_TRANSMIT_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "INDIRECT_SPECULAR_TRANSMIT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_SPECULAR_TRANSMIT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::MATERIAL_ID_MASK)) {
		if (film->GetChannelCount(Film::MATERIAL_ID_MASK) > 1)
			throw runtime_error("PathOCL supports only 1 MATERIAL_ID_MASK");
		else
			renderThread->intersectionDevice->AllocBuffer(&channel_MATERIAL_ID_MASK_Buff, memTypeFlags,
					nullptr, sizeof(float[2]) * filmPixelCount, "MATERIAL_ID_MASK");
	} else
		renderThread->intersectionDevice->FreeBuffer(&channel_MATERIAL_ID_MASK_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::DIRECT_SHADOW_MASK))
		renderThread->intersectionDevice->AllocBuffer(&channel_DIRECT_SHADOW_MASK_Buff, memTypeFlags, nullptr, sizeof(float[2]) * filmPixelCount, "DIRECT_SHADOW_MASK");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_SHADOW_MASK_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::INDIRECT_SHADOW_MASK))
		renderThread->intersectionDevice->AllocBuffer(&channel_INDIRECT_SHADOW_MASK_Buff, memTypeFlags, nullptr, sizeof(float[2]) * filmPixelCount, "INDIRECT_SHADOW_MASK");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_SHADOW_MASK_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::UV))
		renderThread->intersectionDevice->AllocBuffer(&channel_UV_Buff, memTypeFlags, nullptr, sizeof(float[2]) * filmPixelCount, "UV");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_UV_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::RAYCOUNT))
		renderThread->intersectionDevice->AllocBuffer(&channel_RAYCOUNT_Buff, memTypeFlags, nullptr, sizeof(float) * filmPixelCount, "RAYCOUNT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_RAYCOUNT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::BY_MATERIAL_ID)) {
		if (film->GetChannelCount(Film::BY_MATERIAL_ID) > 1)
			throw runtime_error("PathOCL supports only 1 BY_MATERIAL_ID");
		else
			renderThread->intersectionDevice->AllocBuffer(&channel_BY_MATERIAL_ID_Buff, memTypeFlags,
					nullptr, sizeof(float[4]) * filmPixelCount, "BY_MATERIAL_ID");
	} else
		renderThread->intersectionDevice->FreeBuffer(&channel_BY_MATERIAL_ID_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::IRRADIANCE))
		renderThread->intersectionDevice->AllocBuffer(&channel_IRRADIANCE_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "IRRADIANCE");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_IRRADIANCE_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::OBJECT_ID))
		renderThread->intersectionDevice->AllocBuffer(&channel_OBJECT_ID_Buff, memTypeFlags, nullptr, sizeof(u_int) * filmPixelCount, "OBJECT_ID");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_OBJECT_ID_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::OBJECT_ID_MASK)) {
		if (film->GetChannelCount(Film::OBJECT_ID_MASK) > 1)
			throw runtime_error("PathOCL supports only 1 OBJECT_ID_MASK");
		else
			renderThread->intersectionDevice->AllocBuffer(&channel_OBJECT_ID_MASK_Buff, memTypeFlags,
					nullptr, sizeof(float[2]) * filmPixelCount, "OBJECT_ID_MASK");
	} else
		renderThread->intersectionDevice->FreeBuffer(&channel_OBJECT_ID_MASK_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::BY_OBJECT_ID)) {
		if (film->GetChannelCount(Film::BY_OBJECT_ID) > 1)
			throw runtime_error("PathOCL supports only 1 BY_OBJECT_ID");
		else
			renderThread->intersectionDevice->AllocBuffer(&channel_BY_OBJECT_ID_Buff, memTypeFlags,
					nullptr, sizeof(float[4]) * filmPixelCount, "BY_OBJECT_ID");
	} else
		renderThread->intersectionDevice->FreeBuffer(&channel_BY_OBJECT_ID_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::SAMPLECOUNT))
		renderThread->intersectionDevice->AllocBuffer(&channel_SAMPLECOUNT_Buff, memTypeFlags, nullptr, sizeof(u_int) * filmPixelCount, "SAMPLECOUNT");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_SAMPLECOUNT_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::CONVERGENCE))
		renderThread->intersectionDevice->AllocBuffer(&channel_CONVERGENCE_Buff, memTypeFlags, nullptr, sizeof(float) * filmPixelCount, "CONVERGENCE");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_CONVERGENCE_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::MATERIAL_ID_COLOR))
		renderThread->intersectionDevice->AllocBuffer(&channel_MATERIAL_ID_COLOR_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "MATERIAL_ID_COLOR");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_MATERIAL_ID_COLOR_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::ALBEDO))
		renderThread->intersectionDevice->AllocBuffer(&channel_ALBEDO_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "ALBEDO");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_ALBEDO_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::AVG_SHADING_NORMAL))
		renderThread->intersectionDevice->AllocBuffer(&channel_AVG_SHADING_NORMAL_Buff, memTypeFlags, nullptr, sizeof(float[4]) * filmPixelCount, "AVG_SHADING_NORMAL");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_AVG_SHADING_NORMAL_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::NOISE))
		renderThread->intersectionDevice->AllocBuffer(&channel_NOISE_Buff, memTypeFlags, nullptr, sizeof(float) * filmPixelCount, "NOISE");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_NOISE_Buff);
	//--------------------------------------------------------------------------
	if (film->HasChannel(Film::USER_IMPORTANCE))
		renderThread->intersectionDevice->AllocBuffer(&channel_USER_IMPORTANCE_Buff, memTypeFlags, nullptr, sizeof(float) * filmPixelCount, "USER_IMPORTANCE");
	else
		renderThread->intersectionDevice->FreeBuffer(&channel_USER_IMPORTANCE_Buff);

	//--------------------------------------------------------------------------
	// Film denoiser sample accumulator buffers
	//--------------------------------------------------------------------------

	if (film->GetDenoiser().IsEnabled()) {
		renderThread->intersectionDevice->AllocBuffer(&denoiser_NbOfSamplesImage_Buff, memTypeFlags,
				nullptr, sizeof(float) * filmPixelCount, "Denoiser samples count");
		renderThread->intersectionDevice->AllocBuffer(&denoiser_SquaredWeightSumsImage_Buff, memTypeFlags,
				nullptr, sizeof(float) * filmPixelCount, "Denoiser squared weight");
		renderThread->intersectionDevice->AllocBuffer(&denoiser_MeanImage_Buff, memTypeFlags,
				nullptr, sizeof(float[3]) * filmPixelCount, "Denoiser mean image");
		renderThread->intersectionDevice->AllocBuffer(&denoiser_CovarImage_Buff, memTypeFlags,
				nullptr, sizeof(float[6]) * filmPixelCount, "Denoiser covariance");
		renderThread->intersectionDevice->AllocBuffer(&denoiser_HistoImage_Buff, memTypeFlags,
				nullptr, film->GetDenoiser().GetHistogramBinsCount() * 3 * sizeof(float) * filmPixelCount,
				"Denoiser sample histogram");
	} else {
		renderThread->intersectionDevice->FreeBuffer(&denoiser_NbOfSamplesImage_Buff);
		renderThread->intersectionDevice->FreeBuffer(&denoiser_SquaredWeightSumsImage_Buff);
		renderThread->intersectionDevice->FreeBuffer(&denoiser_MeanImage_Buff);
		renderThread->intersectionDevice->FreeBuffer(&denoiser_CovarImage_Buff);
		renderThread->intersectionDevice->FreeBuffer(&denoiser_HistoImage_Buff);
	}
}

void PathOCLBaseOCLRenderThread::ThreadFilm::FreeAllOCLBuffers() {
	// Film buffers
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i)
		renderThread->intersectionDevice->FreeBuffer(&channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]);
	channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.clear();
	renderThread->intersectionDevice->FreeBuffer(&channel_ALPHA_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_DEPTH_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_POSITION_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_GEOMETRY_NORMAL_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_SHADING_NORMAL_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_MATERIAL_ID_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_DIFFUSE_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_DIFFUSE_REFLECT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_DIFFUSE_TRANSMIT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_GLOSSY_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_GLOSSY_REFLECT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_GLOSSY_TRANSMIT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_EMISSION_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_DIFFUSE_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_DIFFUSE_REFLECT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_DIFFUSE_TRANSMIT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_GLOSSY_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_GLOSSY_REFLECT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_GLOSSY_TRANSMIT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_SPECULAR_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_SPECULAR_REFLECT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_SPECULAR_TRANSMIT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_MATERIAL_ID_MASK_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_DIRECT_SHADOW_MASK_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_INDIRECT_SHADOW_MASK_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_UV_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_RAYCOUNT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_BY_MATERIAL_ID_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_IRRADIANCE_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_OBJECT_ID_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_OBJECT_ID_MASK_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_BY_OBJECT_ID_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_SAMPLECOUNT_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_CONVERGENCE_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_MATERIAL_ID_COLOR_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_ALBEDO_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_AVG_SHADING_NORMAL_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_NOISE_Buff);
	renderThread->intersectionDevice->FreeBuffer(&channel_USER_IMPORTANCE_Buff);

	// Film denoiser sample accumulator buffers
	renderThread->intersectionDevice->FreeBuffer(&denoiser_NbOfSamplesImage_Buff);
	renderThread->intersectionDevice->FreeBuffer(&denoiser_SquaredWeightSumsImage_Buff);
	renderThread->intersectionDevice->FreeBuffer(&denoiser_MeanImage_Buff);
	renderThread->intersectionDevice->FreeBuffer(&denoiser_CovarImage_Buff);
	renderThread->intersectionDevice->FreeBuffer(&denoiser_HistoImage_Buff);
}

u_int PathOCLBaseOCLRenderThread::ThreadFilm::SetFilmKernelArgs(HardwareIntersectionDevice *intersectionDevice,
		HardwareDeviceKernel *kernel, u_int argIndex) const {
	// Film parameters
	
	intersectionDevice->SetKernelArg(kernel, argIndex++, film->GetWidth());
	intersectionDevice->SetKernelArg(kernel, argIndex++, film->GetHeight());

	const u_int *filmSubRegion = film->GetSubRegion();
	intersectionDevice->SetKernelArg(kernel, argIndex++, filmSubRegion[0]);
	intersectionDevice->SetKernelArg(kernel, argIndex++, filmSubRegion[1]);
	intersectionDevice->SetKernelArg(kernel, argIndex++, filmSubRegion[2]);
	intersectionDevice->SetKernelArg(kernel, argIndex++, filmSubRegion[3]);

	for (u_int i = 0; i < FILM_MAX_RADIANCE_GROUP_COUNT; ++i) {
		if (i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size())
			intersectionDevice->SetKernelArg(kernel, argIndex++, channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]);
		else
			intersectionDevice->SetKernelArg(kernel, argIndex++, nullptr);
	}

	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_ALPHA_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_DEPTH_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_POSITION_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_GEOMETRY_NORMAL_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_SHADING_NORMAL_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_MATERIAL_ID_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_DIRECT_DIFFUSE_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_DIRECT_DIFFUSE_REFLECT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_DIRECT_DIFFUSE_TRANSMIT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_DIRECT_GLOSSY_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_DIRECT_GLOSSY_REFLECT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_DIRECT_GLOSSY_TRANSMIT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_EMISSION_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_INDIRECT_DIFFUSE_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_INDIRECT_DIFFUSE_REFLECT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_INDIRECT_DIFFUSE_TRANSMIT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_INDIRECT_GLOSSY_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_INDIRECT_GLOSSY_REFLECT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_INDIRECT_GLOSSY_TRANSMIT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_INDIRECT_SPECULAR_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_INDIRECT_SPECULAR_REFLECT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_INDIRECT_SPECULAR_TRANSMIT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_MATERIAL_ID_MASK_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_DIRECT_SHADOW_MASK_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_INDIRECT_SHADOW_MASK_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_UV_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_RAYCOUNT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_BY_MATERIAL_ID_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_IRRADIANCE_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_OBJECT_ID_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_OBJECT_ID_MASK_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_BY_OBJECT_ID_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_SAMPLECOUNT_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_CONVERGENCE_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_MATERIAL_ID_COLOR_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_ALBEDO_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_AVG_SHADING_NORMAL_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_NOISE_Buff);
	intersectionDevice->SetKernelArg(kernel, argIndex++, channel_USER_IMPORTANCE_Buff);

	// Film denoiser sample accumulator parameters
	FilmDenoiser &denoiser = film->GetDenoiser();
	if (denoiser.IsEnabled()) {
		intersectionDevice->SetKernelArg(kernel, argIndex++, (int)denoiser.IsWarmUpDone());
		intersectionDevice->SetKernelArg(kernel, argIndex++, denoiser.GetSampleGamma());
		intersectionDevice->SetKernelArg(kernel, argIndex++, denoiser.GetSampleMaxValue());
		intersectionDevice->SetKernelArg(kernel, argIndex++, denoiser.GetSampleScale());
		intersectionDevice->SetKernelArg(kernel, argIndex++, denoiser.GetHistogramBinsCount());
		intersectionDevice->SetKernelArg(kernel, argIndex++, denoiser_NbOfSamplesImage_Buff);
		intersectionDevice->SetKernelArg(kernel, argIndex++, denoiser_SquaredWeightSumsImage_Buff);
		intersectionDevice->SetKernelArg(kernel, argIndex++, denoiser_MeanImage_Buff);
		intersectionDevice->SetKernelArg(kernel, argIndex++, denoiser_CovarImage_Buff);
		intersectionDevice->SetKernelArg(kernel, argIndex++, denoiser_HistoImage_Buff);

		if (denoiser.IsWarmUpDone()) {
			const vector<RadianceChannelScale> &scales = denoiser.GetRadianceChannelScales();
			for (u_int i = 0; i < FILM_MAX_RADIANCE_GROUP_COUNT; ++i) {
				if (i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size()) {
					const Spectrum s = scales[i].GetScale();
					intersectionDevice->SetKernelArg(kernel, argIndex++, s.c[0]);
					intersectionDevice->SetKernelArg(kernel, argIndex++, s.c[1]);
					intersectionDevice->SetKernelArg(kernel, argIndex++, s.c[2]);
				} else {
					intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);
					intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);
					intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);					
				}
			}
		} else {
			for (u_int i = 0; i < FILM_MAX_RADIANCE_GROUP_COUNT; ++i) {
				intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);
				intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);
				intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);
			}
		}
	} else {
		intersectionDevice->SetKernelArg(kernel, argIndex++, 0);
		intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);
		intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);
		intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);
		intersectionDevice->SetKernelArg(kernel, argIndex++, 0);
		intersectionDevice->SetKernelArg(kernel, argIndex++, nullptr);
		intersectionDevice->SetKernelArg(kernel, argIndex++, nullptr);
		intersectionDevice->SetKernelArg(kernel, argIndex++, nullptr);
		intersectionDevice->SetKernelArg(kernel, argIndex++, nullptr);
		intersectionDevice->SetKernelArg(kernel, argIndex++, nullptr);

		for (u_int i = 0; i < FILM_MAX_RADIANCE_GROUP_COUNT; ++i) {
			intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);
			intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);
			intersectionDevice->SetKernelArg(kernel, argIndex++, 0.f);
		}
	}

	return argIndex;
}

void PathOCLBaseOCLRenderThread::ThreadFilm::RecvFilm(HardwareIntersectionDevice *intersectionDevice) {
	// Async. transfer of the Film buffers

	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i) {
		if (channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]) {
			intersectionDevice->EnqueueReadBuffer(
				channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i],
				CL_FALSE,
				channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]->GetSize(),
				film->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixels());
		}
	}

	if (channel_ALPHA_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_ALPHA_Buff,
			CL_FALSE,
			channel_ALPHA_Buff->GetSize(),
			film->channel_ALPHA->GetPixels());
	}
	if (channel_DEPTH_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_DEPTH_Buff,
			CL_FALSE,
			channel_DEPTH_Buff->GetSize(),
			film->channel_DEPTH->GetPixels());
	}
	if (channel_POSITION_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_POSITION_Buff,
			CL_FALSE,
			channel_POSITION_Buff->GetSize(),
			film->channel_POSITION->GetPixels());
	}
	if (channel_GEOMETRY_NORMAL_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_GEOMETRY_NORMAL_Buff,
			CL_FALSE,
			channel_GEOMETRY_NORMAL_Buff->GetSize(),
			film->channel_GEOMETRY_NORMAL->GetPixels());
	}
	if (channel_SHADING_NORMAL_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_SHADING_NORMAL_Buff,
			CL_FALSE,
			channel_SHADING_NORMAL_Buff->GetSize(),
			film->channel_SHADING_NORMAL->GetPixels());
	}
	if (channel_MATERIAL_ID_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_MATERIAL_ID_Buff,
			CL_FALSE,
			channel_MATERIAL_ID_Buff->GetSize(),
			film->channel_MATERIAL_ID->GetPixels());
	}
	if (channel_DIRECT_DIFFUSE_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_DIRECT_DIFFUSE_Buff,
			CL_FALSE,
			channel_DIRECT_DIFFUSE_Buff->GetSize(),
			film->channel_DIRECT_DIFFUSE->GetPixels());
	}
	if (channel_DIRECT_DIFFUSE_REFLECT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_DIRECT_DIFFUSE_REFLECT_Buff,
			CL_FALSE,
			channel_DIRECT_DIFFUSE_REFLECT_Buff->GetSize(),
			film->channel_DIRECT_DIFFUSE_REFLECT->GetPixels());
	}
	if (channel_DIRECT_DIFFUSE_TRANSMIT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_DIRECT_DIFFUSE_TRANSMIT_Buff,
			CL_FALSE,
			channel_DIRECT_DIFFUSE_TRANSMIT_Buff->GetSize(),
			film->channel_DIRECT_DIFFUSE_TRANSMIT->GetPixels());
	}
	if (channel_DIRECT_GLOSSY_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_DIRECT_GLOSSY_Buff,
			CL_FALSE,
			channel_DIRECT_GLOSSY_Buff->GetSize(),
			film->channel_DIRECT_GLOSSY->GetPixels());
	}
	if (channel_DIRECT_GLOSSY_REFLECT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_DIRECT_GLOSSY_REFLECT_Buff,
			CL_FALSE,
			channel_DIRECT_GLOSSY_REFLECT_Buff->GetSize(),
			film->channel_DIRECT_GLOSSY_REFLECT->GetPixels());
	}
	if (channel_DIRECT_GLOSSY_TRANSMIT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_DIRECT_GLOSSY_TRANSMIT_Buff,
			CL_FALSE,
			channel_DIRECT_GLOSSY_TRANSMIT_Buff->GetSize(),
			film->channel_DIRECT_GLOSSY_TRANSMIT->GetPixels());
	}
	if (channel_EMISSION_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_EMISSION_Buff,
			CL_FALSE,
			channel_EMISSION_Buff->GetSize(),
			film->channel_EMISSION->GetPixels());
	}
	if (channel_INDIRECT_DIFFUSE_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_INDIRECT_DIFFUSE_Buff,
			CL_FALSE,
			channel_INDIRECT_DIFFUSE_Buff->GetSize(),
			film->channel_INDIRECT_DIFFUSE->GetPixels());
	}
	if (channel_INDIRECT_DIFFUSE_REFLECT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_INDIRECT_DIFFUSE_REFLECT_Buff,
			CL_FALSE,
			channel_INDIRECT_DIFFUSE_REFLECT_Buff->GetSize(),
			film->channel_INDIRECT_DIFFUSE_REFLECT->GetPixels());
	}
	if (channel_INDIRECT_DIFFUSE_TRANSMIT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_INDIRECT_DIFFUSE_TRANSMIT_Buff,
			CL_FALSE,
			channel_INDIRECT_DIFFUSE_TRANSMIT_Buff->GetSize(),
			film->channel_INDIRECT_DIFFUSE_TRANSMIT->GetPixels());
	}
	if (channel_INDIRECT_GLOSSY_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_INDIRECT_GLOSSY_Buff,
			CL_FALSE,
			channel_INDIRECT_GLOSSY_Buff->GetSize(),
			film->channel_INDIRECT_GLOSSY->GetPixels());
	}
	if (channel_INDIRECT_GLOSSY_REFLECT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_INDIRECT_GLOSSY_REFLECT_Buff,
			CL_FALSE,
			channel_INDIRECT_GLOSSY_REFLECT_Buff->GetSize(),
			film->channel_INDIRECT_GLOSSY_REFLECT->GetPixels());
	}
	if (channel_INDIRECT_GLOSSY_TRANSMIT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_INDIRECT_GLOSSY_TRANSMIT_Buff,
			CL_FALSE,
			channel_INDIRECT_GLOSSY_TRANSMIT_Buff->GetSize(),
			film->channel_INDIRECT_GLOSSY_TRANSMIT->GetPixels());
	}
	if (channel_INDIRECT_SPECULAR_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_INDIRECT_SPECULAR_Buff,
			CL_FALSE,
			channel_INDIRECT_SPECULAR_Buff->GetSize(),
			film->channel_INDIRECT_SPECULAR->GetPixels());
	}
	if (channel_INDIRECT_SPECULAR_REFLECT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_INDIRECT_SPECULAR_REFLECT_Buff,
			CL_FALSE,
			channel_INDIRECT_SPECULAR_REFLECT_Buff->GetSize(),
			film->channel_INDIRECT_SPECULAR_REFLECT->GetPixels());
	}
	if (channel_INDIRECT_SPECULAR_TRANSMIT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_INDIRECT_SPECULAR_TRANSMIT_Buff,
			CL_FALSE,
			channel_INDIRECT_SPECULAR_TRANSMIT_Buff->GetSize(),
			film->channel_INDIRECT_SPECULAR_TRANSMIT->GetPixels());
	}
	if (channel_MATERIAL_ID_MASK_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_MATERIAL_ID_MASK_Buff,
			CL_FALSE,
			channel_MATERIAL_ID_MASK_Buff->GetSize(),
			film->channel_MATERIAL_ID_MASKs[0]->GetPixels());
	}
	if (channel_DIRECT_SHADOW_MASK_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_DIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			channel_DIRECT_SHADOW_MASK_Buff->GetSize(),
			film->channel_DIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_INDIRECT_SHADOW_MASK_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_INDIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			channel_INDIRECT_SHADOW_MASK_Buff->GetSize(),
			film->channel_INDIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_UV_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_UV_Buff,
			CL_FALSE,
			channel_UV_Buff->GetSize(),
			film->channel_UV->GetPixels());
	}
	if (channel_RAYCOUNT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_RAYCOUNT_Buff,
			CL_FALSE,
			channel_RAYCOUNT_Buff->GetSize(),
			film->channel_RAYCOUNT->GetPixels());
	}
	if (channel_BY_MATERIAL_ID_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_BY_MATERIAL_ID_Buff,
			CL_FALSE,
			channel_BY_MATERIAL_ID_Buff->GetSize(),
			film->channel_BY_MATERIAL_IDs[0]->GetPixels());
	}
	if (channel_IRRADIANCE_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_IRRADIANCE_Buff,
			CL_FALSE,
			channel_IRRADIANCE_Buff->GetSize(),
			film->channel_IRRADIANCE->GetPixels());
	}
	if (channel_OBJECT_ID_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_OBJECT_ID_Buff,
			CL_FALSE,
			channel_OBJECT_ID_Buff->GetSize(),
			film->channel_OBJECT_ID->GetPixels());
	}
	if (channel_OBJECT_ID_MASK_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_OBJECT_ID_MASK_Buff,
			CL_FALSE,
			channel_OBJECT_ID_MASK_Buff->GetSize(),
			film->channel_OBJECT_ID_MASKs[0]->GetPixels());
	}
	if (channel_BY_OBJECT_ID_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_BY_OBJECT_ID_Buff,
			CL_FALSE,
			channel_BY_OBJECT_ID_Buff->GetSize(),
			film->channel_BY_OBJECT_IDs[0]->GetPixels());
	}
	if (channel_SAMPLECOUNT_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_SAMPLECOUNT_Buff,
			CL_FALSE,
			channel_SAMPLECOUNT_Buff->GetSize(),
			film->channel_SAMPLECOUNT->GetPixels());
	}
	if (channel_CONVERGENCE_Buff) {
		// This may look wrong but CONVERGENCE channel is compute by the engine
		// film convergence test on the CPU so I write instead of read (to
		// synchronize the content).
		intersectionDevice->EnqueueWriteBuffer(
			channel_CONVERGENCE_Buff,
			CL_FALSE,
			channel_CONVERGENCE_Buff->GetSize(),
			engineFilm->channel_CONVERGENCE->GetPixels());
	}
	if (channel_MATERIAL_ID_COLOR_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_MATERIAL_ID_COLOR_Buff,
			CL_FALSE,
			channel_MATERIAL_ID_COLOR_Buff->GetSize(),
			film->channel_MATERIAL_ID_COLOR->GetPixels());
	}
	if (channel_ALBEDO_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_ALBEDO_Buff,
			CL_FALSE,
			channel_ALBEDO_Buff->GetSize(),
			film->channel_ALBEDO->GetPixels());
	}
	if (channel_AVG_SHADING_NORMAL_Buff) {
		intersectionDevice->EnqueueReadBuffer(
			channel_AVG_SHADING_NORMAL_Buff,
			CL_FALSE,
			channel_AVG_SHADING_NORMAL_Buff->GetSize(),
			film->channel_AVG_SHADING_NORMAL->GetPixels());
	}
	if (channel_NOISE_Buff) {
		// This may look wrong but NOISE channel is compute by the engine
		// film noise estimation on the CPU so I write instead of read (to
		// synchronize the content).
		intersectionDevice->EnqueueWriteBuffer(
			channel_NOISE_Buff,
			CL_FALSE,
			channel_NOISE_Buff->GetSize(),
			engineFilm->channel_NOISE->GetPixels());
	}
	if (channel_USER_IMPORTANCE_Buff) {
		// This may look wrong but USER_IMPORTANCE channel is like NOISE channel
		// so I write instead of read (to synchronize the content).
		intersectionDevice->EnqueueWriteBuffer(
			channel_USER_IMPORTANCE_Buff,
			CL_FALSE,
			channel_USER_IMPORTANCE_Buff->GetSize(),
			engineFilm->channel_USER_IMPORTANCE->GetPixels());
	}

	// Async. transfer of the Film denoiser sample accumulator buffers
	FilmDenoiser &denoiser = film->GetDenoiser();
	if (denoiser.IsEnabled() && denoiser.IsWarmUpDone()) {
		intersectionDevice->EnqueueReadBuffer(
			denoiser_NbOfSamplesImage_Buff,
			CL_FALSE,
			denoiser_NbOfSamplesImage_Buff->GetSize(),
			denoiser.GetNbOfSamplesImage());
		intersectionDevice->EnqueueReadBuffer(
			denoiser_SquaredWeightSumsImage_Buff,
			CL_FALSE,
			denoiser_SquaredWeightSumsImage_Buff->GetSize(),
			denoiser.GetSquaredWeightSumsImage());
		intersectionDevice->EnqueueReadBuffer(
			denoiser_MeanImage_Buff,
			CL_FALSE,
			denoiser_MeanImage_Buff->GetSize(),
			denoiser.GetMeanImage());
		intersectionDevice->EnqueueReadBuffer(
			denoiser_CovarImage_Buff,
			CL_FALSE,
			denoiser_CovarImage_Buff->GetSize(),
			denoiser.GetCovarImage());
		intersectionDevice->EnqueueReadBuffer(
			denoiser_HistoImage_Buff,
			CL_FALSE,
			denoiser_HistoImage_Buff->GetSize(),
			denoiser.GetHistoImage());
	}
}

void PathOCLBaseOCLRenderThread::ThreadFilm::SendFilm(HardwareIntersectionDevice *intersectionDevice) {
	// Async. transfer of the Film buffers

	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff.size(); ++i) {
		if (channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]) {
			intersectionDevice->EnqueueWriteBuffer(
				channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i],
				CL_FALSE,
				channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff[i]->GetSize(),
				film->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixels());
		}
	}

	if (channel_ALPHA_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_ALPHA_Buff,
			CL_FALSE,
			channel_ALPHA_Buff->GetSize(),
			film->channel_ALPHA->GetPixels());
	}
	if (channel_DEPTH_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_DEPTH_Buff,
			CL_FALSE,
			channel_DEPTH_Buff->GetSize(),
			film->channel_DEPTH->GetPixels());
	}
	if (channel_POSITION_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_POSITION_Buff,
			CL_FALSE,
			channel_POSITION_Buff->GetSize(),
			film->channel_POSITION->GetPixels());
	}
	if (channel_GEOMETRY_NORMAL_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_GEOMETRY_NORMAL_Buff,
			CL_FALSE,
			channel_GEOMETRY_NORMAL_Buff->GetSize(),
			film->channel_GEOMETRY_NORMAL->GetPixels());
	}
	if (channel_SHADING_NORMAL_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_SHADING_NORMAL_Buff,
			CL_FALSE,
			channel_SHADING_NORMAL_Buff->GetSize(),
			film->channel_SHADING_NORMAL->GetPixels());
	}
	if (channel_MATERIAL_ID_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_MATERIAL_ID_Buff,
			CL_FALSE,
			channel_MATERIAL_ID_Buff->GetSize(),
			film->channel_MATERIAL_ID->GetPixels());
	}
	if (channel_DIRECT_DIFFUSE_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_DIRECT_DIFFUSE_Buff,
			CL_FALSE,
			channel_DIRECT_DIFFUSE_Buff->GetSize(),
			film->channel_DIRECT_DIFFUSE->GetPixels());
	}
	if (channel_DIRECT_DIFFUSE_REFLECT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_DIRECT_DIFFUSE_REFLECT_Buff,
			CL_FALSE,
			channel_DIRECT_DIFFUSE_REFLECT_Buff->GetSize(),
			film->channel_DIRECT_DIFFUSE_REFLECT->GetPixels());
	}
	if (channel_DIRECT_DIFFUSE_TRANSMIT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_DIRECT_DIFFUSE_TRANSMIT_Buff,
			CL_FALSE,
			channel_DIRECT_DIFFUSE_TRANSMIT_Buff->GetSize(),
			film->channel_DIRECT_DIFFUSE_TRANSMIT->GetPixels());
	}
	if (channel_DIRECT_GLOSSY_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_DIRECT_GLOSSY_Buff,
			CL_FALSE,
			channel_DIRECT_GLOSSY_Buff->GetSize(),
			film->channel_DIRECT_GLOSSY->GetPixels());
	}
	if (channel_DIRECT_GLOSSY_REFLECT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_DIRECT_GLOSSY_REFLECT_Buff,
			CL_FALSE,
			channel_DIRECT_GLOSSY_REFLECT_Buff->GetSize(),
			film->channel_DIRECT_GLOSSY_REFLECT->GetPixels());
	}
	if (channel_DIRECT_GLOSSY_TRANSMIT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_DIRECT_GLOSSY_TRANSMIT_Buff,
			CL_FALSE,
			channel_DIRECT_GLOSSY_TRANSMIT_Buff->GetSize(),
			film->channel_DIRECT_GLOSSY_TRANSMIT->GetPixels());
	}
	if (channel_EMISSION_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_EMISSION_Buff,
			CL_FALSE,
			channel_EMISSION_Buff->GetSize(),
			film->channel_EMISSION->GetPixels());
	}
	if (channel_INDIRECT_DIFFUSE_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_INDIRECT_DIFFUSE_Buff,
			CL_FALSE,
			channel_INDIRECT_DIFFUSE_Buff->GetSize(),
			film->channel_INDIRECT_DIFFUSE->GetPixels());
	}
	if (channel_INDIRECT_DIFFUSE_REFLECT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_INDIRECT_DIFFUSE_REFLECT_Buff,
			CL_FALSE,
			channel_INDIRECT_DIFFUSE_REFLECT_Buff->GetSize(),
			film->channel_INDIRECT_DIFFUSE_REFLECT->GetPixels());
	}
	if (channel_INDIRECT_DIFFUSE_TRANSMIT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_INDIRECT_DIFFUSE_TRANSMIT_Buff,
			CL_FALSE,
			channel_INDIRECT_DIFFUSE_TRANSMIT_Buff->GetSize(),
			film->channel_INDIRECT_DIFFUSE_TRANSMIT->GetPixels());
	}
	if (channel_INDIRECT_GLOSSY_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_INDIRECT_GLOSSY_Buff,
			CL_FALSE,
			channel_INDIRECT_GLOSSY_Buff->GetSize(),
			film->channel_INDIRECT_GLOSSY->GetPixels());
	}
	if (channel_INDIRECT_GLOSSY_REFLECT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_INDIRECT_GLOSSY_REFLECT_Buff,
			CL_FALSE,
			channel_INDIRECT_GLOSSY_REFLECT_Buff->GetSize(),
			film->channel_INDIRECT_GLOSSY_REFLECT->GetPixels());
	}
	if (channel_INDIRECT_GLOSSY_TRANSMIT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_INDIRECT_GLOSSY_TRANSMIT_Buff,
			CL_FALSE,
			channel_INDIRECT_GLOSSY_TRANSMIT_Buff->GetSize(),
			film->channel_INDIRECT_GLOSSY_TRANSMIT->GetPixels());
	}
	if (channel_INDIRECT_SPECULAR_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_INDIRECT_SPECULAR_Buff,
			CL_FALSE,
			channel_INDIRECT_SPECULAR_Buff->GetSize(),
			film->channel_INDIRECT_SPECULAR->GetPixels());
	}
	if (channel_INDIRECT_SPECULAR_REFLECT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_INDIRECT_SPECULAR_REFLECT_Buff,
			CL_FALSE,
			channel_INDIRECT_SPECULAR_REFLECT_Buff->GetSize(),
			film->channel_INDIRECT_SPECULAR_REFLECT->GetPixels());
	}
	if (channel_INDIRECT_SPECULAR_TRANSMIT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_INDIRECT_SPECULAR_TRANSMIT_Buff,
			CL_FALSE,
			channel_INDIRECT_SPECULAR_TRANSMIT_Buff->GetSize(),
			film->channel_INDIRECT_SPECULAR_TRANSMIT->GetPixels());
	}
	if (channel_MATERIAL_ID_MASK_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_MATERIAL_ID_MASK_Buff,
			CL_FALSE,
			channel_MATERIAL_ID_MASK_Buff->GetSize(),
			film->channel_MATERIAL_ID_MASKs[0]->GetPixels());
	}
	if (channel_DIRECT_SHADOW_MASK_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_DIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			channel_DIRECT_SHADOW_MASK_Buff->GetSize(),
			film->channel_DIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_INDIRECT_SHADOW_MASK_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_INDIRECT_SHADOW_MASK_Buff,
			CL_FALSE,
			channel_INDIRECT_SHADOW_MASK_Buff->GetSize(),
			film->channel_INDIRECT_SHADOW_MASK->GetPixels());
	}
	if (channel_UV_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_UV_Buff,
			CL_FALSE,
			channel_UV_Buff->GetSize(),
			film->channel_UV->GetPixels());
	}
	if (channel_RAYCOUNT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_RAYCOUNT_Buff,
			CL_FALSE,
			channel_RAYCOUNT_Buff->GetSize(),
			film->channel_RAYCOUNT->GetPixels());
	}
	if (channel_BY_MATERIAL_ID_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_BY_MATERIAL_ID_Buff,
			CL_FALSE,
			channel_BY_MATERIAL_ID_Buff->GetSize(),
			film->channel_BY_MATERIAL_IDs[0]->GetPixels());
	}
	if (channel_IRRADIANCE_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_IRRADIANCE_Buff,
			CL_FALSE,
			channel_IRRADIANCE_Buff->GetSize(),
			film->channel_IRRADIANCE->GetPixels());
	}
	if (channel_OBJECT_ID_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_OBJECT_ID_Buff,
			CL_FALSE,
			channel_OBJECT_ID_Buff->GetSize(),
			film->channel_OBJECT_ID->GetPixels());
	}
	if (channel_OBJECT_ID_MASK_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_OBJECT_ID_MASK_Buff,
			CL_FALSE,
			channel_OBJECT_ID_MASK_Buff->GetSize(),
			film->channel_OBJECT_ID_MASKs[0]->GetPixels());
	}
	if (channel_BY_OBJECT_ID_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_BY_OBJECT_ID_Buff,
			CL_FALSE,
			channel_BY_OBJECT_ID_Buff->GetSize(),
			film->channel_BY_OBJECT_IDs[0]->GetPixels());
	}
	if (channel_SAMPLECOUNT_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_SAMPLECOUNT_Buff,
			CL_FALSE,
			channel_SAMPLECOUNT_Buff->GetSize(),
			film->channel_SAMPLECOUNT->GetPixels());
	}
	if (channel_CONVERGENCE_Buff) {
		// The CONVERGENCE channel is compute by the engine
		// film convergence test on the CPU so I write the engine film, not the
		// thread film.
		intersectionDevice->EnqueueWriteBuffer(
			channel_CONVERGENCE_Buff,
			CL_FALSE,
			channel_CONVERGENCE_Buff->GetSize(),
			engineFilm->channel_CONVERGENCE->GetPixels());
	}
	if (channel_MATERIAL_ID_COLOR_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_MATERIAL_ID_COLOR_Buff,
			CL_FALSE,
			channel_MATERIAL_ID_COLOR_Buff->GetSize(),
			film->channel_MATERIAL_ID_COLOR->GetPixels());
	}
	if (channel_ALBEDO_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_ALBEDO_Buff,
			CL_FALSE,
			channel_ALBEDO_Buff->GetSize(),
			film->channel_ALBEDO->GetPixels());
	}
	if (channel_AVG_SHADING_NORMAL_Buff) {
		intersectionDevice->EnqueueWriteBuffer(
			channel_AVG_SHADING_NORMAL_Buff,
			CL_FALSE,
			channel_AVG_SHADING_NORMAL_Buff->GetSize(),
			film->channel_AVG_SHADING_NORMAL->GetPixels());
	}
	if (channel_NOISE_Buff) {
		// The NOISE channel is compute by the engine
		// film noise estimation on the CPU so I write the engine film, not the
		// thread film.
		intersectionDevice->EnqueueWriteBuffer(
			channel_NOISE_Buff,
			CL_FALSE,
			channel_NOISE_Buff->GetSize(),
			engineFilm->channel_NOISE->GetPixels());
	}
	if (channel_USER_IMPORTANCE_Buff) {
		// The USER_IMPORTANCE channel is like NOISE channel
		// so I write the engine film, not the thread film.
		intersectionDevice->EnqueueWriteBuffer(
			channel_USER_IMPORTANCE_Buff,
			CL_FALSE,
			channel_USER_IMPORTANCE_Buff->GetSize(),
			engineFilm->channel_USER_IMPORTANCE->GetPixels());
	}

	// Async. transfer of the Film denoiser sample accumulator buffers
	FilmDenoiser &denoiser = film->GetDenoiser();
	if (denoiser.IsEnabled()) {
		if (denoiser.GetNbOfSamplesImage())
			intersectionDevice->EnqueueWriteBuffer(
				denoiser_NbOfSamplesImage_Buff,
				CL_FALSE,
				denoiser_NbOfSamplesImage_Buff->GetSize(),
				denoiser.GetNbOfSamplesImage());
		if (denoiser.GetSquaredWeightSumsImage())
			intersectionDevice->EnqueueWriteBuffer(
				denoiser_SquaredWeightSumsImage_Buff,
				CL_FALSE,
				denoiser_SquaredWeightSumsImage_Buff->GetSize(),
				denoiser.GetSquaredWeightSumsImage());
		if (denoiser.GetMeanImage())
			intersectionDevice->EnqueueWriteBuffer(
				denoiser_MeanImage_Buff,
				CL_FALSE,
				denoiser_MeanImage_Buff->GetSize(),
				denoiser.GetMeanImage());
		if (denoiser.GetCovarImage())
			intersectionDevice->EnqueueWriteBuffer(
				denoiser_CovarImage_Buff,
				CL_FALSE,
				denoiser_CovarImage_Buff->GetSize(),
				denoiser.GetCovarImage());
		if (denoiser.GetHistoImage())
			intersectionDevice->EnqueueWriteBuffer(
				denoiser_HistoImage_Buff,
				CL_FALSE,
				denoiser_HistoImage_Buff->GetSize(),
				denoiser.GetHistoImage());
	}
}

void PathOCLBaseOCLRenderThread::ThreadFilm::ClearFilm(HardwareIntersectionDevice *intersectionDevice,
		HardwareDeviceKernel *filmClearKernel, const size_t filmClearWorkGroupSize) {
	// Set kernel arguments
	
	// This is the dummy variable required by KERNEL_ARGS_FILM macro
	intersectionDevice->SetKernelArg(filmClearKernel, 0, 0);

	SetFilmKernelArgs(intersectionDevice, filmClearKernel, 1);
	
	// Clear the film
	const u_int filmPixelCount = film->GetWidth() * film->GetHeight();
	intersectionDevice->EnqueueKernel(filmClearKernel,
			HardwareDeviceRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
			HardwareDeviceRange(filmClearWorkGroupSize));
}

#endif
