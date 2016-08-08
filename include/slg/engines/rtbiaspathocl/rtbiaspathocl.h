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

	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	friend class RTBiasPathOCLRenderEngine;

protected:
	virtual std::string AdditionalKernelOptions();
	virtual void RenderThreadImpl();
	virtual void EnqueueRenderSampleKernel(cl::CommandQueue &oclQueue);

	void UpdateOCLBuffers(const EditActionList &updateActions);

	TileRepository::Tile *tile;
};

//------------------------------------------------------------------------------
// Real-Time path tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class RTBiasPathOCLRenderEngine : public BiasPathOCLRenderEngine {
public:
	RTBiasPathOCLRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~RTBiasPathOCLRenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	double GetFrameTime() const { return frameTime; }

	virtual void EndSceneEdit(const EditActionList &editActions);

	virtual void BeginFilmEdit();
	virtual void EndFilmEdit(Film *flm);

	virtual void WaitNewFrame();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return RTBIASPATHOCL; }
	static std::string GetObjectTag() { return "RTBIASPATHOCL"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex);

	friend class BiasPathOCLRenderEngine;
	friend class RTBiasPathOCLRenderThread;

	// Must be a power of 2
	u_int previewResolutionReduction, previewResolutionReductionStep;
	u_int resolutionReduction;
	u_int longRunResolutionReduction, longRunResolutionReductionStep;
	bool previewDirectLightOnly;

protected:
	static const luxrays::Properties &GetDefaultProps();

	virtual BiasPathOCLRenderThread *CreateOCLThread(const u_int index,
		luxrays::OpenCLIntersectionDevice *device);

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

#endif	/* _SLG_RTBIASPATHOCL_H */
