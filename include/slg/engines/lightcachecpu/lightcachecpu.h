/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_LIGHTCACHECPU_H
#define	_SLG_LIGHTCACHECPU_H

#include "slg/slg.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/samplers/sampler.h"
#include "slg/film/film.h"
#include "slg/film/filmsamplesplatter.h"
#include "slg/bsdf/bsdf.h"

namespace slg {

//------------------------------------------------------------------------------
// Light cache tracing CPU render engine
//------------------------------------------------------------------------------

class LightCacheCPURenderEngine;

class LightCacheCPURenderThread : public CPUNoTileRenderThread {
public:
	LightCacheCPURenderThread(LightCacheCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class LightCacheCPURenderEngine;

private:
	virtual boost::thread *AllocRenderThread() { return new boost::thread(&LightCacheCPURenderThread::RenderFunc, this); }

	void RenderFunc();

	void TraceEyePath(Sampler *sampler, std::vector<SampleResult> &sampleResults);

	SampleResult &AddResult(std::vector<SampleResult> &sampleResults) const;
	
	// Used to offset Sampler data
	static const u_int sampleBootSize = 5;
	static const u_int sampleStepSize = 3;

};

class LightCacheCPURenderEngine : public CPUNoTileRenderEngine {
public:
	LightCacheCPURenderEngine(const RenderConfig *cfg);
	~LightCacheCPURenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	virtual RenderState *GetRenderState();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return LIGHTCPU; }
	static std::string GetObjectTag() { return "LIGHTCACHECPU"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(const RenderConfig *rcfg);

	// Signed because of the delta parameter
	int maxPathDepth;

	int rrDepth;
	float rrImportanceCap;

	// Clamping settings
	float sqrtVarianceClampMaxValue;

	bool forceBlackBackground;

	friend class LightCacheCPURenderThread;

protected:
	static const luxrays::Properties &GetDefaultProps();

	virtual void InitFilm();
	virtual void StartLockLess();
	virtual void StopLockLess();

private:
	CPURenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) {
		return new LightCacheCPURenderThread(this, index, device);
	}

	FilmSampleSplatter *sampleSplatter;
};

}

#endif	/* _SLG_LIGHTCACHECPU_H */
