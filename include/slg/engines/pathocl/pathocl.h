/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
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

	// OpenCL variables
	cl::Kernel *initKernel;
	size_t initWorkGroupSize;
	cl::Kernel *advancePathsKernel;
	size_t advancePathsWorkGroupSize;

	cl::Buffer *raysBuff;
	cl::Buffer *hitsBuff;
	cl::Buffer *tasksBuff;
	cl::Buffer *samplesBuff;
	cl::Buffer *sampleDataBuff;
	cl::Buffer *taskStatsBuff;

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

	u_int taskCount;
	bool usePixelAtomics;

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
