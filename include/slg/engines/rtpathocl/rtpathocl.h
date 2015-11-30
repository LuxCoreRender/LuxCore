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

	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	void SetAssignedIterations(const u_int iters) { assignedIters = iters; }
	u_int GetAssignedIterations() const { return assignedIters; }
	double GetFrameTime() const { return frameTime; }
	u_int GetMinIterationsToShow() const;

	friend class RTPathOCLRenderEngine;

protected:
	virtual void RenderThreadImpl();
	void UpdateOCLBuffers(const EditActionList &updateActions);

	volatile double frameTime;
	volatile u_int assignedIters;
};

//------------------------------------------------------------------------------
// Real-Time path tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class RTPathOCLRenderEngine : public PathOCLRenderEngine {
public:
	RTPathOCLRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~RTPathOCLRenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	double GetFrameTime() const { return frameTime; }

	virtual void EndSceneEdit(const EditActionList &editActions);

	void WaitNewFrame();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return RTPATHOCL; }
	static std::string GetObjectTag() { return "RTPATHOCL"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex);

	friend class PathOCLRenderEngine;
	friend class RTPathOCLRenderThread;

protected:
	static luxrays::Properties GetDefaultProps();

	virtual PathOCLRenderThread *CreateOCLThread(const u_int index,
		luxrays::OpenCLIntersectionDevice *device);

	virtual void StartLockLess();
	virtual void StopLockLess();
	virtual void UpdateFilmLockLess();

	u_int minIterations;

  	boost::mutex editMutex;
	boost::condition_variable_any editCanStart;
	EditActionList updateActions;

	boost::barrier *frameBarrier;
	double frameStartTime, frameTime;
};

}

#endif

#endif	/* _SLG_RTPATHOCL_H */
