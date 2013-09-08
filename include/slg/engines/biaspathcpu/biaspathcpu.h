/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#ifndef _SLG_BIASPATHCPU_H
#define	_SLG_BIASPATHCPU_H

#include "luxrays/core/randomgen.h"
#include "slg/slg.h"
#include "slg/renderengine.h"
#include "slg/sampler/sampler.h"
#include "slg/film/film.h"
#include "slg/sdl/bsdf.h"

namespace slg {

//------------------------------------------------------------------------------
// Biased path tracing CPU render engine
//------------------------------------------------------------------------------

class BiasPathCPURenderEngine;

class BiasPathCPURenderThread : public CPUTileRenderThread {
public:
	BiasPathCPURenderThread(BiasPathCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class BiasPathCPURenderEngine;

private:
	virtual boost::thread *AllocRenderThread() { return new boost::thread(&BiasPathCPURenderThread::RenderFunc, this); }

	void RenderFunc();

	void SampleGrid(luxrays::RandomGenerator *rndGen, const u_int size,
		const u_int ix, const u_int iy, float *u0, float *u1) const;

	void DirectLightSampling(const float u0, const float u1,
			const float u2, const float u3, const float u4,
			const luxrays::Spectrum &pathThrouput, const BSDF &bsdf, const int depth,
			luxrays::Spectrum *radiance);

	void DirectHitFiniteLight(const bool lastSpecular,
			const luxrays::Spectrum &pathThrouput, const float distance, const BSDF &bsdf,
			const float lastPdfW, luxrays::Spectrum *radiance);

	void DirectHitInfiniteLight(const bool lastSpecular, const luxrays::Spectrum &pathThrouput,
			const luxrays::Vector &eyeDir, const float lastPdfW, luxrays::Spectrum *radiance);

	void TracePath(luxrays::RandomGenerator *rndGen, const luxrays::Ray &ray,
		luxrays::Spectrum *radiance, float *alpha);
};

class BiasPathCPURenderEngine : public CPUTileRenderEngine {
public:
	BiasPathCPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return BIASPATHCPU; }

	// Signed because of the delta parameter
	int maxPathDepth;
	int rrDepth;
	float rrImportanceCap;

	u_int aaSamples;

	friend class BiasPathCPURenderThread;

protected:
	virtual void StartLockLess();

private:
	CPURenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) {
		return new BiasPathCPURenderThread(this, index, device);
	}
};

}

#endif	/* _SLG_BIASPATHCPU_H */
