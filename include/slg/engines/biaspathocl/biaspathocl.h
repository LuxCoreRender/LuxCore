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

#ifndef _SLG_BIASPATHOCL_H
#define	_SLG_BIASPATHOCL_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/engines/biaspathcpu/biaspathcpu.h"
#include "slg/engines/biaspathocl/biaspathocl_datatypes.h"

namespace slg {

class BiasPathOCLRenderEngine;

//------------------------------------------------------------------------------
// Biased path tracing GPU-only render threads
//------------------------------------------------------------------------------

class BiasPathOCLRenderThread : public PathOCLBaseRenderThread {
public:
	BiasPathOCLRenderThread(const u_int index, luxrays::OpenCLIntersectionDevice *device,
			BiasPathOCLRenderEngine *re);
	virtual ~BiasPathOCLRenderThread();

	friend class BiasPathOCLRenderEngine;

protected:
	virtual void RenderThreadImpl();
	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight);
	virtual void AdditionalInit();
	virtual std::string AdditionalKernelOptions();
	virtual std::string AdditionalKernelDefinitions();
	virtual std::string AdditionalKernelSources();
	virtual void SetAdditionalKernelArgs();
	virtual void CompileAdditionalKernels(cl::Program *program);

	virtual void Stop();

	// OpenCL variables
	cl::Kernel *initSeedKernel;
	size_t initSeedWorkGroupSize;
	cl::Kernel *initStatKernel;
	size_t initStatWorkGroupSize;
	cl::Kernel *renderSampleKernel;
	size_t renderSampleWorkGroupSize;
	cl::Kernel *mergePixelSamplesKernel;
	size_t mergePixelSamplesWorkGroupSize;

	cl::Buffer *tasksBuff;
	cl::Buffer *tasksDirectLightBuff;
	cl::Buffer *tasksPathVertexNBuff;
	cl::Buffer *taskStatsBuff;
	cl::Buffer *taskResultsBuff;
	cl::Buffer *pixelFilterBuff;

	u_int sampleDimensions;

	slg::ocl::biaspathocl::GPUTaskStats *gpuTaskStats;
};

//------------------------------------------------------------------------------
// Biased path tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class BiasPathOCLRenderEngine : public PathOCLBaseRenderEngine {
public:
	BiasPathOCLRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex,
			const bool realTime = false);
	virtual ~BiasPathOCLRenderEngine();

	virtual RenderEngineType GetEngineType() const { return BIASPATHOCL; }

	void GetPendingTiles(std::deque<TileRepository::Tile *> &tiles) { return tileRepository->GetPendingTiles(tiles); }
	void GetNotConvergedTiles(std::deque<TileRepository::Tile *> &tiles) { return tileRepository->GetNotConvergedTiles(tiles); }
	void GetConvergedTiles(std::deque<TileRepository::Tile *> &tiles) { return tileRepository->GetConvergedTiles(tiles); }
	u_int GetTileSize() const { return tileRepository->tileSize; }

	friend class BiasPathOCLRenderThread;

	// Path depth settings
	PathDepthInfo maxPathDepth;

	// Samples settings
	u_int aaSamples, diffuseSamples, glossySamples, specularSamples, directLightSamples;

	// Clamping settings
	float radianceClampMaxValue;
	float pdfClampValue;

	// Light settings
	float lowLightThreashold, nearStartLight;
	u_int firstVertexLightSampleCount;

protected:
	virtual PathOCLBaseRenderThread *CreateOCLThread(const u_int index,
		luxrays::OpenCLIntersectionDevice *device);

	virtual void StartLockLess();
	virtual void StopLockLess();
	virtual void EndSceneEditLockLess(const EditActionList &editActions);
	virtual void UpdateFilmLockLess() { }
	virtual void UpdateCounters();

	void InitPixelFilterDistribution();

	u_int taskCount;
	float *pixelFilterDistribution;
	u_int pixelFilterDistributionSize;


	TileRepository *tileRepository;
	bool printedRenderingTime;
};

}

#endif

#endif	/* _SLG_BIASPATHOCL_H */
