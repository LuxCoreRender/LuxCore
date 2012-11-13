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

#ifndef _BIDIRCPU_H
#define	_BIDIRCPU_H

#include "smalllux.h"
#include "renderengine.h"

#include "luxrays/utils/sdl/bsdf.h"

//------------------------------------------------------------------------------
// Bidirectional path tracing CPU render engine
//------------------------------------------------------------------------------

typedef struct {
	BSDF bsdf;
	Spectrum throughput;
	int depth;

	float d0, d1vc;
} PathVertex;

class BiDirCPURenderEngine;

class BiDirCPURenderThread : public CPURenderThread {
public:
	BiDirCPURenderThread(BiDirCPURenderEngine *engine, const u_int index, const u_int seedVal);

	friend class BiDirCPURenderEngine;

private:
	boost::thread *AllocRenderThread() { return new boost::thread(&BiDirCPURenderThread::RenderFunc, this); }

	void RenderFunc();

	void DirectLightSampling(
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathVertex &eyeVertex, Spectrum *radiance) const;
	void DirectHitFiniteLight(const PathVertex &eyeVertex, Spectrum *radiance) const;
	void DirectHitInfiniteLight(const PathVertex &eyeVertex,
		Spectrum *radiance) const;

	void ConnectToEye(const unsigned int pixelCount, 
		const PathVertex &BiDirVertex, const float u0,
		const Point &lensPoint, vector<SampleResult> &sampleResults) const;
	void ConnectVertices(const PathVertex &eyeVertex, const PathVertex &BiDirVertex,
		SampleResult *eyeSampleResult, const float u0) const;

	double samplesCount;
};

class BiDirCPURenderEngine : public CPURenderEngine {
public:
	BiDirCPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return BIDIRCPU; }

	// Signed because of the delta parameter
	int maxEyePathDepth, maxLightPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class BiDirCPURenderThread;

private:
	CPURenderThread *NewRenderThread(const u_int index, const u_int seedVal) {
		return new BiDirCPURenderThread(this, index, seedVal);
	}

	void UpdateSamplesCount() {
		double count = 0.0;
		for (size_t i = 0; i < renderThreads.size(); ++i) {
			BiDirCPURenderThread *thread = (BiDirCPURenderThread *)renderThreads[i];
			if (thread)
				count += thread->samplesCount;
		}
		samplesCount = count;
	}
};

#endif	/* _BIDIRCPU_H */
