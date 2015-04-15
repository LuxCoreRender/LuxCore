/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_PATHHYBRID_H
#define	_SLG_PATHHYBRID_H

#include "slg/slg.h"
#include "slg/renderengine.h"
#include "slg/samplers/sampler.h"
#include "slg/film/film.h"
#include "slg/bsdf/bsdf.h"

namespace slg {

//------------------------------------------------------------------------------
// Path tracing hybrid render engine
//------------------------------------------------------------------------------

class PathHybridRenderThread;
class PathHybridRenderEngine;

class PathHybridState : public HybridRenderState {
public:
	PathHybridState(PathHybridRenderThread *renderThread, Film *film, luxrays::RandomGenerator *rndGen);
	virtual ~PathHybridState() { }

	virtual void GenerateRays(HybridRenderThread *renderThread);
	virtual double CollectResults(HybridRenderThread *renderThread);

private:
	void Init(const PathHybridRenderThread *thread);
	void DirectHitInfiniteLight(const Scene *scene, const luxrays::Vector &eyeDir);
	void DirectHitFiniteLight(const Scene *scene, const float distance, const BSDF &bsdf);
	void DirectLightSampling(const PathHybridRenderThread *renderThread,
		const float u0, const float u1,
		const float u2, const float u3,
		const BSDF &bsdf);
	bool FinalizeRay(const PathHybridRenderThread *renderThread, const luxrays::Ray *ray,
		const luxrays::RayHit *rayHit, BSDF *bsdf, const float u0,
		luxrays::Spectrum *radiance);
	void SplatSample(const PathHybridRenderThread *renderThread);

	int depth;
	float lastPdfW;
	luxrays::Spectrum throuput;

	luxrays::Ray nextPathVertexRay, directLightRay;
	luxrays::Spectrum directLightRadiance;

	vector<SampleResult> sampleResults;

	bool lastSpecular;
};

class PathHybridRenderThread : public HybridRenderThread {
public:
	PathHybridRenderThread(PathHybridRenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class PathHybridState;
	friend class PathHybridRenderEngine;

private:
	HybridRenderState *AllocRenderState(luxrays::RandomGenerator *rndGen) {
		return new PathHybridState(this, threadFilm, rndGen);
	}
	boost::thread *AllocRenderThread() {
		return new boost::thread(&PathHybridRenderThread::RenderFunc, this);
	}
};

class PathHybridRenderEngine : public HybridRenderEngine {
public:
	PathHybridRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return PATHHYBRID; }

	// Signed because of the delta parameter
	int maxPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class PathHybridState;
	friend class PathHybridRenderThread;

protected:
	virtual void StartLockLess();

private:
	HybridRenderThread *NewRenderThread(const u_int index, luxrays::IntersectionDevice *device) {
		return new PathHybridRenderThread(this, index, device);
	}
};

}

#endif	/* _SLG_PATHHYBRID_H */
