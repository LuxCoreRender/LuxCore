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

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/utils/thread.h"
#include "luxrays/usings.h"

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
	PathOCLBaseOCLRenderThread(const u_int index, luxrays::HardwareIntersectionDeviceRef device,
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

	class ThreadFilm {
	public:
		ThreadFilm(PathOCLBaseOCLRenderThread *renderThread);
		virtual ~ThreadFilm();

		void Init(FilmRef engineFilm,
			const u_int threadFilmWidth, const u_int threadFilmHeight,
			const u_int *threadFilmSubRegion);
		void FreeAllOCLBuffers();
		u_int SetFilmKernelArgs(
			luxrays::HardwareIntersectionDeviceRef intersectionDevice,
			luxrays::HardwareDeviceKernelRPtr filmClearKernel,
			u_int argIndex) const;
		void ClearFilm(luxrays::HardwareIntersectionDeviceRef intersectionDevice,
			luxrays::HardwareDeviceKernelRPtr filmClearKernel,
			const size_t filmClearWorkGroupSize);
		void RecvFilm(luxrays::HardwareIntersectionDeviceRef intersectionDevice);
		void SendFilm(luxrays::HardwareIntersectionDeviceRef intersectionDevice);

		FilmRef GetFilm() { return *film; }
		FilmConstRef GetFilm() const { return *film; }

		// Film buffers
		std::vector<luxrays::HardwareDeviceBuffer *> channel_RADIANCE_PER_PIXEL_NORMALIZEDs_Buff;
		luxrays::HardwareDeviceBuffer *channel_ALPHA_Buff;
		luxrays::HardwareDeviceBuffer *channel_DEPTH_Buff;
		luxrays::HardwareDeviceBuffer *channel_POSITION_Buff;
		luxrays::HardwareDeviceBuffer *channel_GEOMETRY_NORMAL_Buff;
		luxrays::HardwareDeviceBuffer *channel_SHADING_NORMAL_Buff;
		luxrays::HardwareDeviceBuffer *channel_MATERIAL_ID_Buff;
		luxrays::HardwareDeviceBuffer *channel_DIRECT_DIFFUSE_Buff;
		luxrays::HardwareDeviceBuffer *channel_DIRECT_DIFFUSE_REFLECT_Buff;
		luxrays::HardwareDeviceBuffer *channel_DIRECT_DIFFUSE_TRANSMIT_Buff;
		luxrays::HardwareDeviceBuffer *channel_DIRECT_GLOSSY_Buff;
		luxrays::HardwareDeviceBuffer *channel_DIRECT_GLOSSY_REFLECT_Buff;
		luxrays::HardwareDeviceBuffer *channel_DIRECT_GLOSSY_TRANSMIT_Buff;
		luxrays::HardwareDeviceBuffer *channel_EMISSION_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_DIFFUSE_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_DIFFUSE_REFLECT_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_DIFFUSE_TRANSMIT_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_GLOSSY_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_GLOSSY_REFLECT_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_GLOSSY_TRANSMIT_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_SPECULAR_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_SPECULAR_REFLECT_Buff;
		luxrays::HardwareDeviceBuffer *channel_INDIRECT_SPECULAR_TRANSMIT_Buff;
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
		FilmUPtr film;
		FilmPtr engineFilm;
		PathOCLBaseOCLRenderThread *renderThread;
	};

protected:
	// Implementation specific methods
	virtual void RenderThreadImpl(std::stop_token stop_token) = 0;
	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight, u_int *filmSubRegion) = 0;

	virtual void StartRenderThread();
	virtual void StopRenderThread();

	void IncThreadFilms();
	void ClearThreadFilms();
	void TransferThreadFilms(luxrays::HardwareIntersectionDeviceRef intersectionDevice);
	void FreeThreadFilmsOCLBuffers();
	void FreeThreadFilms();

	void InitRender();

	void InitFilm();
	void InitCamera();
	void InitGeometry();
	void InitImageMaps();
	void InitTextures();
	void InitMaterials();
	// True when the kernels keep the material and texture evaluation stacks
	// in CUDA local memory (see evalstack_funcs.cl), sized at compile time
	// for the scene: the global stack buffers are then not used.
	bool UseLocalEvalStacks() const;
	void InitSceneObjects();
	void InitLights();
	void InitPhotonGI();
	void InitKernels();
	void InitGPUTaskBuffer();
	// Uploads renderEngine->taskConfig
	void UpdateGPUTaskConfiguration();
	void InitSamplerSharedDataBuffer();
	void InitSamplesBuffer();
	void InitSampleDataBuffer();
	void InitSampleResultsBuffer();

	void SetInitKernelArgs(const u_int filmIndex);
	void SetAdvancePathsKernelArgs(luxrays::HardwareDeviceKernelRPtr advancePathsKernel, const u_int filmIndex);
	void SetAllAdvancePathsKernelArgs(const u_int filmIndex);
	void SetKernelArgs();


	std::tuple<luxrays::HardwareDeviceKernelUPtr, size_t> CompileKernel(
		luxrays::HardwareIntersectionDeviceRef device,
		luxrays::HardwareDeviceProgramRef program,
		const std::string &name
	);

	void EnqueueInitKernel();
	void EnqueueAdvancePathsKernel();
	// With opencl.kernel.profile.enable, waits for the queue and returns the
	// time; EndKernelProfile() waits again and adds the elapsed time to
	// kernelProfileTimes[index]. Both do nothing when profiling is off.
	double BeginKernelProfile();
	void EndKernelProfile(const u_int index, const double startTime);
	void LogKernelProfile() const;

	static luxrays::oclKernelCache *AllocKernelCache(const std::string &type);
	static void GetKernelParamters(std::vector<std::string> &params,
			luxrays::HardwareIntersectionDeviceRef intersectionDevice,
			const std::string renderEngineType,
			const float epsilonMin, const float epsilonMax);
	static std::string GetKernelSources();

	u_int threadIndex;
	luxrays::HardwareIntersectionDeviceRef intersectionDevice;
	PathOCLBaseRenderEngine *renderEngine;

	// OpenCL variables
	std::string kernelSrcHash;
	luxrays::HardwareDeviceKernelUPtr filmClearKernel;
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
	luxrays::HardwareDeviceBuffer *cameraBokehDistributionBuff;
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
	luxrays::HardwareDeviceBuffer *pathStatesBuff;
	luxrays::HardwareDeviceBuffer *pathQueuesBuff;
	luxrays::HardwareDeviceBuffer *pathQueueCountsBuff;
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

	luxrays::JThreadUPtr renderThread;

	std::vector<std::shared_ptr<ThreadFilm> > threadFilms;

	// OpenCL kernels
	luxrays::HardwareDeviceKernelUPtr initSeedKernel;
	luxrays::HardwareDeviceKernelUPtr initKernel;
	luxrays::HardwareDeviceKernelUPtr resetPathQueuesKernel;
	size_t resetPathQueuesWorkGroupSize;
	luxrays::HardwareDeviceKernelUPtr queuePathsKernel;
	size_t queuePathsWorkGroupSize;
	size_t initWorkGroupSize;
	luxrays::HardwareDeviceKernelUPtr advancePathsKernel_MK_RT_NEXT_VERTEX;
	luxrays::HardwareDeviceKernelUPtr advancePathsKernel_MK_HIT_NOTHING;
	luxrays::HardwareDeviceKernelUPtr advancePathsKernel_MK_HIT_OBJECT;
	luxrays::HardwareDeviceKernelUPtr advancePathsKernel_MK_RT_DL;
	luxrays::HardwareDeviceKernelUPtr advancePathsKernel_MK_DL_ILLUMINATE;
	luxrays::HardwareDeviceKernelUPtr advancePathsKernel_MK_DL_SAMPLE_BSDF;
	luxrays::HardwareDeviceKernelUPtr advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY;
	luxrays::HardwareDeviceKernelUPtr advancePathsKernel_MK_SPLAT_SAMPLE;
	luxrays::HardwareDeviceKernelUPtr advancePathsKernel_MK_NEXT_SAMPLE;
	luxrays::HardwareDeviceKernelUPtr advancePathsKernel_MK_GENERATE_CAMERA_RAY;
	// Indexed by the PathState of the micro-kernel
	size_t advancePathsWorkGroupSizes[slg::ocl::pathoclbase::MK_DONE];

	// Profiling slots: one per micro-kernel (indexed by PathState), then
	// the ray trace and the queue reset
	static const u_int KERNEL_PROFILE_TRACE = slg::ocl::pathoclbase::MK_DONE;
	static const u_int KERNEL_PROFILE_PATH_QUEUES = slg::ocl::pathoclbase::MK_DONE + 1;
	static const u_int KERNEL_PROFILE_COUNT = slg::ocl::pathoclbase::MK_DONE + 2;
	double kernelProfileTimes[KERNEL_PROFILE_COUNT] = {};

	std::unique_ptr<slg::ocl::pathoclbase::GPUTaskStats[]> gpuTaskStats;

	bool started, editMode, threadDone;
};


// These usings cannot be gathered in slg/usings.h, as C++ does not allow
// forward declarations on nested classes
using ThreadFilmRPtr = std::shared_ptr<PathOCLBaseOCLRenderThread::ThreadFilm>;
using ThreadFilmConstPtr = std::shared_ptr<const PathOCLBaseOCLRenderThread::ThreadFilm>;

}

#endif

#endif	/* _SLG_PATHOCLTHREADBASE_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
