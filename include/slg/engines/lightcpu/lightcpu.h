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

#ifndef _SLG_LIGHTCPU_H
#define	_SLG_LIGHTCPU_H

#include "slg/slg.h"
#include "slg/renderengine.h"
#include "slg/samplers/sampler.h"
#include "slg/film/film.h"
#include "slg/bsdf/bsdf.h"

namespace slg {

//------------------------------------------------------------------------------
// Light tracing CPU render engine
//------------------------------------------------------------------------------

class LightCPURenderEngine;

class LightCPURenderThread : public CPUNoTileRenderThread {
public:
	LightCPURenderThread(LightCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class LightCPURenderEngine;

private:
	virtual boost::thread *AllocRenderThread() { return new boost::thread(&LightCPURenderThread::RenderFunc, this); }

	void RenderFunc();

	void ConnectToEye(const float u0, const LightSource &light,
			const BSDF &bsdf, const luxrays::Point &lensPoint,
			const std::vector<luxrays::Spectrum> &flux,
			std::vector<luxrays::Spectrum> &connectionEmissions,
			PathVolumeInfo volInfo, vector<SampleResult> &sampleResults);
	void TraceEyePath(const float time, Sampler *sampler,
			std::vector<luxrays::Spectrum> &connectionEmissions,
			PathVolumeInfo volInfo,	vector<SampleResult> &sampleResults);

	SampleResult &AddResult(vector<SampleResult> &sampleResults, const bool fromLight) const;
	
	// Used to offset Sampler data
	static const u_int sampleBootSize = 13;
	static const u_int sampleEyeStepSize = 3;
	static const u_int sampleLightStepSize = 5;

};

class LightCPURenderEngine : public CPUNoTileRenderEngine {
public:
	LightCPURenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return LIGHTCPU; }

	// Signed because of the delta parameter
	int maxPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class LightCPURenderThread;

protected:
	virtual void StartLockLess();

private:
	CPURenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) {
		return new LightCPURenderThread(this, index, device);
	}
};

}

#endif	/* _SLG_LIGHTCPU_H */
