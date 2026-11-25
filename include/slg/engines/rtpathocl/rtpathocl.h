/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/tilepathocl/tilepathocl.h"

namespace slg {

class RTPathOCLRenderEngine;

//------------------------------------------------------------------------------
// Real-Time path tracing GPU-only render threads
//------------------------------------------------------------------------------

class RTPathOCLRenderThread : public TilePathOCLRenderThread {
public:
	RTPathOCLRenderThread(const u_int index, luxrays::HardwareIntersectionDeviceRef device,
			TilePathOCLRenderEngine *re);
	virtual ~RTPathOCLRenderThread();

	virtual void Interrupt();

	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	friend class RTPathOCLRenderEngine;

protected:
	virtual void RenderThreadImpl(std::stop_token stop_token);

	void UpdateOCLBuffers(const EditActionList &updateActions);
	void UpdateAllThreadsOCLBuffers();
	
	void UpdateCameraOCLBuffer();
	void UpdateAllCameraThreadsOCLBuffers();

	TileWork tileWork;
};

//------------------------------------------------------------------------------
// Real-Time path tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

typedef enum {
	SYNCTYPE_NONE,
	SYNCTYPE_STOP,
	SYNCTYPE_ENDSCENEEDIT,
	SYNCTYPE_BEGINFILMEDIT
} RTPathOCLSyncType;


class RTPathOCLRenderEngine : public TilePathOCLRenderEngine {
public:
	RTPathOCLRenderEngine(RenderConfigRef cfg);
	virtual ~RTPathOCLRenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	double GetFrameTime() const { return frameTime; }

	virtual void EndSceneEdit(const EditActionList &editActions);

	virtual void BeginFilmEdit();
	virtual void EndFilmEdit(FilmRef film, std::mutex *flmMutex);

	virtual void WaitNewFrame();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return RTPATHOCL; }
	static std::string GetObjectTag() { return "RTPATHOCL"; }
	static luxrays::PropertiesUPtr ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(RenderConfigRef rcfg);

	friend class TilePathOCLRenderEngine;
	friend class RTPathOCLRenderThread;

	// Must be a power of 2
	u_int previewResolutionReduction, previewResolutionReductionStep;
	u_int resolutionReduction;

    struct completion_t {
        void operator()() noexcept { }
    };

protected:
	static luxrays::PropertiesUPtr GetDefaultProps();

	virtual void InitGPUTaskConfiguration();
	virtual bool IsRTMode() const { return true; }

	virtual PathOCLBaseOCLRenderThread *CreateOCLThread(const u_int index,
			luxrays::HardwareIntersectionDeviceRef device);

	virtual void StartLockLess();
	virtual void StopLockLess();
	virtual void UpdateFilmLockLess();

	void PauseThreads();
	void ResumeThreads();

	EditActionList updateActions;
	bool useFastCameraEditPath, cameraIsUsingCustomBokeh;

	// Used by RTPathOCLRenderEngine code to sync. with render thread 0
	std::barrier<completion_t> *syncBarrier;
	std::atomic<RTPathOCLSyncType> syncType;

	// Used by all render threads to sync.
	std::barrier<completion_t> *frameBarrier;

	std::atomic<double> frameTime;
};

}

#endif

#endif	/* _SLG_RTPATHOCL_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
