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

#ifndef _PATHOCL_H
#define	_PATHOCL_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "smalllux.h"
#include "renderengine.h"
#include "ocldatatypes.h"
#include "compiledscene.h"

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/opencl/utils.h"

#include <boost/thread/thread.hpp>

class PathOCLRenderEngine;

//------------------------------------------------------------------------------
// Path Tracing GPU-only render threads
//------------------------------------------------------------------------------

class PathOCLRenderThread {
public:
	PathOCLRenderThread(const unsigned int index, const unsigned int seedBase,
			const float samplingStart, OpenCLIntersectionDevice *device,
			PathOCLRenderEngine *re);
	~PathOCLRenderThread();

	void Start();
    void Interrupt();
	void Stop();

	void BeginEdit();
	void EndEdit(const EditActionList &editActions);

	friend class PathOCLRenderEngine;

private:
	static void RenderThreadImpl(PathOCLRenderThread *renderThread);

	void AllocOCLBufferRO(cl::Buffer **buff, void *src, const size_t size, const string &desc);
	void AllocOCLBufferRW(cl::Buffer **buff, const size_t size, const string &desc);
	void FreeOCLBuffer(cl::Buffer **buff);

	void StartRenderThread();
	void StopRenderThread();

	void InitRender();

	void InitFrameBuffer();
	void InitCamera();
	void InitGeometry();
	void InitMaterials();
	void InitAreaLights();
	void InitInfiniteLight();
	void InitSunLight();
	void InitSkyLight();
	void InitTextureMaps();
	void InitKernels();

	void SetKernelArgs();

	OpenCLIntersectionDevice *intersectionDevice;

	// OpenCL variables
	string kernelsParameters;
	cl::Kernel *initKernel;
	size_t initWorkGroupSize;
	cl::Kernel *initFBKernel;
	size_t initFBWorkGroupSize;
	cl::Kernel *samplerKernel;
	size_t samplerWorkGroupSize;
	cl::Kernel *advancePathsKernel;
	size_t advancePathsWorkGroupSize;

	cl::Buffer *raysBuff;
	cl::Buffer *hitsBuff;
	cl::Buffer *tasksBuff;
	cl::Buffer *taskStatsBuff;
	cl::Buffer *frameBufferBuff;
	cl::Buffer *alphaFrameBufferBuff;
	cl::Buffer *materialsBuff;
	cl::Buffer *meshIDBuff;
	cl::Buffer *triangleIDBuff;
	cl::Buffer *meshDescsBuff;
	cl::Buffer *meshMatsBuff;
	cl::Buffer *infiniteLightBuff;
	cl::Buffer *infiniteLightMapBuff;
	cl::Buffer *sunLightBuff;
	cl::Buffer *skyLightBuff;
	cl::Buffer *vertsBuff;
	cl::Buffer *normalsBuff;
	cl::Buffer *trianglesBuff;
	cl::Buffer *cameraBuff;
	cl::Buffer *areaLightsBuff;
	cl::Buffer *texMapRGBBuff;
	cl::Buffer *texMapAlphaBuff;
	cl::Buffer *texMapDescBuff;
	cl::Buffer *meshTexsBuff;
	cl::Buffer *meshBumpsBuff;
	cl::Buffer *meshBumpsScaleBuff;
	cl::Buffer *meshNormalMapsBuff;
	cl::Buffer *uvsBuff;

	luxrays::utils::oclKernelCache *kernelCache;

	float samplingStart;
	unsigned int seed;

	boost::thread *renderThread;

	unsigned int threadIndex;
	PathOCLRenderEngine *renderEngine;
	PathOCL::Pixel *frameBuffer;
	PathOCL::AlphaPixel *alphaFrameBuffer;

	// TODO: cleanup
	unsigned int frameBufferPixelCount;
	size_t stratifiedDataSize;

	bool started, editMode, reportedPermissionError;

	PathOCL::GPUTaskStats *gpuTaskStats;
};

//------------------------------------------------------------------------------
// Path Tracing 100% OpenCL render engine
//------------------------------------------------------------------------------

class PathOCLRenderEngine : public OCLRenderEngine {
public:
	PathOCLRenderEngine(RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~PathOCLRenderEngine();

	RenderEngineType GetEngineType() const { return PATHOCL; }

	bool IsMaterialCompiled(const MaterialType type) const {
		return (compiledScene == NULL) ? false : compiledScene->IsMaterialCompiled(type);
	}

	friend class PathOCLRenderThread;

	// Signed because of the delta parameter
	int maxPathDepth, maxDiffusePathVertexCount;

	int rrDepth;
	float rrImportanceCap;
	float epsilon;

private:
	void StartLockLess();
	void StopLockLess();

	void BeginEditLockLess();
	void EndEditLockLess(const EditActionList &editActions);

	void UpdateFilmLockLess();

	virtual void UpdateSamplesCount() {
		// Update the sample count statistic
		unsigned long long totalCount = 0;
		for (size_t i = 0; i < renderThreads.size(); ++i) {
			PathOCL::GPUTaskStats *stats = renderThreads[i]->gpuTaskStats;

			for (size_t i = 0; i < taskCount; ++i)
				totalCount += stats[i].sampleCount;
		}

		samplesCount = totalCount;
	}

	CompiledScene *compiledScene;

	vector<PathOCLRenderThread *> renderThreads;

	PathOCL::Sampler *sampler;
	PathOCL::Filter *filter;

	unsigned int taskCount;

	bool usePixelAtomics;
};

#endif

#endif	/* _PATHOCL_H */
