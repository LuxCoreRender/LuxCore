/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _BIDIRHYBRID_H
#define	_BIDIRHYBRID_H

#include "smalllux.h"
#include "renderengine.h"

//------------------------------------------------------------------------------
// Bidirectional path tracing hybrid render engine
//------------------------------------------------------------------------------

typedef struct {
	BSDF bsdf;
	Spectrum throughput;
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
	float screenX, screenY;
	float alpha;
	Spectrum radiance;
	vector<float> sampleValue; // Used for pass-through sampling
	vector<Spectrum> sampleRadiance;
} BiDirEyeSampleResult;

class BiDirState : public HybridRenderState {
public:
	BiDirState(BiDirHybridRenderThread *renderThread, Film *film, RandomGenerator *rndGen);
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
			const bool finiteLightSource, const PathVertex &eyeVertex,
			Spectrum *radiance) const;

	bool ConnectToEye(HybridRenderThread *renderThread,
			const PathVertex &lightVertex, const float u0,
			const Point &lensPoint);
	void ConnectVertices(HybridRenderThread *renderThread,
			const u_int eyePathIndex,
			const PathVertex &eyeVertex, const PathVertex &lightVertex,
			const float u0);

	void TraceLightPath(HybridRenderThread *renderThread, Sampler *sampler,
			const u_int lightPathIndex, vector<vector<PathVertex> > &lightPaths);
	bool Bounce(HybridRenderThread *renderThread,
			Sampler *sampler, const u_int sampleOffset,
			PathVertex *pathVertex, Ray *nextEventRay) const;

	// Light tracing results
	vector<float> lightSampleValue; // Used for pass-through sampling
	vector<SampleResult> lightSampleResults;

	// Eye tracing results: I use a vector because of CBiDir. With standard BiDir,
	// the size of the vector is just 1.
	vector<BiDirEyeSampleResult>  eyeSampleResults;

private:
	bool ValidResult(BiDirHybridRenderThread *renderThread,
		const Ray *ray, const RayHit *rayHit,
		const float u0, Spectrum *radiance);
};

class BiDirHybridRenderThread : public HybridRenderThread {
public:
	BiDirHybridRenderThread(BiDirHybridRenderEngine *engine, const u_int index,
			IntersectionDevice *device, const u_int seedVal);

	friend class BiDirState;
	friend class BiDirHybridRenderEngine;

private:
	HybridRenderState *AllocRenderState(RandomGenerator *rndGen) {
		return new BiDirState(this, threadFilm, rndGen);
	}
	boost::thread *AllocRenderThread() {
		return new boost::thread(&BiDirHybridRenderThread::RenderFunc, this);
	}
};

class BiDirHybridRenderEngine : public HybridRenderEngine {
public:
	BiDirHybridRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return BIDIRHYBRID; }

	// For classic BiDir, the count is always 1
	u_int eyePathCount, lightPathCount;

	// Signed because of the delta parameter
	int maxEyePathDepth, maxLightPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class BiDirState;
	friend class BiDirHybridRenderThread;

private:
	HybridRenderThread *NewRenderThread(const u_int index, IntersectionDevice *device,
			const u_int seedVal) {
		return new BiDirHybridRenderThread(this, index, device, seedVal);
	}
};

#endif	/* _BIDIRHYBRID_H */
