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

#ifndef _SLG_RTBIASPATHOCL_H
#define	_SLG_RTBIASPATHOCL_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/biaspathocl/biaspathocl.h"

namespace slg {

class RTBiasPathOCLRenderEngine;

//------------------------------------------------------------------------------
// Real-Time path tracing GPU-only render threads
//------------------------------------------------------------------------------

class RTBiasPathOCLRenderThread : public BiasPathOCLRenderThread {
public:
	RTBiasPathOCLRenderThread(const u_int index, luxrays::OpenCLIntersectionDevice *device,
			BiasPathOCLRenderEngine *re);
	virtual ~RTBiasPathOCLRenderThread();

	virtual void Interrupt();
	virtual void Stop();

	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	friend class RTBiasPathOCLRenderEngine;

protected:
	virtual void RenderThreadImpl();
	virtual void AdditionalInit();
	virtual std::string AdditionalKernelOptions();
	virtual std::string AdditionalKernelSources();
	virtual void SetAdditionalKernelArgs();
	virtual void CompileAdditionalKernels(cl::Program *program);

	void InitDisplayThread();
	void UpdateOCLBuffers(const EditActionList &updateActions);

	double lastEditTime;

	cl::Kernel *clearFBKernel;
	size_t clearFBWorkGroupSize;
	cl::Kernel *clearSBKernel;
	size_t clearSBWorkGroupSize;
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

	// OpenCL variables
	cl::Buffer *tmpFrameBufferBuff;
	cl::Buffer *mergedFrameBufferBuff;
	cl::Buffer *screenBufferBuff;
};

//------------------------------------------------------------------------------
// Real-Time path tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class RTBiasPathOCLRenderEngine : public BiasPathOCLRenderEngine {
public:
	RTBiasPathOCLRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~RTBiasPathOCLRenderEngine();

	virtual RenderEngineType GetEngineType() const { return RTBIASPATHOCL; }
	double GetFrameTime() const { return frameTime; }

	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	void WaitNewFrame();

	friend class RTBiasPathOCLRenderThread;

protected:
	virtual BiasPathOCLRenderThread *CreateOCLThread(const u_int index,
		luxrays::OpenCLIntersectionDevice *device);

	virtual void StartLockLess();
	virtual void StopLockLess();
	virtual void UpdateFilmLockLess();

	u_int displayDeviceIndex;
	float blurTimeWindow, blurMinCap, blurMaxCap;
	float ghostEffect;

 	boost::mutex editMutex;
	boost::condition_variable_any editCanStart;
	EditActionList updateActions;

	boost::barrier *frameBarrier;
	double frameTime;
};

}

#endif

#endif	/* _SLG_RTBIASPATHOCL_H */
