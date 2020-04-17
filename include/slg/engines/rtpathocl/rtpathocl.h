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

#ifndef _SLG_RTPATHOCL_H
#define	_SLG_RTPATHOCL_H

#include <boost/thread.hpp>

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/tilepathocl/tilepathocl.h"

namespace slg {

class RTPathOCLRenderEngine;

//------------------------------------------------------------------------------
// Real-Time path tracing GPU-only render threads
//------------------------------------------------------------------------------

class RTPathOCLRenderThread : public TilePathOCLRenderThread {
public:
	RTPathOCLRenderThread(const u_int index, luxrays::HardwareIntersectionDevice *device,
			TilePathOCLRenderEngine *re);
	virtual ~RTPathOCLRenderThread();

	virtual void Interrupt();

	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	friend class RTPathOCLRenderEngine;

protected:
	virtual void RenderThreadImpl();

	void UpdateOCLBuffers(const EditActionList &updateActions);

	TileWork tileWork;
};

//------------------------------------------------------------------------------
// Real-Time path tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class RTPathOCLRenderEngine : public TilePathOCLRenderEngine {
public:
	RTPathOCLRenderEngine(const RenderConfig *cfg);
	virtual ~RTPathOCLRenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	double GetFrameTime() const { return frameTime; }

	virtual void EndSceneEdit(const EditActionList &editActions);

	virtual void BeginFilmEdit();
	virtual void EndFilmEdit(Film *film, boost::mutex *flmMutex);

	virtual void WaitNewFrame();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return RTPATHOCL; }
	static std::string GetObjectTag() { return "RTPATHOCL"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(const RenderConfig *rcfg);

	friend class TilePathOCLRenderEngine;
	friend class RTPathOCLRenderThread;

	// Must be a power of 2
	u_int previewResolutionReduction, previewResolutionReductionStep;
	u_int resolutionReduction;

protected:
	static const luxrays::Properties &GetDefaultProps();

	virtual void InitGPUTaskConfiguration();
	virtual bool IsRTMode() const { return true; }

	virtual PathOCLBaseOCLRenderThread *CreateOCLThread(const u_int index,
			luxrays::HardwareIntersectionDevice *device);

	virtual void StartLockLess();
	virtual void StopLockLess();
	virtual void UpdateFilmLockLess();

	EditActionList updateActions;

	boost::barrier *frameBarrier;
	double frameStartTime, frameTime;
	u_int frameCounter;
};

}

#endif

#endif	/* _SLG_RTPATHOCL_H */
