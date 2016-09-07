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

#ifndef _SLG_PATHOCLSTATEBASE_H
#define	_SLG_PATHOCLSTATEBASE_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/engines/pathoclbase/pathoclstatebase_datatypes.h"

namespace slg {

class PathOCLStateKernelBaseRenderEngine;

//------------------------------------------------------------------------------
// Path Tracing GPU-only render threads using state micro-kernels
//------------------------------------------------------------------------------

class PathOCLStateKernelBaseRenderThread : public PathOCLBaseRenderThread {
public:
	PathOCLStateKernelBaseRenderThread(const u_int index, luxrays::OpenCLIntersectionDevice *device,
			PathOCLStateKernelBaseRenderEngine *re);
	virtual ~PathOCLStateKernelBaseRenderThread();

	virtual void Stop();

	friend class PathOCLStateKernelBaseRenderEngine;

protected:
	virtual void AdditionalInit();
	virtual std::string AdditionalKernelOptions();
	virtual std::string AdditionalKernelDefinitions();
	virtual std::string AdditionalKernelSources();
	virtual void SetAdditionalKernelArgs();
	virtual void CompileAdditionalKernels(cl::Program *program);

	void InitGPUTaskBuffer();
	void InitSamplesBuffer();
	void InitSampleDataBuffer();
	void SetAdvancePathsKernelArgs(cl::Kernel *advancePathsKernel);
	void EnqueueAdvancePathsKernel(cl::CommandQueue &oclQueue);

	u_int sampleDimensions;

	// OpenCL variables
	cl::Kernel *initKernel;
	size_t initWorkGroupSize;
	cl::Kernel *advancePathsKernel_MK_RT_NEXT_VERTEX;
	cl::Kernel *advancePathsKernel_MK_HIT_NOTHING;
	cl::Kernel *advancePathsKernel_MK_HIT_OBJECT;
	cl::Kernel *advancePathsKernel_MK_RT_DL;
	cl::Kernel *advancePathsKernel_MK_DL_ILLUMINATE;
	cl::Kernel *advancePathsKernel_MK_DL_SAMPLE_BSDF;
	cl::Kernel *advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY;
	cl::Kernel *advancePathsKernel_MK_SPLAT_SAMPLE;
	cl::Kernel *advancePathsKernel_MK_NEXT_SAMPLE;
	cl::Kernel *advancePathsKernel_MK_GENERATE_CAMERA_RAY;
	size_t advancePathsWorkGroupSize;

	cl::Buffer *raysBuff;
	cl::Buffer *hitsBuff;
	cl::Buffer *tasksBuff;
	cl::Buffer *tasksDirectLightBuff;
	cl::Buffer *tasksStateBuff;
	cl::Buffer *samplesBuff;
	cl::Buffer *sampleDataBuff;
	cl::Buffer *taskStatsBuff;
	cl::Buffer *pathVolInfosBuff;
	cl::Buffer *directLightVolInfosBuff;
	cl::Buffer *pixelFilterBuff;

	slg::ocl::pathoclstatebase::GPUTaskStats *gpuTaskStats;
};

//------------------------------------------------------------------------------
// Path Tracing 100% OpenCL render engine using state micro-kernels
//------------------------------------------------------------------------------

class PathOCLStateKernelBaseRenderEngine : public PathOCLBaseRenderEngine {
public:
	PathOCLStateKernelBaseRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~PathOCLStateKernelBaseRenderEngine();

	friend class PathOCLStateKernelBaseRenderThread;

	// Path depth settings
	//PathDepthInfo maxPathDepth;
	int maxPathDepth;

	// Clamping settings
	float sqrtVarianceClampMaxValue;
	float pdfClampValue;

	int rrDepth;
	float rrImportanceCap;

	u_int taskCount;
	bool usePixelAtomics, useFastPixelFilter, forceBlackBackground;

protected:
	void InitPixelFilterDistribution();

	virtual void StartLockLess();
	virtual void StopLockLess();

	// Pixel filter related variables
	float *pixelFilterDistribution;
	u_int pixelFilterDistributionSize;

	slg::ocl::Sampler *oclSampler;
	slg::ocl::Filter *oclPixelFilter;
};

}

#endif

#endif	/* _SLG_PATHOCLSTATEBASE_H */
