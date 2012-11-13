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

#ifndef _RENDERENGINE_H
#define	_RENDERENGINE_H

#include "smalllux.h"
#include "renderconfig.h"
#include "editaction.h"

#include "luxrays/utils/film/film.h"
#include "luxrays/utils/sdl/bsdf.h"

enum RenderEngineType {
#if !defined(LUXRAYS_DISABLE_OPENCL)
	PATHOCL  = 4,
#endif
	LIGHTCPU = 5,
	PATHCPU = 6,
	BIDIRCPU = 7
#if !defined(LUXRAYS_DISABLE_OPENCL)
	, BIDIRHYBRID = 8
#endif
};

//------------------------------------------------------------------------------
// Base class for render engines
//------------------------------------------------------------------------------

class RenderEngine {
public:
	RenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~RenderEngine();

	void Start();
	void Stop();
	void BeginEdit();
	void EndEdit(const EditActionList &editActions);

	void UpdateFilm();

	virtual RenderEngineType GetEngineType() const = 0;

	virtual bool IsMaterialCompiled(const MaterialType type) const {
		return true;
	}

	unsigned int GetPass() const {
		return samplesCount / (film->GetWidth() * film->GetHeight());
	}
	float GetConvergence() const { return convergence; }
	double GetTotalSamplesSec() const {
		return (elapsedTime == 0.0) ? 0.0 : (samplesCount / elapsedTime);
	}
	double GetRenderingTime() const { return elapsedTime; }

	static RenderEngineType String2RenderEngineType(const string &type);
	static const string RenderEngineType2String(const RenderEngineType type);
	static RenderEngine *AllocRenderEngine(const RenderEngineType engineType,
		RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex);

protected:
	virtual void StartLockLess() = 0;
	virtual void StopLockLess() = 0;

	virtual void BeginEditLockLess() = 0;
	virtual void EndEditLockLess(const EditActionList &editActions) = 0;

	virtual void UpdateFilmLockLess() = 0;

	virtual void UpdateSamplesCount() {
		samplesCount = film->GetTotalSampleCount();
	}

	boost::mutex engineMutex;
	Context *ctx;

	RenderConfig *renderConfig;
	Film *film;
	boost::mutex *filmMutex;
	u_int seedBase;

	double startTime, elapsedTime;
	double samplesCount;

	float convergence;
	double lastConvergenceTestTime;
	double lastConvergenceTestSamplesCount;

	bool started, editMode;
};

//------------------------------------------------------------------------------
// Base class for CPU render engines
//------------------------------------------------------------------------------

class CPURenderEngine;

class CPURenderThread {
public:
	CPURenderThread(CPURenderEngine *engine,
		const u_int index, const u_int seedVal,
		const bool enablePerPixelNormBuffer,
		const bool enablePerScreenNormBuffer);
	virtual ~CPURenderThread();

	virtual void Start();
	virtual void Interrupt();
	virtual void Stop();

	virtual void BeginEdit();
	virtual void EndEdit(const EditActionList &editActions);

	friend class CPURenderEngine;

protected:
	virtual boost::thread *AllocRenderThread() = 0;

	virtual void StartRenderThread();
	virtual void StopRenderThread();

	virtual void RenderFunc() = 0;

	u_int threadIndex;
	u_int seed;
	CPURenderEngine *renderEngine;

	boost::thread *renderThread;
	Film *threadFilm;

	bool started, editMode;
	bool enablePerPixelNormBuffer, enablePerScreenNormBuffer;

	double samplesCount;
};

class CPURenderEngine : public RenderEngine {
public:
	CPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	~CPURenderEngine();

	friend class CPURenderThread;

protected:
	virtual CPURenderThread *NewRenderThread(CPURenderEngine *engine,
		const u_int index, const u_int seedVal) = 0;

	virtual void StartLockLess();
	virtual void StopLockLess();

	virtual void BeginEditLockLess();
	virtual void EndEditLockLess(const EditActionList &editActions);

	virtual void UpdateFilmLockLess();

	vector<CPURenderThread *> renderThreads;
};

#if !defined(LUXRAYS_DISABLE_OPENCL)

//------------------------------------------------------------------------------
// Base class for OpenCL render engines
//------------------------------------------------------------------------------

class OCLRenderEngine : public RenderEngine {
public:
	OCLRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);

	const vector<IntersectionDevice *> &GetIntersectionDevices() const {
		return intersectionDevices;
	}

	double GetTotalRaysSec() const;

protected:
	vector<DeviceDescription *> selectedDeviceDescs;
	vector<IntersectionDevice *> intersectionDevices;
};

#endif

#endif	/* _RENDERENGINE_H */
