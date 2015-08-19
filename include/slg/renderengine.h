/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifndef _SLG_RENDERENGINE_H
#define	_SLG_RENDERENGINE_H

#include <deque>
#include <boost/heap/priority_queue.hpp>
#include <boost/thread/condition_variable.hpp>

#include "luxrays/core/utils.h"

#include "slg/slg.h"
#include "slg/renderconfig.h"
#include "slg/editaction.h"
#include "slg/film/film.h"
#include "slg/bsdf/bsdf.h"
#include "slg/utils/varianceclamping.h"

namespace slg {

typedef enum {
	PATHOCL = 4,
	LIGHTCPU = 5,
	PATHCPU = 6,
	BIDIRCPU = 7,
	BIDIRVMCPU = 10,
	FILESAVER = 11,
	RTPATHOCL = 12,
	BIASPATHCPU = 14,
	BIASPATHOCL = 15,
	RTBIASPATHOCL = 16
} RenderEngineType;

//------------------------------------------------------------------------------
// Base class for render engines
//------------------------------------------------------------------------------

class RenderEngine {
public:
	RenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~RenderEngine();

	virtual void Start();
	virtual void Stop();
	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	virtual bool HasDone() const = 0;
	virtual void WaitForDone() const = 0;

	void UpdateFilm();
	virtual void WaitNewFrame() { }

	virtual RenderEngineType GetEngineType() const = 0;

	void SetSeed(const unsigned long seed);
	void GenerateNewSeed();

	virtual bool IsMaterialCompiled(const MaterialType type) const {
		return true;
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

	u_int GetPass() const {
		return static_cast<u_int>(samplesCount / (film->GetWidth() * film->GetHeight()));
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

protected:
	virtual void StartLockLess() = 0;
	virtual void StopLockLess() = 0;

	virtual void BeginSceneEditLockLess() = 0;
	virtual void EndSceneEditLockLess(const EditActionList &editActions) = 0;

	virtual void UpdateFilmLockLess() = 0;
	virtual void UpdateCounters() = 0;

	boost::mutex engineMutex;
	luxrays::Context *ctx;
	vector<luxrays::DeviceDescription *> selectedDeviceDescs;
	vector<luxrays::IntersectionDevice *> intersectionDevices;

	const RenderConfig *renderConfig;
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

	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	virtual bool HasDone() const;
	virtual void WaitForDone() const;

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
	CPURenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	~CPURenderEngine();

	virtual bool HasDone() const;
	virtual void WaitForDone() const;

	friend class CPURenderThread;

protected:
	virtual CPURenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) = 0;

	virtual void StartLockLess();
	virtual void StopLockLess();

	virtual void BeginSceneEditLockLess();
	virtual void EndSceneEditLockLess(const EditActionList &editActions);

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
	CPUNoTileRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
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
	class Tile {
	public:
		Tile(TileRepository *tileRepository, const Film &film,
				const u_int xStart, const u_int yStart);
		virtual ~Tile();

		void Restart();
		void VarianceClamp(Film &tileFilm);
		void AddPass(const Film &tileFilm);
		
		// Read-only for every one but Tile/TileRepository classes
		u_int xStart, yStart;
		u_int pass;
		bool done;

	private:
		void InitTileFilm(const Film &film, Film **tileFilm);
		void CheckConvergence();
		void UpdateMaxPixelValue();

		TileRepository *tileRepository;
		Film *allPassFilm, *evenPassFilm;
	};

	TileRepository(const u_int tileWidth, const u_int tileHeight);
	~TileRepository();

	void Clear();
	void Restart();
	void SetVarianceClamping(float sqrtVarianceClampMaxValue);
	void GetPendingTiles(std::deque<const Tile *> &tiles);
	void GetNotConvergedTiles(std::deque<const Tile *> &tiles);
	void GetConvergedTiles(std::deque<const Tile *> &tiles);

	void InitTiles(const Film &film);
	bool NextTile(Film *film, boost::mutex *filmMutex,
		Tile **tile, Film *tileFilm);

	friend class Tile;

	u_int tileWidth, tileHeight;
	u_int totalSamplesPerPixel;

	u_int maxPassCount;
	float convergenceTestThreshold, convergenceTestThresholdReduction;
	VarianceClamping varianceClamping;
	bool enableMultipassRendering, enableRenderingDonePrint;

	bool done;

private:
	class CompareTilesPtr {
	public:
		bool operator()(const TileRepository::Tile *lt, const TileRepository::Tile *rt) const {
			return lt->pass > rt->pass;
		}
	};

	void HilberCurveTiles(
		const Film &film,
		const u_int n, const int xo, const int yo,
		const int xd, const int yd, const int xp, const int yp,
		const int xEnd, const int yEnd);

	void SetDone();
	bool GetToDoTile(Tile **tile);

	boost::mutex tileMutex;
	double startTime;

	float tileMaxPixelValue; // Updated only if convergence test is enabled

	std::vector<Tile *> tileList;

	boost::heap::priority_queue<Tile *,
			boost::heap::compare<CompareTilesPtr>,
			boost::heap::stable<true>
		> todoTiles;
	std::deque<Tile *> pendingTiles;
	std::deque<Tile *> convergedTiles;
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
	CPUTileRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	~CPUTileRenderEngine();

	void GetPendingTiles(std::deque<const TileRepository::Tile *> &tiles) { return tileRepository->GetPendingTiles(tiles); }
	void GetNotConvergedTiles(std::deque<const TileRepository::Tile *> &tiles) { return tileRepository->GetNotConvergedTiles(tiles); }
	void GetConvergedTiles(std::deque<const TileRepository::Tile *> &tiles) { return tileRepository->GetConvergedTiles(tiles); }
	u_int GetTileWidth() const { return tileRepository->tileWidth; }
	u_int GetTileHeight() const { return tileRepository->tileHeight; }

	friend class CPUTileRenderThread;

protected:
	// I don't implement StartLockLess() here because the step of initializing
	// the tile repository is left to the sub-class (so some TileRepository
	// can be set before to start all rendering threads).
	// virtual void StartLockLess();
	virtual void StopLockLess();

	virtual void EndSceneEditLockLess(const EditActionList &editActions);

	virtual void UpdateFilmLockLess() { }
	virtual void UpdateCounters();

	TileRepository *tileRepository;
};

//------------------------------------------------------------------------------
// Base class for OpenCL render engines
//------------------------------------------------------------------------------

class OCLRenderEngine : public RenderEngine {
public:
	OCLRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex,
		bool fatal = true);

	static size_t GetQBVHEstimatedStackSize(const luxrays::DataSet &dataSet);
};

}

#endif	/* _SLG_RENDERENGINE_H */
