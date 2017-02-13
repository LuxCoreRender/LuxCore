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

#ifndef _SLG_TILEPATHOCL_H
#define	_SLG_TILEPATHOCL_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/tilepathcpu/tilepathcpu.h"
#include "slg/engines/pathoclbase/pathoclstatebase.h"

namespace slg {

class TilePathOCLRenderEngine;

//------------------------------------------------------------------------------
// Tile path tracing GPU-only render threads
//------------------------------------------------------------------------------

class TilePathOCLRenderThread : public PathOCLStateKernelBaseRenderThread {
public:
	TilePathOCLRenderThread(const u_int index, luxrays::OpenCLIntersectionDevice *device,
			TilePathOCLRenderEngine *re);
	virtual ~TilePathOCLRenderThread();

	friend class TilePathOCLRenderEngine;

protected:
	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight, u_int *filmSubRegion);
	virtual void RenderThreadImpl();
	
	void RenderTile(const TileRepository::Tile *tile, const u_int filmIndex);
};

//------------------------------------------------------------------------------
// Tile path tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class TilePathOCLRenderEngine : public PathOCLStateKernelBaseRenderEngine {
public:
	TilePathOCLRenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~TilePathOCLRenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	void GetPendingTiles(std::deque<const TileRepository::Tile *> &tiles) { return tileRepository->GetPendingTiles(tiles); }
	void GetNotConvergedTiles(std::deque<const TileRepository::Tile *> &tiles) { return tileRepository->GetNotConvergedTiles(tiles); }
	void GetConvergedTiles(std::deque<const TileRepository::Tile *> &tiles) { return tileRepository->GetConvergedTiles(tiles); }
	u_int GetTileWidth() const { return tileRepository->tileWidth; }
	u_int GetTileHeight() const { return tileRepository->tileHeight; }

	virtual RenderState *GetRenderState();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return TILEPATHOCL; }
	static std::string GetObjectTag() { return "TILEPATHOCL"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex);

	friend class TilePathOCLRenderThread;

	// Samples settings
	u_int aaSamples;

	u_int maxTilePerDevice;

protected:
	static const luxrays::Properties &GetDefaultProps();

	virtual PathOCLBaseRenderThread *CreateOCLThread(const u_int index,
		luxrays::OpenCLIntersectionDevice *device);

	virtual void StartLockLess();
	virtual void StopLockLess();
	virtual void EndSceneEditLockLess(const EditActionList &editActions);
	virtual void UpdateFilmLockLess() { }
	virtual void UpdateCounters();

	void InitTaskCount();
	void InitTileRepository();

	TileRepository *tileRepository;
};

}

#endif

#endif	/* _SLG_TILEPATHOCL_H */
