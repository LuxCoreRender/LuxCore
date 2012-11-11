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

class BiDirHybridRenderEngine;
class BiDirHybridRenderThread;

class BiDirState {
public:
	BiDirState(BiDirHybridRenderEngine *renderEngine, Film *film, RandomGenerator *rndGen);
	~BiDirState();

	void GenerateRays(BiDirHybridRenderThread *renderThread);
	void CollectResults(BiDirHybridRenderThread *renderThread);

private:
	void DirectLightSampling(BiDirHybridRenderThread *renderThread,
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathVertex &eyeVertex);
	void DirectHitFiniteLight(BiDirHybridRenderThread *renderThread,
			const PathVertex &eyeVertex, Spectrum *radiance) const;
	void DirectHitInfiniteLight(BiDirHybridRenderThread *renderThread,
		const PathVertex &eyeVertex, Spectrum *radiance) const;

	void ConnectVertices(BiDirHybridRenderThread *renderThread,
		const PathVertex &eyeVertex, const PathVertex &lightVertex,
		const float u0);
	void ConnectToEye(BiDirHybridRenderThread *renderThread,
			const unsigned int pixelCount, const PathVertex &lightVertex,
			const float u0,	const Point &lensPoint);

	Sampler *sampler;

	// Light tracing results
	vector<float> lightSampleValue; // Used for pass-through sampling
	vector<SampleResult> lightSampleResults;

	// Eye tracing results
	u_int eyeScreenX, eyeScreenY;
	float eyeAlpha;
	Spectrum eyeRadiance;
	vector<float> eyeSampleValue; // Used for pass-through sampling
	vector<Spectrum> eyeSampleRadiance;
};

//------------------------------------------------------------------------------
// Bidirectional path tracing hybrid render threads
//------------------------------------------------------------------------------

class BiDirHybridRenderThread {
public:
	BiDirHybridRenderThread(const unsigned int index, const unsigned int seedBase,
			IntersectionDevice *device, BiDirHybridRenderEngine *re);
	~BiDirHybridRenderThread();

	void Start();
    void Interrupt();
	void Stop();

	void BeginEdit();
	void EndEdit(const EditActionList &editActions);

	friend class BiDirState;
	friend class BiDirHybridRenderEngine;

private:
	static void RenderThreadImpl(BiDirHybridRenderThread *renderThread);

	void StartRenderThread();
	void StopRenderThread();

	size_t PushRay(const Ray &ray);
	void PopRay(const Ray **ray, const RayHit **rayHit);

	boost::thread *renderThread;
	Film *threadFilm;

	IntersectionDevice *intersectionDevice;

	unsigned int threadIndex, seed;
	BiDirHybridRenderEngine *renderEngine;
	double samplesCount;

	unsigned int pendingRayBuffers;
	RayBuffer *currentRayBufferToSend;
	deque<RayBuffer *> freeRayBuffers;

	RayBuffer *currentReiceivedRayBuffer;
	size_t currentReiceivedRayBufferIndex;
	
	bool started, editMode;
};

//------------------------------------------------------------------------------
// Bidirectional path tracing hybrid render engine
//------------------------------------------------------------------------------

class BiDirHybridRenderEngine : public OCLRenderEngine {
public:
	BiDirHybridRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	~BiDirHybridRenderEngine();

	RenderEngineType GetEngineType() const { return BIDIRHYBRID; }

	const vector<IntersectionDevice *> &GetRealIntersectionDevices() const {
		return realDevices;
	}

	// Signed because of the delta parameter
	int maxEyePathDepth, maxLightPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class BiDirState;
	friend class BiDirHybridRenderThread;

private:
	void StartLockLess();
	void StopLockLess();

	void BeginEditLockLess();
	void EndEditLockLess(const EditActionList &editActions);

	void UpdateFilmLockLess();

	void UpdateSamplesCount() {
		double count = 0.0;
		for (size_t i = 0; i < renderThreads.size(); ++i)
			count += renderThreads[i]->samplesCount;
		samplesCount = count;
	}

	vector<IntersectionDevice *> devices; // Virtual M20 or M2M intersection device
	vector<IntersectionDevice *> realDevices;
	vector<BiDirHybridRenderThread *> renderThreads;
};

#endif	/* _BIDIRHYBRID_H */
