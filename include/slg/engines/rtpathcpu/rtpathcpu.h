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

#ifndef _SLG_RTPATHCPU_H
#define	_SLG_RTPATHCPU_H

#include <condition_variable>
#include <barrier>

#include "slg/engines/pathcpu/pathcpu.h"

namespace slg {

//------------------------------------------------------------------------------
// Real-Time path tracing CPU render engine
//------------------------------------------------------------------------------

class RTPathCPURenderEngine;

class RTPathCPURenderThread : public PathCPURenderThread {
public:
	RTPathCPURenderThread(RTPathCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDeviceRef device);
	~RTPathCPURenderThread();

	friend class RTPathCPURenderEngine;

protected:
	void RTRenderFunc(std::stop_token stop_token);
	virtual luxrays::JThreadUPtr AllocRenderThread() {
		auto t = std::make_unique<luxrays::JThread>(
			std::bind_front(&RTPathCPURenderThread::RTRenderFunc, this)
		);
		luxrays::SetThreadName(t, "LxRTPathCPU");
		return std::move(t);
	}

	virtual void StartRenderThread();
};

class RTPathCPUSampler;

class RTPathCPURenderEngine : public PathCPURenderEngine {
public:
	RTPathCPURenderEngine(RenderConfigRef cfg);
	~RTPathCPURenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	void WaitNewFrame();
	virtual void Pause();
	virtual void Resume();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return RTPATHCPU; }
	static std::string GetObjectTag() { return "RTPATHCPU"; }
	static luxrays::PropertiesUPtr ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(RenderConfigRef rcfg);

	friend class PathCPURenderEngine;
	friend class RTPathCPURenderThread;
	friend class RTPathCPUSampler;

    struct completion_t {
        void operator()() noexcept { }
    };

protected:
	static luxrays::PropertiesUPtr GetDefaultProps();

	virtual bool IsRTMode() const { return true; }
	
	CPURenderThreadUPtr NewRenderThread(const u_int index,
			luxrays::IntersectionDeviceRef device) {
		return std::make_unique<RTPathCPURenderThread>(this, index, device);
	}

	virtual void StartLockLess();
	virtual void StopLockLess();

	virtual void BeginSceneEditLockLess();
	virtual void EndSceneEditLockLess(const EditActionList &editActions);

	virtual void BeginFilmEdit();
	virtual void EndFilmEdit(FilmRef film, std::mutex *flmMutex);

	virtual void UpdateFilmLockLess();

	void PauseThreads();
	void ResumeThreads();

	u_int zoomFactor;
	float zoomWeight;

	std::mutex firstFrameMutex;
    std::condition_variable firstFrameCondition;
	u_int firstFrameThreadDoneCount;
	bool firstFrameDone;

	std::barrier<completion_t> *threadsSyncBarrier;
	std::atomic<bool> threadsPauseMode;
};

}

#endif	/* _SLG_RTPATHCPU_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
