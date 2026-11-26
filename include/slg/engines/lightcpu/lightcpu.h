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

#ifndef _SLG_LIGHTCPU_H
#define	_SLG_LIGHTCPU_H

#include "luxrays/utils/thread.h"
#include "slg/slg.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/engines/pathtracer.h"
#include "slg/samplers/sampler.h"
#include "slg/film/film.h"
#include "slg/film/filmsamplesplatter.h"
#include "slg/bsdf/bsdf.h"

namespace slg {

//------------------------------------------------------------------------------
// Light tracing CPU render engine
//------------------------------------------------------------------------------

class LightCPURenderEngine;

class LightCPURenderThread : public CPUNoTileRenderThread {
public:
	LightCPURenderThread(LightCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDeviceRef device);

	friend class LightCPURenderEngine;

private:
	virtual luxrays::JThreadUPtr AllocRenderThread() {
		auto t = std::make_unique<luxrays::JThread>(
			std::bind_front(&LightCPURenderThread::RenderFunc, this)
		);
		luxrays::SetThreadName(t, "LxLightCPU");
		return std::move(t);
	}

	void RenderFunc(std::stop_token stop_token);
};

class LightCPURenderEngine : public CPUNoTileRenderEngine {
public:
	LightCPURenderEngine(RenderConfigRef cfg);
	~LightCPURenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	virtual RenderStateSPtr GetRenderState();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return LIGHTCPU; }
	static std::string GetObjectTag() { return "LIGHTCPU"; }
	static luxrays::PropertiesUPtr ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(RenderConfigRef rcfg);

	friend class LightCPURenderThread;

protected:
	static luxrays::PropertiesUPtr GetDefaultProps();


	virtual void InitFilm();
	virtual void StartLockLess();
	virtual void StopLockLess();

	CPURenderThreadUPtr NewRenderThread(const u_int index,
			luxrays::IntersectionDeviceRef device) {
		return std::make_unique<LightCPURenderThread>(this, index, device);
	}

	PathTracer pathTracer;
};

}

#endif	/* _SLG_LIGHTCPU_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
