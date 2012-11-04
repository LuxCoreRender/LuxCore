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

//------------------------------------------------------------------------------
// Bidirectional path tracing CPU render engine
//------------------------------------------------------------------------------

typedef struct {
	BSDF bsdf;
	Spectrum throughput;
	int depth;

	float d0;
} PathVertex;

class BiDirCPURenderEngine : public CPURenderEngine {
public:
	BiDirCPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return BIDIRCPU; }

	// Signed because of the delta parameter
	int maxEyePathDepth, maxLightPathDepth;

	int rrDepth;
	float rrImportanceCap;

private:
	static void RenderThreadFuncImpl(CPURenderThread *thread);

	void StartLockLess() {
		threadSamplesCount.resize(renderThreads.size(), 0.0);

		CPURenderEngine::StartLockLess();
	}

	void UpdateSamplesCount() {
		double count = 0.0;
		for (size_t i = 0; i < renderThreads.size(); ++i)
			count += threadSamplesCount[i];
		samplesCount = count;
	}

	void DirectLightSampling(
		const float u0, const float u1, const float u2, const float u3,
		const float u4, const float u5,
		const PathVertex &eyeVertex, Spectrum *radiance);
	void DirectHitFiniteLight(const PathVertex &eyeVertex, Spectrum *radiance);
	void DirectHitInfiniteLight(const PathVertex &eyeVertex,
		Spectrum *radiance);

	void ConnectToEye(const unsigned int pixelCount, 
		const PathVertex &lightVertex, const float u0,
		const Point &lensPoint, vector<SampleResult> &sampleResults);

	vector<double> threadSamplesCount;
};

#endif	/* _BIDIRCPU_H */
