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
		void RecvFilm(luxrays::OpenCLIntersectionDevice *intersectionDevice);
		void SendFilm(luxrays::OpenCLIntersectionDevice *intersectionDevice);

		Film *film;

		// Film buffers
		std::vector<luxrays::HardwareDeviceBuffer *> channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff;
		luxrays::HardwareDeviceBuffer *channel_ALPHA_Buff;
		luxrays::HardwareDeviceBuffer *channel_DEPTH_Buff;
		luxrays::HardwareDeviceBuffer *channel_POSITION_Buff;
		luxrays::HardwareDeviceBuffer *channel_GEOMETRY_NORMAL_Buff;
		luxrays::HardwareDeviceBuffer *channel_SHADING_NORMAL_Buff;
		luxrays::HardwareDeviceBuffer *channel_MATERIAL_ID_Buff;
		luxrays::HardwareDeviceBuffer *channel_DIRECT_DIFFUSE_Buff;
		luxrays::HardwareDeviceBuffer *channel_DIRECT_GLOSSY_Buff;
		luxrays::HardwareDeviceBuffer *channel_EMISSION_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_DIFFUSE_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_GLOSSY_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_SPECULAR_Buff;
		luxrays::HardwareDeviceBuffer *channel_MATERIAL_ID_MASK_Buff;
		luxrays::HardwareDeviceBuffer *channel_DIRECT_SHADOW_MASK_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_SHADOW_MASK_Buff;
		luxrays::HardwareDeviceBuffer *channel_UV_Buff;
		luxrays::HardwareDeviceBuffer *channel_RAYCOUNT_Buff;
		luxrays::HardwareDeviceBuffer *channel_BY_MATERIAL_ID_Buff;
		luxrays::HardwareDeviceBuffer *channel_IRRADIANCE_Buff;
		luxrays::HardwareDeviceBuffer *channel_OBJECT_ID_Buff;
		luxrays::HardwareDeviceBuffer *channel_OBJECT_ID_MASK_Buff;
		luxrays::HardwareDeviceBuffer *channel_BY_OBJECT_ID_Buff;
		luxrays::HardwareDeviceBuffer *channel_SAMPLECOUNT_Buff;
		luxrays::HardwareDeviceBuffer *channel_CONVERGENCE_Buff;
		luxrays::HardwareDeviceBuffer *channel_MATERIAL_ID_COLOR_Buff;
		luxrays::HardwareDeviceBuffer *channel_ALBEDO_Buff;
		luxrays::HardwareDeviceBuffer *channel_AVG_SHADING_NORMAL_Buff;
		luxrays::HardwareDeviceBuffer *channel_NOISE_Buff;
		luxrays::HardwareDeviceBuffer *channel_USER_IMPORTANCE_Buff;
		
		// Denoiser sample accumulator buffers
		luxrays::HardwareDeviceBuffer *denoiser_NbOfSamplesImage_Buff;
		luxrays::HardwareDeviceBuffer *denoiser_SquaredWeightSumsImage_Buff;
		luxrays::HardwareDeviceBuffer *denoiser_MeanImage_Buff;
		luxrays::HardwareDeviceBuffer *denoiser_CovarImage_Buff;
		luxrays::HardwareDeviceBuffer *denoiser_HistoImage_Buff;

	private:
		Film *engineFilm;
		PathOCLBaseOCLRenderThread *renderThread;
	};

	// Implementation specific methods
	virtual void RenderThreadImpl() = 0;
	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight, u_int *filmSubRegion) = 0;

	virtual void StartRenderThread();
	virtual void StopRenderThread();

	void IncThreadFilms();
	void ClearThreadFilms(cl::CommandQueue &oclQueue);
	void TransferThreadFilms(luxrays::OpenCLIntersectionDevice *intersectionDevice);
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
	void InitPhotonGI();
	void InitKernels();
	void InitGPUTaskBuffer();
	void InitSamplerSharedDataBuffer();
	void InitSamplesBuffer();
	void InitSampleDataBuffer();
	void InitSampleResultsBuffer();

	void SetInitKernelArgs(const u_int filmIndex);
	void SetAdvancePathsKernelArgs(cl::Kernel *advancePathsKernel, const u_int filmIndex);
	void SetAllAdvancePathsKernelArgs(const u_int filmIndex);
	void SetKernelArgs();

	void CompileKernel(cl::Program *program, cl::Kernel **kernel, size_t *workgroupSize, const std::string &name);

	void EnqueueAdvancePathsKernel(cl::CommandQueue &oclQueue);

	static luxrays::oclKernelCache *AllocKernelCache(const std::string &type);
	static std::string GetKernelParamters(luxrays::OpenCLIntersectionDevice *intersectionDevice,
			const std::string renderEngineType,
			const float epsilonMin, const float epsilonMax,
			const bool usePixelAtomics);
	static std::string GetKernelSources();

	u_int threadIndex;
	luxrays::OpenCLIntersectionDevice *intersectionDevice;
	PathOCLBaseRenderEngine *renderEngine;

	// OpenCL variables
	std::string kernelSrcHash;
	cl::Kernel *filmClearKernel;
	size_t filmClearWorkGroupSize;

	// Scene buffers
	luxrays::HardwareDeviceBuffer *materialsBuff;
	luxrays::HardwareDeviceBuffer *materialEvalOpsBuff;
	luxrays::HardwareDeviceBuffer *materialEvalStackBuff;
	luxrays::HardwareDeviceBuffer *texturesBuff;
	luxrays::HardwareDeviceBuffer *textureEvalOpsBuff;
	luxrays::HardwareDeviceBuffer *textureEvalStackBuff;
	luxrays::HardwareDeviceBuffer *meshIDBuff;
	luxrays::HardwareDeviceBuffer *meshDescsBuff;
	luxrays::HardwareDeviceBuffer *scnObjsBuff;
	luxrays::HardwareDeviceBuffer *lightsBuff;
	luxrays::HardwareDeviceBuffer *envLightIndicesBuff;
	luxrays::HardwareDeviceBuffer *lightsDistributionBuff;
	luxrays::HardwareDeviceBuffer *infiniteLightSourcesDistributionBuff;
	luxrays::HardwareDeviceBuffer *dlscAllEntriesBuff;
	luxrays::HardwareDeviceBuffer *dlscDistributionsBuff;
	luxrays::HardwareDeviceBuffer *dlscBVHNodesBuff;
	luxrays::HardwareDeviceBuffer *elvcAllEntriesBuff;
	luxrays::HardwareDeviceBuffer *elvcDistributionsBuff;
	luxrays::HardwareDeviceBuffer *elvcTileDistributionOffsetsBuff;
	luxrays::HardwareDeviceBuffer *elvcBVHNodesBuff;
	luxrays::HardwareDeviceBuffer *envLightDistributionsBuff;
	luxrays::HardwareDeviceBuffer *vertsBuff;
	luxrays::HardwareDeviceBuffer *normalsBuff;
	luxrays::HardwareDeviceBuffer *triNormalsBuff;
	luxrays::HardwareDeviceBuffer *uvsBuff;
	luxrays::HardwareDeviceBuffer *colsBuff;
	luxrays::HardwareDeviceBuffer *alphasBuff;
	luxrays::HardwareDeviceBuffer *vertexAOVBuff;
	luxrays::HardwareDeviceBuffer *triAOVBuff;
	luxrays::HardwareDeviceBuffer *trianglesBuff;
	luxrays::HardwareDeviceBuffer *interpolatedTransformsBuff;
	luxrays::HardwareDeviceBuffer *cameraBuff;
	luxrays::HardwareDeviceBuffer *lightIndexOffsetByMeshIndexBuff;
	luxrays::HardwareDeviceBuffer *lightIndexByTriIndexBuff;
	luxrays::HardwareDeviceBuffer *imageMapDescsBuff;
	std::vector<luxrays::HardwareDeviceBuffer *> imageMapsBuff;
	luxrays::HardwareDeviceBuffer *pgicRadiancePhotonsBuff;
	luxrays::HardwareDeviceBuffer *pgicRadiancePhotonsValuesBuff;
	luxrays::HardwareDeviceBuffer *pgicRadiancePhotonsBVHNodesBuff;
	luxrays::HardwareDeviceBuffer *pgicCausticPhotonsBuff;
	luxrays::HardwareDeviceBuffer *pgicCausticPhotonsBVHNodesBuff;

	// OpenCL task related buffers
	luxrays::HardwareDeviceBuffer *raysBuff;
	luxrays::HardwareDeviceBuffer *hitsBuff;
	luxrays::HardwareDeviceBuffer *taskConfigBuff;
	luxrays::HardwareDeviceBuffer *tasksBuff;
	luxrays::HardwareDeviceBuffer *tasksDirectLightBuff;
	luxrays::HardwareDeviceBuffer *tasksStateBuff;
	luxrays::HardwareDeviceBuffer *samplerSharedDataBuff;
	luxrays::HardwareDeviceBuffer *samplesBuff;
	luxrays::HardwareDeviceBuffer *sampleDataBuff;
	luxrays::HardwareDeviceBuffer *sampleResultsBuff;
	luxrays::HardwareDeviceBuffer *taskStatsBuff;
	luxrays::HardwareDeviceBuffer *eyePathInfosBuff;
	luxrays::HardwareDeviceBuffer *directLightVolInfosBuff;
	luxrays::HardwareDeviceBuffer *pixelFilterBuff;

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

	slg::ocl::pathoclbase::GPUTaskStats *gpuTaskStats;

	bool started, editMode, threadDone;
};

}

#endif

#endif	/* _SLG_PATHOCLTHREADBASE_H */
