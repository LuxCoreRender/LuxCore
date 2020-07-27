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

#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#include "slg/film/film.h"
#include "slg/kernels/kernels.h"
#include "slg/film/imagepipeline/plugins/objectidmask.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ObjectIDMask filter plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ObjectIDMaskFilterPlugin)

ObjectIDMaskFilterPlugin::ObjectIDMaskFilterPlugin(const u_int id) {
	objectID = id;

	applyKernel = nullptr;
}

ObjectIDMaskFilterPlugin::ObjectIDMaskFilterPlugin() {
	applyKernel = nullptr;
}

ObjectIDMaskFilterPlugin::~ObjectIDMaskFilterPlugin() {
	delete applyKernel;
}

ImagePipelinePlugin *ObjectIDMaskFilterPlugin::Copy() const {
	return new ObjectIDMaskFilterPlugin(objectID);
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void ObjectIDMaskFilterPlugin::Apply(Film &film, const u_int index) {
	if (!film.HasChannel(Film::OBJECT_ID)) {
		// I can not work without OBJECT_ID channel
		return;
	}

	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < pixelCount; ++i) {
		const bool maskValue = film.HasSamples(hasPN, hasSN, i);
		const u_int objectIDValue = *(film.channel_OBJECT_ID->GetPixel(i));

		const float value = (maskValue && (objectIDValue == objectID)) ? 1.f : 0.f;

		pixels[i].c[0] = value;
		pixels[i].c[1] = value;
		pixels[i].c[2] = value;
	}
}

//------------------------------------------------------------------------------
// HardwareDevice version
//------------------------------------------------------------------------------

void ObjectIDMaskFilterPlugin::AddHWChannelsUsed(unordered_set<Film::FilmChannelType, hash<int> > &hwChannelsUsed) const {
	hwChannelsUsed.insert(Film::IMAGEPIPELINE);
	hwChannelsUsed.insert(Film::OBJECT_ID);
}

void ObjectIDMaskFilterPlugin::ApplyHW(Film &film, const u_int index) {
	if (!film.HasChannel(Film::OBJECT_ID)) {
		// I can not work without OBJECT_ID channel
		return;
	}

	HardwareDevice *hardwareDevice = film.hardwareDevice;

	if (!applyKernel) {
		film.ctx->SetVerbose(true);
		
		// Compile sources
		const double tStart = WallClockTime();

		vector<string> opts;
		opts.push_back("-D LUXRAYS_OPENCL_KERNEL");
		opts.push_back("-D SLG_OPENCL_KERNEL");

		HardwareDeviceProgram *program = nullptr;
		hardwareDevice->CompileProgram(&program,
				opts,
				slg::ocl::KernelSource_plugin_objectidmask_funcs,
				"ObjectIDMaskFilterPlugin");

		//----------------------------------------------------------------------
		// ObjectIDMaskFilterPlugin_Apply kernel
		//----------------------------------------------------------------------

		SLG_LOG("[ObjectIDMaskFilterPlugin] Compiling ObjectIDMaskFilterPlugin_Apply Kernel");
		hardwareDevice->GetKernel(program, &applyKernel, "ObjectIDMaskFilterPlugin_Apply");

		// Set kernel arguments
		u_int argIndex = 0;
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetWidth());
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.GetHeight());
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.hw_IMAGEPIPELINE);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, film.hw_OBJECT_ID);
		hardwareDevice->SetKernelArg(applyKernel, argIndex++, objectID);

		//----------------------------------------------------------------------

		delete program;

		const double tEnd = WallClockTime();
		SLG_LOG("[ObjectIDMaskFilterPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");

		film.ctx->SetVerbose(false);
	}

	hardwareDevice->EnqueueKernel(applyKernel, HardwareDeviceRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)),
			HardwareDeviceRange(256));
}
