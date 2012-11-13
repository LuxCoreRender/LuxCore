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
#include "bidircpu/bidircpu.h"

//------------------------------------------------------------------------------
// Bidirectional path tracing hybrid render engine
//------------------------------------------------------------------------------

class BiDirHybridRenderEngine;

class BiDirState : public HybridRenderState {
public:
	BiDirState(BiDirHybridRenderEngine *renderEngine, Film *film, RandomGenerator *rndGen);

	void GenerateRays(HybridRenderThread *renderThread);
	void CollectResults(HybridRenderThread *renderThread);

private:
	void DirectLightSampling(HybridRenderThread *renderThread,
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathVertex &eyeVertex);
	void DirectHitFiniteLight(HybridRenderThread *renderThread,
			const PathVertex &eyeVertex, Spectrum *radiance) const;
	void DirectHitInfiniteLight(HybridRenderThread *renderThread,
		const PathVertex &eyeVertex, Spectrum *radiance) const;

	void ConnectVertices(HybridRenderThread *renderThread,
		const PathVertex &eyeVertex, const PathVertex &lightVertex,
		const float u0);
	void ConnectToEye(HybridRenderThread *renderThread,
			const unsigned int pixelCount, const PathVertex &lightVertex,
			const float u0,	const Point &lensPoint);

	// Light tracing results
	vector<float> lightSampleValue; // Used for pass-through sampling
	vector<SampleResult> lightSampleResults;

	// Eye tracing results
	float eyeScreenX, eyeScreenY;
	float eyeAlpha;
	Spectrum eyeRadiance;
	vector<float> eyeSampleValue; // Used for pass-through sampling
	vector<Spectrum> eyeSampleRadiance;
};

class BiDirHybridRenderThread : public HybridRenderThread {
public:
	BiDirHybridRenderThread(BiDirHybridRenderEngine *engine, const u_int index,
			const u_int seedVal, IntersectionDevice *device);

	friend class BiDirState;
	friend class BiDirHybridRenderEngine;

private:
	HybridRenderState *AllocRenderState(RandomGenerator *rndGen) {
		return new BiDirState((BiDirHybridRenderEngine *)renderEngine, threadFilm, rndGen);
	}
	boost::thread *AllocRenderThread() {
		return new boost::thread(&BiDirHybridRenderThread::RenderFunc, this);
	}
};

class BiDirHybridRenderEngine : public HybridRenderEngine {
public:
	BiDirHybridRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return BIDIRHYBRID; }

	// Signed because of the delta parameter
	int maxEyePathDepth, maxLightPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class BiDirState;
	friend class BiDirHybridRenderThread;

private:
	HybridRenderThread *NewRenderThread(const u_int index, const u_int seedVal,
			IntersectionDevice *device) {
		return new BiDirHybridRenderThread(this, index, seedVal, device);
	}
};

#endif	/* _BIDIRHYBRID_H */
