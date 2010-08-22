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
	unsigned int depth, pixelIndex, subpixelIndex;
	Seed seed;
} Path;

typedef struct {
	Spectrum throughput;
	unsigned int depth, pixelIndex, subpixelIndex;
	Seed seed;

	int specularBounce;
	int state;

	Ray pathRay;
	RayHit pathHit;
	Spectrum lightRadiance;
	Spectrum accumRadiance;
} PathDL;

typedef struct {
	Spectrum throughput;
	unsigned int depth, pixelIndex;
	Seed seed;
} PathLowLatency;

typedef struct {
	Spectrum throughput;
	unsigned int depth, pixelIndex;
	Seed seed;

	int specularBounce;
	int state;

	Ray pathRay;
	RayHit pathHit;
	Spectrum lightRadiance;
	Spectrum accumRadiance;
} PathLowLatencyDL;

typedef struct {
	Spectrum c;
	unsigned int count;
} Pixel;

#define MAT_MATTE 0
#define MAT_AREALIGHT 1
#define MAT_MIRROR 2

typedef struct {
	unsigned int type;
	union {
		struct {
			float r, g, b;
		} matte;
		struct {
			float gain_r, gain_g, gain_b;
		} areaLight;
		struct {
			float r, g, b;
			int specularBounce;
		} mirror;
	} mat;
} Material;

typedef struct {
	Point v0, v1, v2;
	Normal normal;
	float area;
	float gain_r, gain_g, gain_b;
} TriangleLight;

}

//------------------------------------------------------------------------------
// Path Tracing GPU-only render threads
//------------------------------------------------------------------------------

#define PATHGPU_PATH_COUNT (2 * 65536)

class PathGPURenderThread {
public:
	PathGPURenderThread(const unsigned int index, const unsigned int seedBase,
			const float samplingStart, OpenCLIntersectionDevice *device,
			PathGPURenderEngine *re);
	~PathGPURenderThread();

	void Start();
    void Interrupt();
	void Stop();

	// Used for Low Latency mode
	void UpdateCamera();
	void UpdatePixelBuffer();

	friend class PathGPURenderEngine;

private:
	static void RenderThreadImpl(PathGPURenderThread *renderThread);

	void InitPixelBuffer();

	OpenCLIntersectionDevice *intersectionDevice;

	// OpenCL variables
	cl::Kernel *initKernel;
	size_t initWorkGroupSize;
	cl::Kernel *initFBKernel;
	size_t initFBWorkGroupSize;
	cl::Kernel *updatePBKernel;
	size_t updatePBWorkGroupSize;
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
	cl::Buffer *infiniteLightBuff;
	cl::Buffer *normalsBuff;
	cl::Buffer *trianglesBuff;
	cl::Buffer *colorsBuff;
	cl::Buffer *cameraBuff;
	cl::Buffer *triLightsBuff;

	// Used only when OpenGL interoperability is enabled and only by the first
	// thread
	GLuint pbo;
	cl::BufferGL *pboBuff;

	float samplingStart;
	unsigned int seed;

	boost::thread *renderThread;

	unsigned int threadIndex;
	PathGPURenderEngine *renderEngine;
	PathGPU::Pixel *frameBuffer;

	bool started, reportedPermissionError;
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

	unsigned int GetPass() const;
	unsigned int GetThreadCount() const;
	RenderEngineType GetEngineType() const { return PATHGPU; }

	double GetTotalSamplesSec() const {
		return (elapsedTime == 0.0) ? 0.0 : (samplesCount / elapsedTime);
	}
	double GetRenderingTime() const { return (startTime == 0.0) ? 0.0 : (WallClockTime() - startTime); }

	// Used for Low Latency mode
	bool HasOpenGLInterop() const { return hasOpenGLInterop; }
	void UpdateCamera() { renderThreads[0]->UpdateCamera(); }
	void UpdatePixelBuffer() { renderThreads[0]->UpdatePixelBuffer(); }

	void UpdateFilm();

	friend class PathGPURenderThread;

	unsigned int samplePerPixel;

	// Signed because of the delta parameter
	int maxPathDepth;

	int rrDepth;
	float rrImportanceCap;

private:
	vector<OpenCLIntersectionDevice *> oclIntersectionDevices;
	vector<PathGPURenderThread *> renderThreads;
	SampleBuffer *sampleBuffer;

	double startTime;
	double elapsedTime;
	unsigned long long samplesCount;
	bool hasOpenGLInterop;
};

#endif

#endif	/* PATHGPU_H */
