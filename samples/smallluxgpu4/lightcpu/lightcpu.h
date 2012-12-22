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

#ifndef _SLG_LIGHTCPU_H
#define	_SLG_LIGHTCPU_H

#include "slg.h"
#include "renderengine.h"

#include "luxrays/utils/core/randomgen.h"
#include "luxrays/utils/sampler/sampler.h"
#include "luxrays/utils/film/film.h"
#include "luxrays/utils/sdl/bsdf.h"

namespace slg {

//------------------------------------------------------------------------------
// Light tracing CPU render engine
//------------------------------------------------------------------------------

class LightCPURenderEngine;

class LightCPURenderThread : public CPURenderThread {
public:
	LightCPURenderThread(LightCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class LightCPURenderEngine;

private:
	boost::thread *AllocRenderThread() { return new boost::thread(&LightCPURenderThread::RenderFunc, this); }

	void RenderFunc();

	void ConnectToEye(const float u0,
			const luxrays::sdl::BSDF &bsdf, const luxrays::Point &lensPoint, const luxrays::Spectrum &flux,
			vector<luxrays::utils::SampleResult> &sampleResults);
	void TraceEyePath(luxrays::utils::Sampler *sampler, vector<luxrays::utils::SampleResult> *sampleResults);
};

class LightCPURenderEngine : public CPURenderEngine {
public:
	LightCPURenderEngine(RenderConfig *cfg, luxrays::utils::Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return LIGHTCPU; }

	// Signed because of the delta parameter
	int maxPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class LightCPURenderThread;

protected:
	virtual void StartLockLess();

private:
	CPURenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) {
		return new LightCPURenderThread(this, index, device);
	}
};

}

#endif	/* _SLG_LIGHTCPU_H */
