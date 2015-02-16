/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_PATHOCL_H
#define	_SLG_PATHOCL_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/thread/thread.hpp>

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/ocl.h"

#include "slg/slg.h"
#include "slg/renderengine.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/engines/pathocl/pathocl_datatypes.h"

namespace slg {

class PathOCLRenderEngine;

//------------------------------------------------------------------------------
// Path Tracing GPU-only render threads
//------------------------------------------------------------------------------

class PathOCLRenderThread : public PathOCLBaseRenderThread {
public:
	PathOCLRenderThread(const u_int index, luxrays::OpenCLIntersectionDevice *device,
			PathOCLRenderEngine *re);
	virtual ~PathOCLRenderThread();

	virtual void Stop();

	friend class PathOCLRenderEngine;

protected:
	virtual void RenderThreadImpl();
	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight);
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

	// OpenCL variables
	cl::Kernel *initKernel;
	size_t initWorkGroupSize;
	cl::Kernel *advancePathsKernel;
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

	u_int sampleDimensions;

	slg::ocl::pathocl::GPUTaskStats *gpuTaskStats;
};

//------------------------------------------------------------------------------
// Path Tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class PathOCLRenderEngine : public PathOCLBaseRenderEngine {
public:
	PathOCLRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex,
			const bool realTime = false);
	virtual ~PathOCLRenderEngine();

	virtual RenderEngineType GetEngineType() const { return PATHOCL; }

	friend class PathOCLRenderThread;

	// Signed because of the delta parameter
	int maxPathDepth;

	int rrDepth;
	float rrImportanceCap;

	// Clamping settings
	float radianceClampMaxValue;
	float pdfClampValue;

	u_int taskCount;
	bool usePixelAtomics, useMicroKernels;

protected:
	virtual PathOCLRenderThread *CreateOCLThread(const u_int index, luxrays::OpenCLIntersectionDevice *device);

	virtual void StartLockLess();

	virtual void UpdateFilmLockLess();
	virtual void UpdateCounters();

	slg::ocl::Sampler *sampler;
	slg::ocl::Filter *filter;
};

}

#endif

#endif	/* _SLG_PATHOCL_H */
