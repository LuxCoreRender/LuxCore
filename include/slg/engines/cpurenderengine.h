/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#ifndef _SLG_CPURENDERENGINE_H
#define	_SLG_CPURENDERENGINE_H

#include "luxrays/utils/utils.h"

#include "slg/slg.h"
#include "slg/engines/renderengine.h"
#include "slg/engines/tilerepository.h"

namespace slg {

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

	bool started, editMode, threadDone;
};

class CPURenderEngine : public RenderEngine {
public:
	CPURenderEngine(const RenderConfig *cfg);
	virtual ~CPURenderEngine();

	virtual bool HasDone() const;
	virtual void WaitForDone() const;

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);

	friend class CPURenderThread;

protected:
	static const luxrays::Properties &GetDefaultProps();

	virtual CPURenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) = 0;

	virtual void StartLockLess();
	virtual void StopLockLess();

	virtual void BeginSceneEditLockLess();
	virtual void EndSceneEditLockLess(const EditActionList &editActions);

	virtual void UpdateFilmLockLess() = 0;
	virtual void UpdateCounters() = 0;

	std::vector<CPURenderThread *> renderThreads;
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
};

class CPUNoTileRenderEngine : public CPURenderEngine {
public:
	CPUNoTileRenderEngine(const RenderConfig *cfg);
	virtual ~CPUNoTileRenderEngine();

	virtual void StartLockLess();
	virtual void StopLockLess();

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);

	friend class CPUNoTileRenderThread;

protected:
	static const luxrays::Properties &GetDefaultProps();

	virtual void EndSceneEditLockLess(const EditActionList &editActions);
	virtual void UpdateFilmLockLess();
	virtual void UpdateCounters();

	SamplerSharedData *samplerSharedData;
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
	CPUTileRenderEngine(const RenderConfig *cfg);
	virtual ~CPUTileRenderEngine();

	void GetPendingTiles(std::deque<const Tile *> &tiles) { return tileRepository->GetPendingTiles(tiles); }
	void GetNotConvergedTiles(std::deque<const Tile *> &tiles) { return tileRepository->GetNotConvergedTiles(tiles); }
	void GetConvergedTiles(std::deque<const Tile *> &tiles) { return tileRepository->GetConvergedTiles(tiles); }
	u_int GetTileWidth() const { return tileRepository->tileWidth; }
	u_int GetTileHeight() const { return tileRepository->tileHeight; }

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);

	friend class CPUTileRenderThread;

protected:
	static const luxrays::Properties &GetDefaultProps();

	// I don't implement StartLockLess() here because the step of initializing
	// the tile repository is left to the sub-class (so some TileRepository
	// parameter can be set before to start all rendering threads).
	// virtual void StartLockLess();
	virtual void StopLockLess();

	virtual void EndSceneEditLockLess(const EditActionList &editActions);

	virtual void UpdateFilmLockLess() { }
	virtual void UpdateCounters();

	TileRepository *tileRepository;
};

}

#endif	/* _SLG_CPURENDERENGINE_H */
