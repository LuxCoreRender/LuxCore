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

	// Check Iliyan Georgiev's latest technical report for the details of how
	// MIS weight computation works (http://www.iliyan.com/publications/ImplementingVCM)
	float dVC; // MIS quantity used for vertex connection
	float dVCM; // MIS quantity used for vertex connection (and merging in a future)
	float dVM; // MIS quantity used for vertex merging
} PathVertexVM;

class BiDirCPURenderEngine;

class BiDirCPURenderThread : public CPURenderThread {
public:
	BiDirCPURenderThread(BiDirCPURenderEngine *engine, const u_int index,
			IntersectionDevice *device);

	friend class BiDirCPURenderEngine;

protected:
	// Used to offset Sampler data
	static const u_int sampleBootSize = 11;
	static const u_int sampleLightStepSize = 6;
	static const u_int sampleEyeStepSize = 11;

	static float MIS(const float a) {
		//return a; // Balance heuristic
		return a * a; // Power heuristic
	}

	boost::thread *AllocRenderThread() { return new boost::thread(&BiDirCPURenderThread::RenderFunc, this); }

	void RenderFunc();

	void DirectLightSampling(
		const float u0, const float u1, const float u2,
		const float u3, const float u4,
		const PathVertexVM &eyeVertex, Spectrum *radiance) const;
	void DirectHitLight(const bool finiteLightSource,
		const PathVertexVM &eyeVertex, Spectrum *radiance) const;

	void ConnectVertices(const PathVertexVM &eyeVertex, const PathVertexVM &BiDirVertex,
		SampleResult *eyeSampleResult, const float u0) const;
	void ConnectToEye(const PathVertexVM &BiDirVertex, const float u0,
		const Point &lensPoint, vector<SampleResult> &sampleResults) const;

	void TraceLightPath(Sampler *sampler, const Point &lensPoint,
		vector<PathVertexVM> &lightPathVertices,
		vector<SampleResult> &sampleResults) const;
	bool Bounce(Sampler *sampler, const u_int sampleOffset,
		PathVertexVM *pathVertex, Ray *nextEventRay) const;

	u_int pixelCount;

	float misVmWeightFactor; // Weight of vertex merging (used in VC)
    float misVcWeightFactor; // Weight of vertex connection (used in VM)
	float vmNormalization; // 1 / (Pi * radius^2 * light_path_count)
};

class BiDirCPURenderEngine : public CPURenderEngine {
public:
	BiDirCPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return BIDIRCPU; }

	// Signed because of the delta parameter
	int maxEyePathDepth, maxLightPathDepth;

	// Used for vertex merging, it enables VM if it is > 0
	u_int lightPathsCount;
	float baseRadius; // VM (i.e. SPPM) start radius parameter
	float radiusAlpha; // VM (i.e. SPPM) alpha parameter

	int rrDepth;
	float rrImportanceCap;

	friend class BiDirCPURenderThread;

private:
	CPURenderThread *NewRenderThread(const u_int index, IntersectionDevice *device) {
		return new BiDirCPURenderThread(this, index, device);
	}
};

#endif	/* _BIDIRCPU_H */
