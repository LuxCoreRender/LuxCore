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

#ifndef _SLG_RTPATHOCL_H
#define	_SLG_RTPATHOCL_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/pathocl/pathocl.h"

namespace slg {

class RTPathOCLRenderEngine;

//------------------------------------------------------------------------------
// Real-Time path tracing GPU-only render threads
//------------------------------------------------------------------------------

class RTPathOCLRenderThread : public PathOCLRenderThread {
public:
	RTPathOCLRenderThread(const u_int index, luxrays::OpenCLIntersectionDevice *device,
			PathOCLRenderEngine *re);
	virtual ~RTPathOCLRenderThread();

	virtual void Interrupt();
	virtual void Stop();

	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	void SetAssignedIterations(const u_int iters) { assignedIters = iters; }
	u_int GetAssignedIterations() const { return assignedIters; }
	double GetFrameTime() const { return frameTime; }
	u_int GetMinIterationsToShow() const;

	friend class RTPathOCLRenderEngine;

protected:
	virtual void RenderThreadImpl();
	virtual void AdditionalInit();
	virtual std::string AdditionalKernelOptions();
	virtual std::string AdditionalKernelSources();
	virtual void SetAdditionalKernelArgs();
	virtual void CompileAdditionalKernels(cl::Program *program);

	void InitDisplayThread();
	void UpdateOCLBuffers(const EditActionList &updateActions);

	cl::Kernel *clearFBKernel;
	size_t clearFBWorkGroupSize;
	cl::Kernel *clearSBKernel;
	size_t clearSBWorkGroupSize;
	cl::Kernel *mergeFBKernel;
	size_t mergeFBWorkGroupSize;
	cl::Kernel *normalizeFBKernel;
	size_t normalizeFBWorkGroupSize;
	cl::Kernel *applyBlurFilterXR1Kernel;
	size_t applyBlurFilterXR1WorkGroupSize;
	cl::Kernel *applyBlurFilterYR1Kernel;
	size_t applyBlurFilterYR1WorkGroupSize;
	cl::Kernel *toneMapLinearKernel;
	size_t toneMapLinearWorkGroupSize;
	cl::Kernel *updateScreenBufferKernel;
	size_t updateScreenBufferWorkGroupSize;

	double lastEditTime;
	volatile double frameTime;
	volatile u_int assignedIters;

	// OpenCL variables
	cl::Buffer *tmpFrameBufferBuff;
	cl::Buffer *mergedFrameBufferBuff;
	cl::Buffer *screenBufferBuff;
};

//------------------------------------------------------------------------------
// Real-Time path tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class RTPathOCLRenderEngine : public PathOCLRenderEngine {
public:
	RTPathOCLRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~RTPathOCLRenderEngine();

	virtual RenderEngineType GetEngineType() const { return RTPATHOCL; }
	double GetFrameTime() const { return frameTime; }

	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	void WaitNewFrame();

	friend class RTPathOCLRenderThread;

protected:
	virtual PathOCLRenderThread *CreateOCLThread(const u_int index,
		luxrays::OpenCLIntersectionDevice *device);

	virtual void StartLockLess();
	virtual void StopLockLess();
	virtual void UpdateFilmLockLess();

	u_int minIterations;
	u_int displayDeviceIndex;
	float blurTimeWindow, blurMinCap, blurMaxCap;
	float ghostEffect;

  	boost::mutex editMutex;
	EditActionList updateActions;

	boost::barrier *frameBarrier;
	double frameTime;
};

}

#endif

#endif	/* _SLG_RTPATHOCL_H */
