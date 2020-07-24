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

#ifndef _SLG_PATHCPU_H
#define	_SLG_PATHCPU_H

#include "slg/slg.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/engines/pathtracer.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/samplers/sampler.h"
#include "slg/film/film.h"
#include "slg/film/filmsamplesplatter.h"
#include "slg/bsdf/bsdf.h"
#include "slg/utils/pathdepthinfo.h"

namespace slg {

//------------------------------------------------------------------------------
// Path tracing CPU render engine
//------------------------------------------------------------------------------

class PathCPURenderEngine;

class PathCPURenderThread : public CPUNoTileRenderThread {
public:
	PathCPURenderThread(PathCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class PathCPURenderEngine;

protected:
	void RenderFunc();
	virtual boost::thread *AllocRenderThread() { return new boost::thread(&PathCPURenderThread::RenderFunc, this); }
};

class PathCPURenderEngine : public CPUNoTileRenderEngine {
public:
	PathCPURenderEngine(const RenderConfig *cfg);
	virtual ~PathCPURenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	virtual RenderState *GetRenderState();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return PATHCPU; }
	static std::string GetObjectTag() { return "PATHCPU"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(const RenderConfig *rcfg);

	friend class PathCPURenderThread;

protected:
	static const luxrays::Properties &GetDefaultProps();

	CPURenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) {
		return new PathCPURenderThread(this, index, device);
	}

	virtual void InitFilm();
	virtual void StartLockLess();
	virtual void StopLockLess();
	virtual void EndSceneEditLockLess(const EditActionList &editActions);

	PhotonGICache *photonGICache;
	PathTracer pathTracer;
	FilmSampleSplatter *lightSampleSplatter;
	SamplerSharedData *lightSamplerSharedData;
};

}

#endif	/* _SLG_PATHCPU_H */
