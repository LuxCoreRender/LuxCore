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

#ifndef _SLG_BIASPATHOCL_H
#define	_SLG_BIASPATHOCL_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/biaspathcpu/biaspathcpu.h"
#include "slg/engines/pathocl/pathocl.h"

namespace slg {

class BiasPathOCLRenderEngine;

//------------------------------------------------------------------------------
// Biased path tracing GPU-only render threads
//------------------------------------------------------------------------------

class BiasPathOCLRenderThread : public PathOCLRenderThread {
public:
	BiasPathOCLRenderThread(const u_int index, luxrays::OpenCLIntersectionDevice *device,
			PathOCLRenderEngine *re);
	virtual ~BiasPathOCLRenderThread();

	friend class BiasPathOCLRenderEngine;

protected:
	virtual void RenderThreadImpl();
};

//------------------------------------------------------------------------------
// Biased path tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class BiasPathOCLRenderEngine : public PathOCLRenderEngine {
public:
	BiasPathOCLRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~BiasPathOCLRenderEngine();

	virtual RenderEngineType GetEngineType() const { return BIASPATHOCL; }

	friend class BiasPathOCLRenderThread;

	// Path depth settings
	PathDepthInfo maxPathDepth;

	// Samples settings
	u_int aaSamples, diffuseSamples, glossySamples, specularSamples, directLightSamples;

	// Clamping settings
	bool clampValueEnabled;
	float clampMaxValue;

	// Light settings
	float lowLightThreashold, nearStartLight;
	bool lightSamplingStrategyONE;

protected:
	virtual PathOCLRenderThread *CreateOCLThread(const u_int index,
		luxrays::OpenCLIntersectionDevice *device);

	virtual void StartLockLess();
	virtual void StopLockLess();
	virtual void EndEditLockLess(const EditActionList &editActions);
	virtual void UpdateCounters();

	const bool NextTile(TileRepository::Tile **tile, const Film *tileFilm);

	TileRepository *tileRepository;
	bool printedRenderingTime;
};

}

#endif

#endif	/* _SLG_BIASPATHOCL_H */
