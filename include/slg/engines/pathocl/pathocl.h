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

#ifndef _SLG_PATHOCL_H
#define	_SLG_PATHOCL_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/pathoclbase/pathoclbase.h"

namespace slg {

class PathOCLRenderEngine;

//------------------------------------------------------------------------------
// Path Tracing OpenCL render threads
//------------------------------------------------------------------------------

class PathOCLOpenCLRenderThread : public PathOCLBaseOCLRenderThread {
public:
	PathOCLOpenCLRenderThread(const u_int index, luxrays::HardwareIntersectionDevice *device,
			PathOCLRenderEngine *re);
	virtual ~PathOCLOpenCLRenderThread();

	virtual void StartRenderThread();

	friend class PathOCLRenderEngine;

protected:
	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight, u_int *filmSubRegion);
	virtual void RenderThreadImpl();
};

//------------------------------------------------------------------------------
// Path Tracing native render threads
//------------------------------------------------------------------------------

class PathOCLNativeRenderThread : public PathOCLBaseNativeRenderThread {
public:
	PathOCLNativeRenderThread(const u_int index, luxrays::NativeIntersectionDevice *device,
			PathOCLRenderEngine *re);
	virtual ~PathOCLNativeRenderThread();

	virtual void Start();

	friend class PathOCLRenderEngine;

protected:
	virtual void StartRenderThread();
	virtual void RenderThreadImpl();
	
	// Only the first thread allocate a film. It is than used by all
	// other threads too.
	Film *threadFilm;
};

//------------------------------------------------------------------------------
// Path Tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class PathOCLRenderEngine : public PathOCLBaseRenderEngine {
public:
	PathOCLRenderEngine(const RenderConfig *cfg);
	virtual ~PathOCLRenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	virtual RenderState *GetRenderState();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return PATHOCL; }
	static std::string GetObjectTag() { return "PATHOCL"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(const RenderConfig *rcfg);

	friend class PathOCLOpenCLRenderThread;
	friend class PathOCLNativeRenderThread;

protected:
	static const luxrays::Properties &GetDefaultProps();

	virtual PathOCLBaseOCLRenderThread *CreateOCLThread(const u_int index,
			luxrays::HardwareIntersectionDevice *device);
	virtual PathOCLBaseNativeRenderThread *CreateNativeThread(const u_int index,
			luxrays::NativeIntersectionDevice *device);

	virtual void StartLockLess();
	virtual void StopLockLess();

	void MergeThreadFilms();
	virtual void UpdateFilmLockLess();
	virtual void UpdateCounters();
	void UpdateTaskCount();

	u_int GetTotalEyeSPP() const;
	
	FilmSampleSplatter *lightSampleSplatter;
	SamplerSharedData *eyeSamplerSharedData;
	SamplerSharedData *lightSamplerSharedData;

	bool hasStartFilm, allRenderingThreadsStarted;
};

}

#endif

#endif	/* _SLG_PATHOCL_H */
