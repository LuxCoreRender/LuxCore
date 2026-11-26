/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_TILEPATHOCL_H
#define	_SLG_TILEPATHOCL_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <vector>

#include "slg/engines/tilepathcpu/tilepathcpu.h"
#include "slg/engines/pathoclbase/pathoclbase.h"

namespace slg {

class TilePathOCLRenderEngine;

//------------------------------------------------------------------------------
// Tile path tracing GPU-only render threads
//------------------------------------------------------------------------------

class TilePathOCLRenderThread : public PathOCLBaseOCLRenderThread {
public:
	TilePathOCLRenderThread(const u_int index, luxrays::HardwareIntersectionDeviceRef device,
			TilePathOCLRenderEngine *re);
	virtual ~TilePathOCLRenderThread();

	friend class TilePathOCLRenderEngine;

protected:
	void UpdateSamplerData(const TileWork &tileWork,
			slg::ocl::TilePathSamplerSharedData &samplerData);

	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight, u_int *filmSubRegion);
	virtual void RenderThreadImpl(std::stop_token stop_token);
	
	void RenderTileWork(const TileWork &tileWork,
			slg::ocl::TilePathSamplerSharedData &samplerData,
			const u_int filmIndex);
};

//------------------------------------------------------------------------------
// Tile path tracing native render threads
//------------------------------------------------------------------------------

class TilePathNativeRenderThread : public PathOCLBaseNativeRenderThread {
public:
	TilePathNativeRenderThread(const u_int index, luxrays::NativeIntersectionDeviceRef device,
			TilePathOCLRenderEngine *re);
	virtual ~TilePathNativeRenderThread();

	friend class TilePathOCLRenderEngine;

protected:
	virtual void StartRenderThread();
	virtual void RenderThreadImpl(std::stop_token stop_token);

	void SampleGrid(luxrays::RandomGenerator *rndGen, const u_int size,
		const u_int ix, const u_int iy, float *u0, float *u1) const;
	void RenderTile(const Tile *tile, const u_int filmIndex);
	FilmRef GetTileFilm() { return *tileFilm; }
	FilmConstRef GetTileFilm() const { return *tileFilm; }
	FilmPtr GetTileFilmPtr() { return FilmPtr(tileFilm.get()); }
	FilmConstPtr GetTileFilmPtr() const { return FilmConstPtr(tileFilm.get()); }


private:
	FilmUPtr tileFilm;
};

//------------------------------------------------------------------------------
// Tile path tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class TilePathOCLRenderEngine : public PathOCLBaseRenderEngine {
public:
	TilePathOCLRenderEngine(RenderConfigRef cfg, const bool supportsNativeThreads);
	virtual ~TilePathOCLRenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	void GetPendingTiles(std::deque<const Tile *> &tiles) { return tileRepository->GetPendingTiles(tiles); }
	void GetNotConvergedTiles(std::deque<const Tile *> &tiles) { return tileRepository->GetNotConvergedTiles(tiles); }
	void GetConvergedTiles(std::deque<const Tile *> &tiles) { return tileRepository->GetConvergedTiles(tiles); }
	u_int GetTileWidth() const { return tileRepository->tileWidth; }
	u_int GetTileHeight() const { return tileRepository->tileHeight; }

	virtual RenderStateSPtr GetRenderState();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return TILEPATHOCL; }
	static std::string GetObjectTag() { return "TILEPATHOCL"; }
	static luxrays::PropertiesUPtr ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(RenderConfigRef rcfg);

	friend class TilePathOCLRenderThread;
	friend class TilePathNativeRenderThread;

	// Samples settings
	u_int aaSamples;

	u_int maxTilePerDevice;

protected:
	static luxrays::PropertiesUPtr GetDefaultProps();

	virtual PathOCLBaseOCLRenderThread *CreateOCLThread(const u_int index,
		luxrays::HardwareIntersectionDeviceRef device);
	virtual PathOCLBaseNativeRenderThread *CreateNativeThread(const u_int index,
			luxrays::NativeIntersectionDeviceRef device);

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
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
