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

#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/engines/biaspathcpu/biaspathcpu.h"
#include "slg/engines/biaspathocl/biaspathocl_datatypes.h"

namespace slg {

class BiasPathOCLRenderEngine;

//------------------------------------------------------------------------------
// Biased path tracing GPU-only render threads
//------------------------------------------------------------------------------

class BiasPathOCLRenderThread : public PathOCLBaseRenderThread {
public:
	BiasPathOCLRenderThread(const u_int index, luxrays::OpenCLIntersectionDevice *device,
			BiasPathOCLRenderEngine *re);
	virtual ~BiasPathOCLRenderThread();

	friend class BiasPathOCLRenderEngine;

protected:
	virtual void RenderThreadImpl();
	virtual void GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight);
	virtual void AdditionalInit();
	virtual std::string AdditionalKernelOptions();
	virtual std::string AdditionalKernelDefinitions();
	virtual std::string AdditionalKernelSources();
	virtual void SetAdditionalKernelArgs();
	virtual void CompileAdditionalKernels(cl::Program *program);

	// OpenCL variables
	cl::Kernel *initSeedKernel;
	size_t initSeedWorkGroupSize;
	cl::Kernel *initStatKernel;
	size_t initStatWorkGroupSize;
	cl::Kernel *renderSampleKernel;
	size_t renderSampleWorkGroupSize;
	cl::Kernel *mergePixelSamplesKernel;
	size_t mergePixelSamplesWorkGroupSize;

	cl::Buffer *tasksBuff;
	cl::Buffer *taskStatsBuff;
	cl::Buffer *taskResultsBuff;
	cl::Buffer *pixelFilterBuff;

	u_int sampleDimensions;

	slg::ocl::biaspathocl::GPUTaskStats *gpuTaskStats;
};

//------------------------------------------------------------------------------
// Biased path tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class BiasPathOCLRenderEngine : public PathOCLBaseRenderEngine {
public:
	BiasPathOCLRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~BiasPathOCLRenderEngine();

	virtual RenderEngineType GetEngineType() const { return BIASPATHOCL; }

	void GetPendingTiles(vector<TileRepository::Tile> &tiles) { return tileRepository->GetPendingTiles(tiles); }
	u_int GetTileSize() const { return tileRepository->tileSize; }

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
	virtual PathOCLBaseRenderThread *CreateOCLThread(const u_int index,
		luxrays::OpenCLIntersectionDevice *device);

	virtual void StartLockLess();
	virtual void StopLockLess();
	virtual void EndEditLockLess(const EditActionList &editActions);
	virtual void UpdateFilmLockLess() { }
	virtual void UpdateCounters();

	const bool NextTile(TileRepository::Tile **tile, const Film *tileFilm);

	void InitPixelFilterDistribution();

	u_int taskCount;
	float *pixelFilterDistribution;
	u_int pixelFilterDistributionSize;


	TileRepository *tileRepository;
	bool printedRenderingTime;
};

}

#endif

#endif	/* _SLG_BIASPATHOCL_H */
