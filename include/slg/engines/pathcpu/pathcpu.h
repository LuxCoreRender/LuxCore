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

#ifndef _SLG_PATHCPU_H
#define	_SLG_PATHCPU_H

#include "slg/slg.h"
#include "slg/renderengine.h"
#include "slg/samplers/sampler.h"
#include "slg/film/film.h"
#include "slg/bsdf/bsdf.h"

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

private:
	virtual boost::thread *AllocRenderThread() { return new boost::thread(&PathCPURenderThread::RenderFunc, this); }

	void RenderFunc();

	void DirectLightSampling(
		const float time, const float u0,
		const float u1, const float u2,
		const float u3, const float u4,
		const luxrays::Spectrum &pathThrouput, const BSDF &bsdf,
		PathVolumeInfo volInfo, const u_int depth,
		SampleResult *sampleResult);

	void DirectHitFiniteLight(const BSDFEvent lastBSDFEvent, const luxrays::Spectrum &pathThrouput,
			const float distance, const BSDF &bsdf, const float lastPdfW,
			SampleResult *sampleResult);
	void DirectHitInfiniteLight(const BSDFEvent lastBSDFEvent, const luxrays::Spectrum &pathThrouput,
			const luxrays::Vector &eyeDir, const float lastPdfW,
			SampleResult *sampleResult);
};

class PathCPURenderEngine : public CPUNoTileRenderEngine {
public:
	PathCPURenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	~PathCPURenderEngine();

	RenderEngineType GetEngineType() const { return PATHCPU; }

	// Signed because of the delta parameter
	u_int maxPathDepth;

	u_int rrDepth;
	float rrImportanceCap;

	// Clamping settings
	float sqrtVarianceClampMaxValue;
	float pdfClampValue;

	bool useFastPixelFilter;

	friend class PathCPURenderThread;

protected:
	virtual void StartLockLess();

	FilterDistribution *pixelFilterDistribution;

private:
	void InitPixelFilterDistribution();

	CPURenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) {
		return new PathCPURenderThread(this, index, device);
	}
};

}

#endif	/* _SLG_PATHCPU_H */
