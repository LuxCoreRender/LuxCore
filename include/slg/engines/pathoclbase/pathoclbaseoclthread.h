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

#ifndef _SLG_PATHOCLTHREADBASE_H
#define	_SLG_PATHOCLTHREADBASE_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/thread/thread.hpp>

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/ocl.h"

#include "slg/slg.h"
#include "slg/engines/oclrenderengine.h"
#include "slg/engines/pathoclbase/compiledscene.h"

namespace slg {

//------------------------------------------------------------------------------
// OpenCL data types
//------------------------------------------------------------------------------

namespace ocl { namespace pathoclbase {
#include "slg/engines/pathoclbase/kernels/pathoclbase_datatypes.cl"
} }

class PathOCLBaseRenderEngine;

//------------------------------------------------------------------------------
// Path Tracing GPU-only render threads
// (base class for all types of OCL path tracers)
//------------------------------------------------------------------------------

class PathOCLBaseOCLRenderThread {
public:
	PathOCLBaseOCLRenderThread(const u_int index, luxrays::OpenCLIntersectionDevice *device,
			PathOCLBaseRenderEngine *re);
	virtual ~PathOCLBaseOCLRenderThread();

	virtual void Start();
	virtual void Interrupt();
	virtual void Stop();

	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	virtual bool HasDone() const;
	virtual void WaitForDone() const;

	friend class PathOCLBaseRenderEngine;

protected:
	class ThreadFilm {
	public:
		ThreadFilm(PathOCLBaseOCLRenderThread *renderThread);
		virtual ~ThreadFilm();
		
		void Init(Film *engineFilm,
			const u_int threadFilmWidth, const u_int threadFilmHeight,
			const u_int *threadFilmSubRegion);
		void FreeAllOCLBuffers();
		u_int SetFilmKernelArgs(cl::Kernel &filmClearKernel, u_int argIndex) const;
		void ClearFilm(cl::CommandQueue &oclQueue,
			cl::Kernel &filmClearKernel, const size_t filmClearWorkGroupSize);
		void RecvFilm(cl::CommandQueue &oclQueue);
		void SendFilm(cl::CommandQueue &oclQueue);

		Film *film;

		// Film buffers
		std::vector<cl::Buffer *> channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff;
		cl::Buffer *channel_ALPHA_Buff;
		cl::Buffer *channel_DEPTH_Buff;
		cl::Buffer *channel_POSITION_Buff;
		cl::Buffer *channel_GEOMETRY_NORMAL_Buff;
		cl::Buffer *channel_SHADING_NORMAL_Buff;
		cl::Buffer *channel_MATERIAL_ID_Buff;
		cl::Buffer *channel_DIRECT_DIFFUSE_Buff;
		cl::Buffer *channel_DIRECT_GLOSSY_Buff;
		cl::Buffer *channel_EMISSION_Buff;
		cl::Buffer *channel_INDIRECT_DIFFUSE_Buff;
		cl::Buffer *channel_INDIRECT_GLOSSY_Buff;
		cl::Buffer *channel_INDIRECT_SPECULAR_Buff;
		cl::Buffer *channel_MATERIAL_ID_MASK_Buff;
		cl::Buffer *channel_DIRECT_SHADOW_MASK_Buff;
		cl::Buffer *channel_INDIRECT_SHADOW_MASK_Buff;
		cl::Buffer *channel_UV_Buff;
		cl::Buffer *channel_RAYCOUNT_Buff;
		cl::Buffer *channel_BY_MATERIAL_ID_Buff;
		cl::Buffer *channel_IRRADIANCE_Buff;
		cl::Buffer *channel_OBJECT_ID_Buff;
		cl::Buffer *channel_OBJECT_ID_MASK_Buff;
		cl::Buffer *channel_BY_OBJECT_ID_Buff;
		cl::Buffer *channel_SAMPLECOUNT_Buff;
		cl::Buffer *channel_CONVERGENCE_Buff;
		cl::Buffer *channel_MATERIAL_ID_COLOR_Buff;
		
		// Denoiser sample accumulator buffers
		cl::Buffer *denoiser_NbOfSamplesImage_Buff;
		cl::Buffer *denoiser_SquaredWeightSumsImage_Buff;
		cl::Buffer *denoiser_MeanImage_Buff;
		cl::Buffer *denoiser_CovarImage_Buff;
		cl::Buffer *denoiser_HistoImage_Buff;

	private:
		Film *engineFilm;
		PathOCLBaseOCLRenderThread *renderThread;
	};

	std::string SamplerKernelDefinitions();

	// Implementation specific methods
	virtual void RenderThreadImpl() = 0;
	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight, u_int *filmSubRegion) = 0;

	virtual void AdditionalInit() { }
	virtual std::string AdditionalKernelOptions() { return ""; }
	virtual std::string AdditionalKernelDefinitions() { return ""; }
	virtual std::string AdditionalKernelSources() { return ""; }
	virtual void CompileAdditionalKernels(cl::Program *program) { }

	void AllocOCLBuffer(const cl_mem_flags clFlags, cl::Buffer **buff,
			void *src, const size_t size, const std::string &desc);
	void AllocOCLBufferRO(cl::Buffer **buff, void *src, const size_t size, const std::string &desc);
	void AllocOCLBufferRW(cl::Buffer **buff, const size_t size, const std::string &desc);
	void FreeOCLBuffer(cl::Buffer **buff);

	virtual void StartRenderThread();
	virtual void StopRenderThread();

	void IncThreadFilms();
	void ClearThreadFilms(cl::CommandQueue &oclQueue);
	void TransferThreadFilms(cl::CommandQueue &oclQueue);
	void FreeThreadFilmsOCLBuffers();
	void FreeThreadFilms();

	void InitRender();

	void InitFilm();
	void InitCamera();
	void InitGeometry();
	void InitImageMaps();
	void InitTextures();
	void InitMaterials();
	void InitSceneObjects();
	void InitLights();
	void InitKernels();
	void InitGPUTaskBuffer();
	void InitSamplerSharedDataBuffer();
	void InitSamplesBuffer();
	void InitSampleDataBuffer();

	void SetInitKernelArgs(const u_int filmIndex);
	void SetAdvancePathsKernelArgs(cl::Kernel *advancePathsKernel, const u_int filmIndex);
	void SetAllAdvancePathsKernelArgs(const u_int filmIndex);
	void SetKernelArgs();

	void CompileKernel(cl::Program *program, cl::Kernel **kernel, size_t *workgroupSize, const std::string &name);

	void EnqueueAdvancePathsKernel(cl::CommandQueue &oclQueue);

	// OpenCL structure size
	size_t GetOpenCLHitPointSize() const;
	size_t GetOpenCLBSDFSize() const;
	size_t GetOpenCLSampleResultSize() const;

	u_int threadIndex;
	luxrays::OpenCLIntersectionDevice *intersectionDevice;
	PathOCLBaseRenderEngine *renderEngine;

	// OpenCL variables
	std::string kernelSrcHash;
	cl::Kernel *filmClearKernel;
	size_t filmClearWorkGroupSize;

	// Scene buffers
	cl::Buffer *materialsBuff;
	cl::Buffer *texturesBuff;
	cl::Buffer *meshIDBuff;
	cl::Buffer *meshDescsBuff;
	cl::Buffer *scnObjsBuff;
	cl::Buffer *lightsBuff;
	cl::Buffer *envLightIndicesBuff;
	cl::Buffer *lightsDistributionBuff;
	cl::Buffer *infiniteLightSourcesDistributionBuff;
	cl::Buffer *envLightDistributionsBuff;
	cl::Buffer *vertsBuff;
	cl::Buffer *normalsBuff;
	cl::Buffer *uvsBuff;
	cl::Buffer *colsBuff;
	cl::Buffer *alphasBuff;
	cl::Buffer *trianglesBuff;
	cl::Buffer *cameraBuff;
	cl::Buffer *lightIndexOffsetByMeshIndexBuff;
	cl::Buffer *lightIndexByTriIndexBuff;
	cl::Buffer *imageMapDescsBuff;
	std::vector<cl::Buffer *> imageMapsBuff;
	cl::Buffer *raysBuff;
	cl::Buffer *hitsBuff;
	cl::Buffer *tasksBuff;
	cl::Buffer *tasksDirectLightBuff;
	cl::Buffer *tasksStateBuff;
	cl::Buffer *samplerSharedDataBuff;
	cl::Buffer *samplesBuff;
	cl::Buffer *sampleDataBuff;
	cl::Buffer *taskStatsBuff;
	cl::Buffer *pathVolInfosBuff;
	cl::Buffer *directLightVolInfosBuff;
	cl::Buffer *pixelFilterBuff;

	u_int initKernelArgsCount;
	std::string kernelsParameters;
	luxrays::oclKernelCache *kernelCache;

	boost::thread *renderThread;

	std::vector<ThreadFilm *> threadFilms;

	// OpenCL kernels
	cl::Kernel *initSeedKernel;
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

	u_int sampleDimensions;

	slg::ocl::pathoclbase::GPUTaskStats *gpuTaskStats;

	bool started, editMode, threadDone;
};

}

#endif

#endif	/* _SLG_PATHOCLTHREADBASE_H */
