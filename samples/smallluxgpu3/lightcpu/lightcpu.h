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

#ifndef _LIGHTCPU_H
#define	_LIGHTCPU_H

#include "smalllux.h"
#include "renderengine.h"

//------------------------------------------------------------------------------
// Light tracing CPU render engine
//------------------------------------------------------------------------------

class LightCPURenderEngine : public CPURenderEngine {
public:
	LightCPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return LIGHTCPU; }

	// Signed because of the delta parameter
	int maxPathDepth;

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

	float ConnectToEye(Film *film, const float u0,
			const Vector &eyeDir, const float eyeDistance, const Point &lensPoint,
			const Normal &shadeN, const Spectrum &bsdfEval,
			const Spectrum &flux);
	float ConnectToEye(Film *film, const float u0,
			const BSDF &bsdf, const Point &lensPoint,
			const Spectrum &flux);

	void DirectHitLightSampling(const Vector &eyeDir, const BSDF &bsdf, Spectrum *radiance);
	void DirectHitInfiniteLight(const Vector &eyeDir, Spectrum *radiance);

	vector<double> threadSamplesCount;
};

#endif	/* _LIGHTCPU_H */
