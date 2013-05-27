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

#ifndef _SLG_PATHHYBRID_H
#define	_SLG_PATHHYBRID_H

#include "luxrays/core/randomgen.h"
#include "slg/slg.h"
#include "slg/renderengine.h"
#include "slg/sampler/sampler.h"
#include "slg/film/film.h"
#include "slg/sdl/bsdf.h"

namespace slg {

//------------------------------------------------------------------------------
// Path tracing hybrid render engine
//------------------------------------------------------------------------------

class PathHybridRenderThread;
class PathHybridRenderEngine;

class PathState : public HybridRenderState {
public:
	PathState(PathHybridRenderThread *renderThread, Film *film, luxrays::RandomGenerator *rndGen);
	virtual ~PathState() { }

	virtual void GenerateRays(HybridRenderThread *renderThread);
	virtual double CollectResults(HybridRenderThread *renderThread);

private:
	vector<SampleResult> sampleResults;
};

class PathHybridRenderThread : public HybridRenderThread {
public:
	PathHybridRenderThread(PathHybridRenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class PathState;
	friend class PathHybridRenderEngine;

private:
	HybridRenderState *AllocRenderState(luxrays::RandomGenerator *rndGen) {
		return new PathState(this, threadFilm, rndGen);
	}
	boost::thread *AllocRenderThread() {
		return new boost::thread(&PathHybridRenderThread::RenderFunc, this);
	}
};

class PathHybridRenderEngine : public HybridRenderEngine {
public:
	PathHybridRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return PATHHYBRID; }

	// Signed because of the delta parameter
	int maxEyePathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class PathState;
	friend class PathHybridRenderThread;

protected:
	virtual void StartLockLess();

private:
	HybridRenderThread *NewRenderThread(const u_int index, luxrays::IntersectionDevice *device) {
		return new PathHybridRenderThread(this, index, device);
	}
};

}

#endif	/* _SLG_PATHHYBRID_H */
