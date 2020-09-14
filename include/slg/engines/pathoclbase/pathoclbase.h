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

#ifndef _SLG_PATHOCLBASE_H
#define	_SLG_PATHOCLBASE_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "luxrays/devices/nativeintersectiondevice.h"
#include "luxrays/devices/oclintersectiondevice.h"
#include "luxrays/devices/cudaintersectiondevice.h"
#include "luxrays/utils/ocl.h"

#include "slg/slg.h"
#include "slg/engines/oclrenderengine.h"
#include "slg/engines/pathoclbase/pathoclbaseoclthread.h"
#include "slg/engines/pathoclbase/pathoclbasenativethread.h"
#include "slg/engines/pathtracer.h"

namespace slg {

//------------------------------------------------------------------------------
// Path Tracing 100% OpenCL render engine
// (base class for all types of OCL path tracers)
//------------------------------------------------------------------------------

class RenderConfig;
class CompiledScene;

class PathOCLBaseRenderEngine : public OCLRenderEngine {
public:
	PathOCLBaseRenderEngine(const RenderConfig *cfg, const bool supportsNativeThreads);
	virtual ~PathOCLBaseRenderEngine();

	virtual bool HasDone() const;
	virtual void WaitForDone() const;

	static bool HasCachedKernels(const RenderConfig &renderConfig);

	friend class PathOCLBaseOCLRenderThread;

	size_t maxMemPageSize;
	u_int taskCount;

	PathTracer pathTracer;

protected:
	virtual PathOCLBaseOCLRenderThread *CreateOCLThread(const u_int index,
			luxrays::HardwareIntersectionDevice *device) = 0;
	virtual PathOCLBaseNativeRenderThread *CreateNativeThread(const u_int index,
			luxrays::NativeIntersectionDevice *device) {
		throw std::runtime_error("Internal error, called PathOCLBaseRenderEngine::CreateNativeThread()");
	}

	virtual void InitGPUTaskConfiguration();
	void InitPixelFilterDistribution();

	virtual void InitFilm();
	virtual void StartLockLess();
	virtual void StopLockLess();

	virtual void BeginSceneEditLockLess();
	virtual void EndSceneEditLockLess(const EditActionList &editActions);

	void SetCachedKernels(const RenderConfig &renderConfig);
	static std::string GetCachedKernelsHash(const RenderConfig &renderConfig);

	boost::mutex setKernelArgsMutex;

	slg::ocl::pathoclbase::GPUTaskConfiguration taskConfig;
	CompiledScene *compiledScene;

	std::vector<PathOCLBaseOCLRenderThread *> renderOCLThreads;
	std::vector<PathOCLBaseNativeRenderThread *> renderNativeThreads;
	
	std::vector<std::string> additionalOpenCLKernelOptions, additionalCUDAKernelOptions;
	bool writeKernelsToFile;

	// Pixel filter related variables
	float *pixelFilterDistribution;
	u_int pixelFilterDistributionSize;

	slg::ocl::Sampler *oclSampler;
	slg::ocl::Filter *oclPixelFilter;
	PhotonGICache *photonGICache;
};

}

#endif

#endif	/* _SLG_PATHOCLBASE_H */
