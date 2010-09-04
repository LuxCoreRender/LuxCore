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
	Spectrum c;
	unsigned int count;
} Pixel;

#define MAT_MATTE 0
#define MAT_AREALIGHT 1
#define MAT_MIRROR 2
#define MAT_GLASS 3
#define MAT_MATTEMIRROR 4
#define MAT_METAL 5
#define MAT_MATTEMETAL 6
#define MAT_ALLOY 7
#define MAT_ARCHGLASS 8
#define MAT_NULL 9

typedef struct {
    float r, g, b;
} MatteParam;

typedef struct {
    float gain_r, gain_g, gain_b;
} AreaLightParam;

typedef struct {
    float r, g, b;
    int specularBounce;
} MirrorParam;

typedef struct {
    float refl_r, refl_g, refl_b;
    float refrct_r, refrct_g, refrct_b;
    float ousideIor, ior;
    float R0;
    int reflectionSpecularBounce, transmitionSpecularBounce;
} GlassParam;

typedef struct {
	MatteParam matte;
	MirrorParam mirror;
	float matteFilter, totFilter, mattePdf, mirrorPdf;
} MatteMirrorParam;

typedef struct {
    float r, g, b;
    float exponent;
    int specularBounce;
} MetalParam;

typedef struct {
	MatteParam matte;
	MetalParam metal;
	float matteFilter, totFilter, mattePdf, metalPdf;
} MatteMetalParam;

typedef struct {
    float diff_r, diff_g, diff_b;
	float refl_r, refl_g, refl_b;
    float exponent;
    float R0;
    int specularBounce;
} AlloyParam;

typedef struct {
    float refl_r, refl_g, refl_b;
    float refrct_r, refrct_g, refrct_b;
	float transFilter, totFilter, reflPdf, transPdf;
	bool reflectionSpecularBounce, transmitionSpecularBounce;
} ArchGlassParam;

typedef struct {
	unsigned int type;
	union {
		MatteParam matte;
        AreaLightParam areaLight;
		MirrorParam mirror;
        GlassParam glass;
		MatteMirrorParam matteMirror;
        MetalParam metal;
        MatteMetalParam matteMetal;
        AlloyParam alloy;
        ArchGlassParam archGlass;
	} param;
} Material;

typedef struct {
	Point v0, v1, v2;
	Normal normal;
	float area;
	float gain_r, gain_g, gain_b;
} TriangleLight;

typedef struct {
	unsigned int rgbOffset, alphaOffset;
	unsigned int width, height;
} TexMap;

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
	void UpdatePixelBuffer(const unsigned int screenRefreshInterval);

	friend class PathGPURenderEngine;

private:
	static void RenderThreadImpl(PathGPURenderThread *renderThread);

	void InitPixelBuffer();
	void InitRender();
	void EnqueueInitFrameBufferKernel(const bool clearPBO = false);

	OpenCLIntersectionDevice *intersectionDevice;

	// OpenCL variables
	string kernelsParameters;
	cl::Kernel *initKernel;
	size_t initWorkGroupSize;
	cl::Kernel *initFBKernel;
	size_t initFBWorkGroupSize;
	cl::Kernel *updatePBKernel;
	size_t updatePBWorkGroupSize;
	cl::Kernel *updatePBBluredKernel;
	size_t updatePBBluredWorkGroupSize;
	cl::Kernel *advancePathStep1Kernel;
	size_t advancePathStep1WorkGroupSize;
	cl::Kernel *advancePathStep2Kernel;
	size_t advancePathStep2WorkGroupSize;

	cl::Buffer *raysBuff;
	cl::Buffer *hitsBuff;
	cl::Buffer *pathsBuff;
	cl::Buffer *frameBufferBuff;
	cl::Buffer *materialsBuff;
	cl::Buffer *meshIDBuff;
	cl::Buffer *meshMatsBuff;
	cl::Buffer *infiniteLightBuff;
	cl::Buffer *normalsBuff;
	cl::Buffer *trianglesBuff;
	cl::Buffer *colorsBuff;
	cl::Buffer *cameraBuff;
	cl::Buffer *triLightsBuff;
	cl::Buffer *texMapRGBBuff;
	cl::Buffer *texMapAlphaBuff;
	cl::Buffer *texMapDescBuff;
	cl::Buffer *meshTexsBuff;
	cl::Buffer *uvsBuff;

	// Used only when OpenGL interoperability is enabled and only by the first
	// thread
	GLuint pbo;
	cl::BufferGL *pboBuff;
	double lastCameraUpdate;

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
	bool HasOpenGLInterop() const { return enableOpenGLInterop; }
	void UpdateCamera() { renderThreads[0]->UpdateCamera(); }
	void UpdatePixelBuffer(const unsigned int screenRefreshInterval) { renderThreads[0]->UpdatePixelBuffer(screenRefreshInterval); }

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

	double screenRefreshInterval; // in seconds

	double startTime;
	double elapsedTime;
	unsigned long long samplesCount;

	bool enableOpenGLInterop, dynamicCamera;
};

#endif

#endif	/* PATHGPU_H */
