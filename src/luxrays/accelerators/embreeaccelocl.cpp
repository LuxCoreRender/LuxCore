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

#include "luxrays/accelerators/embreeaccel.h"
#include "luxrays/core/utils.h"
#include "luxrays/core/context.h"
#ifdef LUXRAYS_DISABLE_OPENCL
#include "luxrays/core/intersectiondevice.h"
#else
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"
#endif

namespace luxrays {

#if !defined(LUXRAYS_DISABLE_OPENCL)

class OpenCLEmbreeKernels : public OpenCLKernels {
public:
	OpenCLEmbreeKernels(OpenCLIntersectionDevice *dev, const u_int kernelCount, const EmbreeAccel *embreeAccel);
	virtual ~OpenCLEmbreeKernels();

	virtual void Update(const DataSet *newDataSet) { assert(false); }
	virtual void EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

	virtual u_int SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int argIndex);
};

OpenCLEmbreeKernels::OpenCLEmbreeKernels(OpenCLIntersectionDevice *dev, const u_int kernelCount, const EmbreeAccel *embreeAccel) :
		OpenCLKernels(dev, kernelCount) {
	throw std::runtime_error("Called OpenCLEmbreeKernels::OpenCLEmbreeKernels()");
}

OpenCLEmbreeKernels::~OpenCLEmbreeKernels() {
}

void OpenCLEmbreeKernels::EnqueueRayBuffer(cl::CommandQueue &oclQueue, const u_int kernelIndex,
		cl::Buffer &rBuff, cl::Buffer &hBuff, const u_int rayCount,
		const VECTOR_CLASS<cl::Event> *events, cl::Event *event) {
}

u_int OpenCLEmbreeKernels::SetIntersectionKernelArgs(cl::Kernel &kernel, const u_int index) {
	return index;
}

OpenCLKernels *EmbreeAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize) const {
	// Setup kernels
	return new OpenCLEmbreeKernels(device, kernelCount, this);
}

#else

OpenCLKernels *EmbreeAccel::NewOpenCLKernels(OpenCLIntersectionDevice *device,
		const u_int kernelCount, const u_int stackSize) const {
	return NULL;
}

#endif

}
