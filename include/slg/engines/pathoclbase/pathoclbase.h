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

#ifndef _SLG_PATHOCLBASE_H
#define	_SLG_PATHOCLBASE_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/ocl.h"

#include "slg/slg.h"
#include "slg/renderengine.h"
#include "slg/engines/pathoclbase/compiledscene.h"

#include <boost/thread/thread.hpp>

namespace slg {

class PathOCLBaseRenderEngine;

//------------------------------------------------------------------------------
// Path Tracing GPU-only render threads
// (base class for all types of OCL path tracers)
//------------------------------------------------------------------------------

class PathOCLBaseRenderThread {
public:
	PathOCLBaseRenderThread(const u_int index, luxrays::OpenCLIntersectionDevice *device,
			PathOCLBaseRenderEngine *re);
	virtual ~PathOCLBaseRenderThread();

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
		ThreadFilm(PathOCLBaseRenderThread *renderThread);
		virtual ~ThreadFilm();
		
		void Init(const Film &engineFilm,
			const u_int threadFilmWidth, const u_int threadFilmHeight);
		void FreeAllOCLBuffers();
		u_int SetFilmKernelArgs(cl::Kernel &filmClearKernel, u_int argIndex) const;
		void ClearFilm(cl::CommandQueue &oclQueue,
			cl::Kernel &filmClearKernel, const size_t filmClearWorkGroupSize);
		void TransferFilm(cl::CommandQueue &oclQueue);

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

	private:
		PathOCLBaseRenderThread *renderThread;
	};

	// Implementation specific methods
	virtual void RenderThreadImpl() = 0;
	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight) = 0;
	virtual void AdditionalInit() = 0;
	virtual std::string AdditionalKernelOptions() = 0;
	virtual std::string AdditionalKernelDefinitions() = 0;
	virtual std::string AdditionalKernelSources() = 0;
	virtual void SetAdditionalKernelArgs() = 0;
	virtual void CompileAdditionalKernels(cl::Program *program) = 0;

	void AllocOCLBufferRO(cl::Buffer **buff, void *src, const size_t size, const std::string &desc);
	void AllocOCLBufferRW(cl::Buffer **buff, const size_t size, const std::string &desc);
	void FreeOCLBuffer(cl::Buffer **buff);

	void StartRenderThread();
	void StopRenderThread();

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
	void InitLights();
	void InitKernels();

	void CompileKernel(cl::Program *program, cl::Kernel **kernel, size_t *workgroupSize, const std::string &name);
	void SetKernelArgs();

	// OpenCL structure size
	size_t GetOpenCLHitPointSize() const;
	size_t GetOpenCLBSDFSize() const;
	size_t GetOpenCLSampleResultSize() const;

	luxrays::OpenCLIntersectionDevice *intersectionDevice;

	// OpenCL variables
	cl::Kernel *filmClearKernel;
	size_t filmClearWorkGroupSize;

	// Scene buffers
	cl::Buffer *materialsBuff;
	cl::Buffer *texturesBuff;
	cl::Buffer *meshIDBuff;
	cl::Buffer *meshDescsBuff;
	cl::Buffer *meshMatsBuff;
	cl::Buffer *lightsBuff;
	cl::Buffer *envLightIndicesBuff;
	cl::Buffer *lightsDistributionBuff;
	cl::Buffer *infiniteLightDistributionsBuff;
	cl::Buffer *vertsBuff;
	cl::Buffer *normalsBuff;
	cl::Buffer *uvsBuff;
	cl::Buffer *colsBuff;
	cl::Buffer *alphasBuff;
	cl::Buffer *trianglesBuff;
	cl::Buffer *cameraBuff;
	cl::Buffer *triLightDefsBuff;
	cl::Buffer *meshTriLightDefsOffsetBuff;
	cl::Buffer *imageMapDescsBuff;
	vector<cl::Buffer *> imageMapsBuff;

	std::string kernelsParameters;
	luxrays::oclKernelCache *kernelCache;

	boost::thread *renderThread;

	u_int threadIndex;
	PathOCLBaseRenderEngine *renderEngine;
	std::vector<ThreadFilm *> threadFilms;

	bool started, editMode;
};

//------------------------------------------------------------------------------
// Path Tracing 100% OpenCL render engine
// (base class for all types of OCL path tracers)
//------------------------------------------------------------------------------

class PathOCLBaseRenderEngine : public OCLRenderEngine {
public:
	PathOCLBaseRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex,
		const bool realTime = false);
	virtual ~PathOCLBaseRenderEngine();

	virtual bool IsHorizontalStereoSupported() const {
		return true;
	}

	virtual bool IsMaterialCompiled(const MaterialType type) const {
		return (compiledScene == NULL) ? false : compiledScene->IsMaterialCompiled(type);
	}

	virtual bool HasDone() const;
	virtual void WaitForDone() const;

	friend class PathOCLBaseRenderThread;

	size_t maxMemPageSize;

protected:
	virtual PathOCLBaseRenderThread *CreateOCLThread(const u_int index, luxrays::OpenCLIntersectionDevice *device) = 0;

	virtual void StartLockLess();
	virtual void StopLockLess();

	virtual void BeginSceneEditLockLess();
	virtual void EndSceneEditLockLess(const EditActionList &editActions);

	boost::mutex setKernelArgsMutex;

	CompiledScene *compiledScene;

	vector<PathOCLBaseRenderThread *> renderThreads;
	
	bool writeKernelsToFile, useDynamicCodeGenerationForTextures,
		useDynamicCodeGenerationForMaterials;
};

}

#endif

#endif	/* _SLG_PATHOCLBASE_H */
