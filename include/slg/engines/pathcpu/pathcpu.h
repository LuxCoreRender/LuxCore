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

#ifndef _SLG_PATHCPU_H
#define	_SLG_PATHCPU_H

#include "luxrays/core/randomgen.h"
#include "slg/slg.h"
#include "slg/renderengine.h"
#include "slg/sampler/sampler.h"
#include "slg/film/film.h"
#include "slg/sdl/bsdf.h"

namespace slg {

//------------------------------------------------------------------------------
// Path tracing CPU render engine
//------------------------------------------------------------------------------

class PathCPURenderEngine;

class PathCPURenderThread : public CPUNoTileRenderThread {
public:
	PathCPURenderThread(PathCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class PathCPURenderEngine;

private:
	virtual boost::thread *AllocRenderThread() { return new boost::thread(&PathCPURenderThread::RenderFunc, this); }

	void RenderFunc();

	void DirectLightSampling(const float u0, const float u1,
			const float u2, const float u3, const float u4,
			const luxrays::Spectrum &pathThrouput, const BSDF &bsdf, const int depth,
			luxrays::Spectrum *radiance);

	void DirectHitFiniteLight(const bool lastSpecular,
			const luxrays::Spectrum &pathThrouput, const float distance, const BSDF &bsdf,
			const float lastPdfW, luxrays::Spectrum *radiance);

	void DirectHitInfiniteLight(const bool lastSpecular, const luxrays::Spectrum &pathThrouput,
			const luxrays::Vector &eyeDir, const float lastPdfW, luxrays::Spectrum *radiance);
};

class PathCPURenderEngine : public CPUNoTileRenderEngine {
public:
	PathCPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return PATHCPU; }

	virtual bool IsHorizontalStereoSupported() const {
		return true;
	}

	// Signed because of the delta parameter
	int maxPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class PathCPURenderThread;

protected:
	virtual void StartLockLess();

private:
	CPURenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) {
		return new PathCPURenderThread(this, index, device);
	}
};

}

#endif	/* _SLG_PATHCPU_H */
