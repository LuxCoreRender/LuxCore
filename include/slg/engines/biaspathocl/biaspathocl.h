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
	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight, u_int *filmSubRegion);
	virtual void AdditionalInit();
	virtual std::string AdditionalKernelOptions();
	virtual std::string AdditionalKernelDefinitions();
	virtual std::string AdditionalKernelSources();
	virtual void SetAdditionalKernelArgs();
	virtual void CompileAdditionalKernels(cl::Program *program);

	virtual void Stop();
	
	void SetRenderSampleKernelArgs(cl::Kernel *renderSampleKernel, bool firstKernel);
	void UpdateKernelArgsForTile(const TileRepository::Tile *tile, const u_int filmIndex);
	void EnqueueRenderSampleKernel(cl::CommandQueue &oclQueue);

	// OpenCL variables
	cl::Kernel *initSeedKernel;
	size_t initSeedWorkGroupSize;
	cl::Kernel *initStatKernel;
	size_t initStatWorkGroupSize;
	cl::Kernel *mergePixelSamplesKernel;
	size_t mergePixelSamplesWorkGroupSize;
	
	cl::Kernel *renderSampleKernel_MK_GENERATE_CAMERA_RAY;
	cl::Kernel *renderSampleKernel_MK_TRACE_EYE_RAY;
	cl::Kernel *renderSampleKernel_MK_ILLUMINATE_EYE_MISS;
	cl::Kernel *renderSampleKernel_MK_ILLUMINATE_EYE_HIT;
	cl::Kernel *renderSampleKernel_MK_DL_VERTEX_1;
	cl::Kernel *renderSampleKernel_MK_BSDF_SAMPLE_DIFFUSE;
	cl::Kernel *renderSampleKernel_MK_BSDF_SAMPLE_GLOSSY;
	cl::Kernel *renderSampleKernel_MK_BSDF_SAMPLE_SPECULAR;
	size_t renderSampleWorkGroupSize;

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
	BiasPathOCLRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~BiasPathOCLRenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	void GetPendingTiles(std::deque<const TileRepository::Tile *> &tiles) { return tileRepository->GetPendingTiles(tiles); }
	void GetNotConvergedTiles(std::deque<const TileRepository::Tile *> &tiles) { return tileRepository->GetNotConvergedTiles(tiles); }
	void GetConvergedTiles(std::deque<const TileRepository::Tile *> &tiles) { return tileRepository->GetConvergedTiles(tiles); }
	u_int GetTileWidth() const { return tileRepository->tileWidth; }
	u_int GetTileHeight() const { return tileRepository->tileHeight; }

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return BIASPATHOCL; }
	static std::string GetObjectTag() { return "BIASPATHOCL"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex);

	friend class BiasPathOCLRenderThread;

	// Path depth settings
	PathDepthInfo maxPathDepth;

	// Samples settings
	u_int aaSamples, diffuseSamples, glossySamples, specularSamples, directLightSamples;

	// Clamping settings
	float sqrtVarianceClampMaxValue;
	float pdfClampValue;

	// Light settings
	float lowLightThreashold, nearStartLight;
	u_int firstVertexLightSampleCount;

	u_int maxTilePerDevice;

protected:
	static const luxrays::Properties &GetDefaultProps();

	void PrintSamplesInfo() const;

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
