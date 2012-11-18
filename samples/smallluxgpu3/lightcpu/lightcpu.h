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

class LightCPURenderEngine;

class LightCPURenderThread : public CPURenderThread {
public:
	LightCPURenderThread(LightCPURenderEngine *engine, const u_int index,
			IntersectionDevice *device, const u_int seedVal);

	friend class LightCPURenderEngine;

private:
	boost::thread *AllocRenderThread() { return new boost::thread(&LightCPURenderThread::RenderFunc, this); }

	void RenderFunc();

	void ConnectToEye(const float u0,
			const BSDF &bsdf, const Point &lensPoint, const Spectrum &flux,
			vector<SampleResult> &sampleResults);
	void TraceEyePath(Sampler *sampler, vector<SampleResult> *sampleResults);
};

class LightCPURenderEngine : public CPURenderEngine {
public:
	LightCPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	RenderEngineType GetEngineType() const { return LIGHTCPU; }

	// Signed because of the delta parameter
	int maxPathDepth;

	int rrDepth;
	float rrImportanceCap;

	friend class LightCPURenderThread;

private:
	CPURenderThread *NewRenderThread(const u_int index,
			IntersectionDevice *device, const unsigned int seedVal) {
		return new LightCPURenderThread(this, index, device, seedVal);
	}
};

#endif	/* _LIGHTCPU_H */
