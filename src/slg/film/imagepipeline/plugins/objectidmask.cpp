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

#if !defined(LUXRAYS_DISABLE_OPENCL)
	applyKernel = NULL;
#endif
}

ObjectIDMaskFilterPlugin::ObjectIDMaskFilterPlugin() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	applyKernel = NULL;
#endif
}

ObjectIDMaskFilterPlugin::~ObjectIDMaskFilterPlugin() {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	delete applyKernel;
#endif
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

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < pixelCount; ++i) {
		const uint maskValue = *(film.channel_FRAMEBUFFER_MASK->GetPixel(i));
		const uint objectIDValue = *(film.channel_OBJECT_ID->GetPixel(i));

		const float value = (maskValue && (objectIDValue == objectID)) ? 1.f : 0.f;

		pixels[i].c[0] = value;
		pixels[i].c[1] = value;
		pixels[i].c[2] = value;
	}
}

//------------------------------------------------------------------------------
// OpenCL version
//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
void ObjectIDMaskFilterPlugin::ApplyOCL(Film &film, const u_int index) {
	if (!film.HasChannel(Film::OBJECT_ID)) {
		// I can not work without OBJECT_ID channel
		return;
	}

	if (!applyKernel) {
		// Compile sources
		const double tStart = WallClockTime();

		cl::Program *program = ImagePipelinePlugin::CompileProgram(
				film,
				"-D LUXRAYS_OPENCL_KERNEL -D SLG_OPENCL_KERNEL",
				slg::ocl::KernelSource_plugin_objectidmask_funcs,
				"ObjectIDMaskFilterPlugin");

		//----------------------------------------------------------------------
		// ObjectIDMaskFilterPlugin_Apply kernel
		//----------------------------------------------------------------------

		SLG_LOG("[ObjectIDMaskFilterPlugin] Compiling ObjectIDMaskFilterPlugin_Apply Kernel");
		applyKernel = new cl::Kernel(*program, "ObjectIDMaskFilterPlugin_Apply");

		// Set kernel arguments
		u_int argIndex = 0;
		applyKernel->setArg(argIndex++, film.GetWidth());
		applyKernel->setArg(argIndex++, film.GetHeight());
		applyKernel->setArg(argIndex++, *(film.ocl_IMAGEPIPELINE));
		applyKernel->setArg(argIndex++, *(film.ocl_FRAMEBUFFER_MASK));
		applyKernel->setArg(argIndex++, *(film.ocl_OBJECT_ID));
		applyKernel->setArg(argIndex++, objectID);

		//----------------------------------------------------------------------

		delete program;

		const double tEnd = WallClockTime();
		SLG_LOG("[ObjectIDMaskFilterPlugin] Kernels compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
	}
	
	film.oclIntersectionDevice->GetOpenCLQueue().enqueueNDRangeKernel(*applyKernel,
			cl::NullRange, cl::NDRange(RoundUp(film.GetWidth() * film.GetHeight(), 256u)), cl::NDRange(256));
}
#endif
