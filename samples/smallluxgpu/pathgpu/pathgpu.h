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

namespace PathGPU {

typedef struct {
	unsigned int s1, s2, s3;
} Seed;

typedef struct {
	Spectrum throughput;
	unsigned int depth, pixelIndex;
	Seed seed;
} Path;

typedef struct {
	Spectrum c;
	unsigned int count;
} Pixel;

#define MAT_MATTE 0

typedef struct {
	unsigned int type;
	union {
		struct {
			float r, g, b;
		} matte;
	} mat;
} Material;

}

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

	friend class PathGPURenderEngine;

protected:
	unsigned int threadIndex;
	PathGPURenderEngine *renderEngine;
	PathGPU::Pixel *frameBuffer;

	bool started;
};

class PathGPUDeviceRenderThread : public PathGPURenderThread {
public:
	PathGPUDeviceRenderThread(const unsigned int index, const unsigned int seedBase,
			const float samplingStart, const unsigned int samplePerPixel,
			OpenCLIntersectionDevice *device, PathGPURenderEngine *re);
	~PathGPUDeviceRenderThread();

	void Start();
    void Interrupt();
	void Stop();

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
	cl::Buffer *materialsBuff;
	cl::Buffer *meshIDBuff;
	cl::Buffer *triIDBuff;
	cl::Buffer *meshMatsBuff;

	float samplingStart;
	unsigned int seed;

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

	unsigned long long GetSamplesCount() const { return samplesCount; }
	double GetTotalSamplesSec() const {
		return (startTime == 0.0) ? 0.0 :
			samplesCount / (WallClockTime() - startTime);
	}
	double GetRenderingTime() const { return (startTime == 0.0) ? 0.0 : (WallClockTime() - startTime); }

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

	double startTime;
	unsigned long long samplesCount;
};

#endif

#endif	/* PATHGPU_H */
