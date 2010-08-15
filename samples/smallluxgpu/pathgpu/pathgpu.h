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

#ifndef PATHGPU_H
#define	PATHGPU_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "smalllux.h"
#include "slgscene.h"
#include "renderengine.h"
#include "luxrays/core/intersectiondevice.h"

#include <boost/thread/thread.hpp>

class PathGPURenderEngine;

typedef struct {
	unsigned int s1, s2, s3;
} PathGPUSeed;

typedef struct {
	Spectrum throughput;
	unsigned int depth, pixelIndex;
	PathGPUSeed seed;
} PathGPU;

typedef struct {
	Spectrum c;
	unsigned int count;
} PathGPUPixel;

//------------------------------------------------------------------------------
// Path Tracing GPU-only render threads
//------------------------------------------------------------------------------

#define PATHGPU_PATH_COUNT (2 * 65536)

class PathGPURenderThread {
public:
	PathGPURenderThread(unsigned int index, PathGPURenderEngine *re);
	virtual ~PathGPURenderThread();

	virtual void Start();
    virtual void Interrupt() = 0;
	virtual void Stop();

	virtual void ClearPaths() = 0;
	virtual unsigned int GetPass() const = 0;

	friend class PathGPURenderEngine;

protected:
	unsigned int threadIndex;
	PathGPURenderEngine *renderEngine;
	PathGPUPixel *frameBuffer;

	bool started;
};

class PathGPUDeviceRenderThread : public PathGPURenderThread {
public:
	PathGPUDeviceRenderThread(unsigned int index, unsigned long seedBase,
			const float samplingStart, const unsigned int samplePerPixel,
			OpenCLIntersectionDevice *device, PathGPURenderEngine *re);
	~PathGPUDeviceRenderThread();

	void Start();
    void Interrupt();
	void Stop();

	void ClearPaths();

	unsigned int GetPass() const { return 0; }

private:
	static void RenderThreadImpl(PathGPUDeviceRenderThread *renderThread);

	OpenCLIntersectionDevice *intersectionDevice;

	// OpenCL variables
	cl::Kernel *initKernel;
	size_t initWorkGroupSize;
	cl::Kernel *initFBKernel;
	size_t initFBWorkGroupSize;
	cl::Kernel *advancePathKernel;
	size_t advancePathWorkGroupSize;

	cl::Buffer *raysBuff;
	cl::Buffer *hitsBuff;
	cl::Buffer *pathsBuff;
	cl::Buffer *frameBufferBuff;

	float samplingStart;

	boost::thread *renderThread;

	bool reportedPermissionError;
};

//------------------------------------------------------------------------------
// Path Tracing GPU-only render engine
//------------------------------------------------------------------------------

class PathGPURenderEngine : public RenderEngine {
public:
	PathGPURenderEngine(SLGScene *scn, Film *flm, boost::mutex *filmMutex,
			vector<IntersectionDevice *> intersectionDevices, const Properties &cfg);
	virtual ~PathGPURenderEngine();

	void Start();
	void Interrupt();
	void Stop();

	void Reset();

	unsigned int GetPass() const;
	unsigned int GetThreadCount() const;
	RenderEngineType GetEngineType() const { return PATHGPU; }

	void UpdateFilm();

	friend class PathGPURenderThread;
	friend class PathGPUDeviceRenderThread;

	unsigned int samplePerPixel;

	// Signed because of the delta parameter
	int maxPathDepth;

	int rrDepth;
	float rrProb;

private:
	vector<OpenCLIntersectionDevice *> oclIntersectionDevices;
	vector<PathGPURenderThread *> renderThreads;
	SampleBuffer *sampleBuffer;
};

#endif

#endif	/* PATHGPU_H */
