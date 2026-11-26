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

#ifndef _SLG_PATHOCL_H
#define	_SLG_PATHOCL_H

#include <functional>
#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/pathoclbase/pathoclbase.h"

namespace slg {

class PathOCLRenderEngine;

//------------------------------------------------------------------------------
// Path Tracing OpenCL render threads
//------------------------------------------------------------------------------

class PathOCLOpenCLRenderThread : public PathOCLBaseOCLRenderThread {
public:
	PathOCLOpenCLRenderThread(const u_int index, luxrays::HardwareIntersectionDeviceRef device,
			PathOCLRenderEngine *re);
	virtual ~PathOCLOpenCLRenderThread();

	virtual void StartRenderThread();

	friend class PathOCLRenderEngine;

protected:
	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight, u_int *filmSubRegion);
	virtual void RenderThreadImpl(std::stop_token stop_token);
};

//------------------------------------------------------------------------------
// Path Tracing native render threads
//------------------------------------------------------------------------------

class PathOCLNativeRenderThread : public PathOCLBaseNativeRenderThread {
public:
	PathOCLNativeRenderThread(const u_int index, luxrays::NativeIntersectionDeviceRef device,
			PathOCLRenderEngine *re);
	virtual ~PathOCLNativeRenderThread();

	virtual void Start();

	FilmRef GetThreadFilm();
	FilmPtr GetThreadFilmPtr();

	friend class PathOCLRenderEngine;

protected:
	virtual void StartRenderThread();
	virtual void RenderThreadImpl(std::stop_token stop_token);

	// Only the first thread allocate a film. It is then used by all
	// other threads too.
	FilmUPtr threadFilm;
};

//------------------------------------------------------------------------------
// Path Tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class PathOCLRenderEngine : public PathOCLBaseRenderEngine {
public:
	PathOCLRenderEngine(RenderConfigRef cfg);
	virtual ~PathOCLRenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	virtual RenderStateSPtr GetRenderState();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return PATHOCL; }
	static std::string GetObjectTag() { return "PATHOCL"; }
	static luxrays::PropertiesUPtr ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(RenderConfigRef rcfg);

	friend class PathOCLOpenCLRenderThread;
	friend class PathOCLNativeRenderThread;

protected:
	static luxrays::PropertiesUPtr GetDefaultProps();

	virtual PathOCLBaseOCLRenderThread *CreateOCLThread(const u_int index,
			luxrays::HardwareIntersectionDeviceRef device);
	virtual PathOCLBaseNativeRenderThread *CreateNativeThread(const u_int index,
			luxrays::NativeIntersectionDeviceRef device);

	virtual void StartLockLess();
	virtual void StopLockLess();

	void MergeThreadFilms();
	virtual void UpdateFilmLockLess();
	virtual void UpdateCounters();
	void UpdateTaskCount();

	u_int GetTotalEyeSPP() const;

	FilmSampleSplatterUPtr lightSampleSplatter;
	std::shared_ptr<SamplerSharedData> eyeSamplerSharedData;

	bool hasStartFilm, allRenderingThreadsStarted;
};

}

#endif

#endif	/* _SLG_PATHOCL_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
