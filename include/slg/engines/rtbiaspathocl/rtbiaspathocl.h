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

	virtual void BeginEdit();
	virtual void EndEdit(const EditActionList &editActions);

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
	RTBiasPathOCLRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~RTBiasPathOCLRenderEngine();

	virtual RenderEngineType GetEngineType() const { return RTBIASPATHOCL; }
	double GetFrameTime() const { return frameTime; }

	virtual void BeginEdit();
	virtual void EndEdit(const EditActionList &editActions);

	bool WaitNewFrame();

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
	EditActionList updateActions;

	boost::barrier *frameBarrier;
	double frameTime;
};

}

#endif

#endif	/* _SLG_RTBIASPATHOCL_H */
