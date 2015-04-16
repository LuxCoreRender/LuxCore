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

#ifndef _SLG_BIDIRHYBRID_H
#define	_SLG_BIDIRHYBRID_H

#include "slg/slg.h"
#include "slg/renderengine.h"
#include "slg/samplers/sampler.h"
#include "slg/film/film.h"
#include "slg/bsdf/bsdf.h"

namespace slg {

//------------------------------------------------------------------------------
// Bidirectional path tracing hybrid render engine
//------------------------------------------------------------------------------

typedef struct {
	BSDF bsdf;
	luxrays::Spectrum throughput;
	int depth;

	// Check Iliyan Georgiev's latest technical report for the details of how
	// MIS weight computation works (http://www.iliyan.com/publications/ImplementingVCM)
	float dVC; // MIS quantity used for vertex connection
	float dVCM; // MIS quantity used for vertex connection (and merging in a future)
} PathVertex;

class BiDirHybridRenderThread;
class BiDirHybridRenderEngine;

typedef struct {
	u_int lightPathVertexConnections;
	float filmX, filmY;
	float alpha;
	luxrays::Spectrum radiance;
	vector<float> sampleValue; // Used for pass-through sampling
	vector<luxrays::Spectrum> sampleRadiance;
} BiDirEyeSampleResult;

class BiDirState : public HybridRenderState {
public:
	BiDirState(BiDirHybridRenderThread *renderThread, Film *film, luxrays::RandomGenerator *rndGen);
	virtual ~BiDirState() { }

	virtual void GenerateRays(HybridRenderThread *renderThread);
	virtual double CollectResults(HybridRenderThread *renderThread);

protected:
	void DirectLightSampling(HybridRenderThread *renderThread,
			const u_int eyePathIndex,
			const float u0, const float u1, const float u2,
			const float u3, const float u4,
			const PathVertex &eyeVertex);
	void DirectHitLight(HybridRenderThread *renderThread,
			const LightSource *light, const luxrays::Spectrum &lightRadiance,
			const float directPdfA, const float emissionPdfW,
			const PathVertex &eyeVertex, luxrays::Spectrum *radiance) const;
	void DirectHitLight(HybridRenderThread *renderThread,
			const bool finiteLightSource, const PathVertex &eyeVertex,
			luxrays::Spectrum *radiance) const;

	bool ConnectToEye(HybridRenderThread *renderThread,
			const PathVertex &lightVertex, const float u0,
			const luxrays::Point &lensPoint);
	void ConnectVertices(HybridRenderThread *renderThread,
			const u_int eyePathIndex,
			const PathVertex &eyeVertex, const PathVertex &lightVertex,
			const float u0);

	void TraceLightPath(HybridRenderThread *renderThread, Sampler *sampler,
			const u_int lightPathIndex, vector<vector<PathVertex> > &lightPaths);
	bool Bounce(HybridRenderThread *renderThread,
			Sampler *sampler, const u_int sampleOffset,
			PathVertex *pathVertex, luxrays::Ray *nextEventRay) const;

	// Light tracing results
	vector<float> lightSampleValue; // Used for pass-through sampling
	vector<SampleResult> lightSampleResults;

	// Eye tracing results: I use a vector because of CBiDir. With standard BiDir,
	// the size of the vector is just 1.
	vector<BiDirEyeSampleResult>  eyeSampleResults;

private:
	bool ValidResult(BiDirHybridRenderThread *renderThread,
		const luxrays::Ray *ray, const luxrays::RayHit *rayHit,
		const float u0, luxrays::Spectrum *radiance);
};

class BiDirHybridRenderThread : public HybridRenderThread {
public:
	BiDirHybridRenderThread(BiDirHybridRenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class BiDirState;
	friend class BiDirHybridRenderEngine;

private:
	HybridRenderState *AllocRenderState(luxrays::RandomGenerator *rndGen) {
		return new BiDirState(this, threadFilm, rndGen);
	}
	boost::thread *AllocRenderThread() {
		return new boost::thread(&BiDirHybridRenderThread::RenderFunc, this);
	}
};

class BiDirHybridRenderEngine : public HybridRenderEngine {
public:
	BiDirHybridRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return BIDIRHYBRID; }

	// For classic BiDir, the count is always 1
	u_int eyePathCount, lightPathCount;

	// Signed because of the delta parameter
	int maxEyePathDepth, maxLightPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class BiDirState;
	friend class BiDirHybridRenderThread;

protected:
	virtual void StartLockLess();

private:
	HybridRenderThread *NewRenderThread(const u_int index, luxrays::IntersectionDevice *device) {
		return new BiDirHybridRenderThread(this, index, device);
	}
};

}

#endif	/* _SLG_BIDIRHYBRID_H */
