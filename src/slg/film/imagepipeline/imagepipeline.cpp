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

#include <boost/foreach.hpp>
#include <boost/serialization/export.hpp>

#include "slg/film/imagepipeline/imagepipeline.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/film.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImagePipelinePlugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImagePipelinePlugin)

#if !defined(LUXRAYS_DISABLE_OPENCL)
cl::Program *ImagePipelinePlugin::CompileProgram(Film &film, const string &kernelsParameters,
		const string &kernelSource, const string &name) {
	cl::Context &oclContext = film.oclIntersectionDevice->GetOpenCLContext();
	cl::Device &oclDevice = film.oclIntersectionDevice->GetOpenCLDevice();

	SLG_LOG("[" << name << "] Defined symbols: " << kernelsParameters);
	SLG_LOG("[" << name << "] Compiling kernels ");

	bool cached;
	cl::STRING_CLASS error;
	cl::Program *program = film.kernelCache->Compile(oclContext, oclDevice,
			kernelsParameters, kernelSource,
			&cached, &error);
	if (!program) {
		SLG_LOG("[" << name << "] kernel compilation error" << endl << error);

		throw runtime_error(name + " kernel compilation error");
	}

	if (cached) {
		SLG_LOG("[" << name << "] Kernels cached");
	} else {
		SLG_LOG("[" << name << "] Kernels not cached");
	}

	return program;
}
#endif

float ImagePipelinePlugin::GetGammaCorrectionValue(const Film &film, const u_int index) {
	float gamma = 1.f;
	const ImagePipeline *ip = film.GetImagePipeline(index);
	if (ip) {
		const GammaCorrectionPlugin *gc = (const GammaCorrectionPlugin *)ip->GetPlugin(typeid(GammaCorrectionPlugin));
		if (gc)
			gamma = gc->gamma;
	}

	return gamma;
}

//------------------------------------------------------------------------------
// ImagePipeline
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ImagePipeline)

ImagePipeline::ImagePipeline() {
	canUseOpenCL = false;
}

ImagePipeline::~ImagePipeline() {
	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline)
		delete plugin;
}

ImagePipeline *ImagePipeline::Copy() const {
	ImagePipeline *ip = new ImagePipeline();

	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline) {
		ip->AddPlugin(plugin->Copy());
	}

	return ip;
}

void ImagePipeline::AddPlugin(ImagePipelinePlugin *plugin) {
	pipeline.push_back(plugin);

	canUseOpenCL |= plugin->CanUseOpenCL();
}

void ImagePipeline::Apply(Film &film, const u_int index) {
	//const double t1 = WallClockTime();

#if !defined(LUXRAYS_DISABLE_OPENCL)
	bool imageInCPURam = true;
	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline) {
		//const double p1 = WallClockTime();

		const bool useOpenCLApply = film.oclEnable && film.oclIntersectionDevice &&
				plugin->CanUseOpenCL();

		if (useOpenCLApply) {
			if (imageInCPURam) {
				// Transfer the buffer to OpenCL device ram
				//SLG_LOG("Transferring IMAGEPIPELINE buffer to OpenCL device");
				film.WriteOCLBuffer_IMAGEPIPELINE(index);
			} else {
				// The buffer is already in the OpenCL device ram
			}
		} else {
			if (!imageInCPURam) {
				// Transfer the buffer from OpenCL device ram
				//SLG_LOG("Transferring IMAGEPIPELINE buffer from OpenCL device");
				film.ReadOCLBuffer_IMAGEPIPELINE(index);
				film.oclIntersectionDevice->GetOpenCLQueue().finish();
			} else {
				// The buffer is already in CPU ram
			}
		}

		if (useOpenCLApply) {
			plugin->ApplyOCL(film, index);
			imageInCPURam = false;
		} else {
			plugin->Apply(film, index);
			imageInCPURam = true;			
		}
		
		//const double p2 = WallClockTime();
		//SLG_LOG("ImagePipeline plugin time: " << int((p2 - p1) * 1000.0) << "ms");
	}

	if (film.oclEnable && film.oclIntersectionDevice && canUseOpenCL) {
		if (!imageInCPURam)
			film.ReadOCLBuffer_IMAGEPIPELINE(index);

		film.oclIntersectionDevice->GetOpenCLQueue().finish();
	}
#else
	BOOST_FOREACH(ImagePipelinePlugin *plugin, pipeline) {
		//const double p1 = WallClockTime();

		plugin->Apply(film, index);
		
		//const double p2 = WallClockTime();
		//SLG_LOG("ImagePipeline plugin time: " << int((p2 - p1) * 1000.0) << "ms");
	}
#endif

	//const double t2 = WallClockTime();
	//SLG_LOG("ImagePipeline time: " << int((t2 - t1) * 1000.0) << "ms");
}

const ImagePipelinePlugin *ImagePipeline::GetPlugin(const std::type_info &type) const {
	BOOST_FOREACH(const ImagePipelinePlugin *plugin, pipeline) {
		if (typeid(*plugin) == type)
			return plugin;
	}

	return NULL;
}
