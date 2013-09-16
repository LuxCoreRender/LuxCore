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

#ifndef _SLG_RENDERENGINE_H
#define	_SLG_RENDERENGINE_H

#include <deque>

#include "luxrays/core/utils.h"

#include "slg/slg.h"
#include "slg/renderconfig.h"
#include "slg/editaction.h"

#include "slg/film/film.h"
#include "slg/sdl/bsdf.h"

namespace slg {

typedef enum {
	PATHOCL  = 4,
	LIGHTCPU = 5,
	PATHCPU = 6,
	BIDIRCPU = 7,
	BIDIRHYBRID = 8,
	CBIDIRHYBRID = 9,
	BIDIRVMCPU = 10,
	FILESAVER = 11,
	RTPATHOCL = 12,
	PATHHYBRID = 13,
	BIASPATHCPU = 14
} RenderEngineType;

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
	virtual bool WaitNewFrame() { return false; };

	virtual RenderEngineType GetEngineType() const = 0;

	void SetSeed(const unsigned long seed);
	void GenerateNewSeed();

	virtual bool IsMaterialCompiled(const MaterialType type) const {
		return true;
	}
	virtual bool IsHorizontalStereoSupported() const {
		return false;
	}

	const vector<luxrays::IntersectionDevice *> &GetIntersectionDevices() const {
		return intersectionDevices;
	}

	const std::vector<luxrays::DeviceDescription *> &GetAvailableDeviceDescriptions() const {
		return ctx->GetAvailableDeviceDescriptions();
	}

	//--------------------------------------------------------------------------
	// Statistics related methods
	//--------------------------------------------------------------------------

	unsigned int GetPass() const {
		return static_cast<unsigned int>(samplesCount / (film->GetWidth() * film->GetHeight()));
	}
	double GetTotalSampleCount() const { return samplesCount; }
	float GetConvergence() const { return convergence; }
	double GetTotalSamplesSec() const {
		return (elapsedTime == 0.0) ? 0.0 : (samplesCount / elapsedTime);
	}
	double GetTotalRaysSec() const { return (elapsedTime == 0.0) ? 0.0 : (raysCount / elapsedTime); }
	double GetRenderingTime() const { return elapsedTime; }

	//--------------------------------------------------------------------------

	static float RussianRouletteProb(const luxrays::Spectrum &color, const float cap) {
		return luxrays::Clamp(color.Filter(), cap, 1.f);
	}

	static RenderEngineType String2RenderEngineType(const std::string &type);
	static const std::string RenderEngineType2String(const RenderEngineType type);
	static RenderEngine *AllocRenderEngine(const RenderEngineType engineType,
		RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex);

protected:
	virtual void StartLockLess() = 0;
	virtual void StopLockLess() = 0;

	virtual void BeginEditLockLess() = 0;
	virtual void EndEditLockLess(const EditActionList &editActions) = 0;

	virtual void UpdateFilmLockLess() = 0;
	virtual void UpdateCounters() = 0;

	boost::mutex engineMutex;
	luxrays::Context *ctx;
	vector<luxrays::DeviceDescription *> selectedDeviceDescs;
	vector<luxrays::IntersectionDevice *> intersectionDevices;

	RenderConfig *renderConfig;
	Film *film;
	boost::mutex *filmMutex;

	u_int seedBase;
	luxrays::RandomGenerator seedBaseGenerator;

	double startTime, elapsedTime;
	double samplesCount, raysCount;

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
			const u_int index, luxrays::IntersectionDevice *dev);
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

	u_int threadIndex;
	CPURenderEngine *renderEngine;

	boost::thread *renderThread;
	luxrays::IntersectionDevice *device;

	bool started, editMode;
};

class CPURenderEngine : public RenderEngine {
public:
	CPURenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	~CPURenderEngine();

	friend class CPURenderThread;

protected:
	virtual CPURenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) = 0;

	virtual void StartLockLess();
	virtual void StopLockLess();

	virtual void BeginEditLockLess();
	virtual void EndEditLockLess(const EditActionList &editActions);

	virtual void UpdateFilmLockLess() = 0;
	virtual void UpdateCounters() = 0;

	vector<CPURenderThread *> renderThreads;
};

//------------------------------------------------------------------------------
// CPU render engines with no tile rendering
//------------------------------------------------------------------------------

class CPUNoTileRenderEngine;

class CPUNoTileRenderThread : public CPURenderThread {
public:
	CPUNoTileRenderThread(CPUNoTileRenderEngine *engine,
			const u_int index, luxrays::IntersectionDevice *dev);
	virtual ~CPUNoTileRenderThread();

	friend class CPUNoTileRenderEngine;

protected:
	virtual void StartRenderThread();

	Film *threadFilm;
};

class CPUNoTileRenderEngine : public CPURenderEngine {
public:
	CPUNoTileRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	~CPUNoTileRenderEngine();

	friend class CPUNoTileRenderThread;

protected:
	virtual void UpdateFilmLockLess();
	virtual void UpdateCounters();
};

//------------------------------------------------------------------------------
// TileRepository 
//------------------------------------------------------------------------------

class TileRepository {
public:
	typedef struct {
		u_int xStart, yStart;
		// -1 means: tile has to be rendered with all samples
		int sampleIndex;
	} Tile;

	TileRepository(const u_int size);
	~TileRepository();

	void HilberCurveTiles(const int sampleIndex,
		const u_int n, const int xo, const int yo,
		const int xd, const int yd, const int xp, const int yp,
		const int xEnd, const int yEnd);

	void Clear();
	void GetPendingTiles(vector<Tile> &tiles);

	void InitTiles(const u_int width, const u_int height);
	const bool NextTile(Tile **tile, const u_int width, const u_int height);

	u_int tileSize;
	u_int totalSamplesPerPixel;
	bool enableProgressiveRefinement, enableMultipassRendering;
	bool done;

private:
	boost::mutex tileMutex;
	std::deque<Tile *> todoTiles;
	std::vector<Tile *> pendingTiles;
};

//------------------------------------------------------------------------------
// CPU render engines with tile rendering
//------------------------------------------------------------------------------

class CPUTileRenderEngine;

class CPUTileRenderThread : public CPURenderThread {
public:
	CPUTileRenderThread(CPUTileRenderEngine *engine,
			const u_int index, luxrays::IntersectionDevice *dev);
	virtual ~CPUTileRenderThread();

	friend class CPUTileRenderEngine;

protected:
	virtual void StartRenderThread();

	Film *tileFilm;
};

class CPUTileRenderEngine : public CPURenderEngine {
public:
	CPUTileRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	~CPUTileRenderEngine();

	void GetPendingTiles(vector<TileRepository::Tile> &tiles) { return tileRepository->GetPendingTiles(tiles); }
	u_int GetTileSize() const { return tileRepository->tileSize; }

	friend class CPUTileRenderThread;

protected:
	const bool NextTile(TileRepository::Tile **tile, const Film *tileFilm);

	virtual void StartLockLess();
	virtual void StopLockLess();

	virtual void EndEditLockLess(const EditActionList &editActions);

	virtual void UpdateFilmLockLess() { }
	virtual void UpdateCounters();

	TileRepository *tileRepository;
	bool printedRenderingTime;
};

//------------------------------------------------------------------------------
// Base class for OpenCL render engines
//------------------------------------------------------------------------------

class OCLRenderEngine : public RenderEngine {
public:
	OCLRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex,
		bool fatal = true);

	static size_t GetQBVHEstimatedStackSize(const luxrays::DataSet &dataSet);
};

//------------------------------------------------------------------------------
// Base class for Hybrid render engines
//------------------------------------------------------------------------------

class HybridRenderThread;
class HybridRenderEngine;

class HybridRenderState {
public:
	HybridRenderState(HybridRenderThread *rendeThread, Film *film, luxrays::RandomGenerator *rndGen);
	virtual ~HybridRenderState();

	virtual void GenerateRays(HybridRenderThread *renderThread) = 0;
	// Returns the number of rendered samples (mostly for statistics)
	virtual double CollectResults(HybridRenderThread *renderThread) = 0;

	friend class HybridRenderThread;
	friend class HybridRenderEngine;

protected:
	Sampler *sampler;
};

class HybridRenderThread {
public:
	HybridRenderThread(HybridRenderEngine *re, const unsigned int index,
			luxrays::IntersectionDevice *device);
	~HybridRenderThread();

	void Start();
    void Interrupt();
	void Stop();

	void BeginEdit();
	void EndEdit(const EditActionList &editActions);

	friend class HybridRenderState;
	friend class HybridRenderEngine;

protected:
	virtual HybridRenderState *AllocRenderState(luxrays::RandomGenerator *rndGen) = 0;
	virtual boost::thread *AllocRenderThread() = 0;

	void RenderFunc();

	void StartRenderThread();
	void StopRenderThread();

	size_t PushRay(const luxrays::Ray &ray);
	void PopRay(const luxrays::Ray **ray, const luxrays::RayHit **rayHit);

	boost::thread *renderThread;
	Film *threadFilm;
	luxrays::IntersectionDevice *device;

	unsigned int threadIndex;
	HybridRenderEngine *renderEngine;
	u_int pixelCount;

	double samplesCount;

	unsigned int pendingRayBuffers;
	luxrays::RayBuffer *currentRayBufferToSend;
	std::deque<luxrays::RayBuffer *> freeRayBuffers;

	luxrays::RayBuffer *currentReiceivedRayBuffer;
	size_t currentReiceivedRayBufferIndex;

	// Used to store values shared among all metropolis samplers
	double metropolisSharedTotalLuminance, metropolisSharedSampleCount;

	bool started, editMode;
};

class HybridRenderEngine : public OCLRenderEngine {
public:
	HybridRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~HybridRenderEngine() { }

	friend class HybridRenderState;
	friend class HybridRenderThread;

protected:
	virtual HybridRenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) = 0;

	virtual void StartLockLess();
	virtual void StopLockLess();

	virtual void BeginEditLockLess();
	virtual void EndEditLockLess(const EditActionList &editActions);

	virtual void UpdateFilmLockLess();
	virtual void UpdateCounters();

	vector<HybridRenderThread *> renderThreads;
};

}

#endif	/* _SLG_RENDERENGINE_H */
