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

#ifndef _PATH_H
#define	_PATH_H

#include "smalllux.h"
#include "slgscene.h"
#include "volume.h"
#include "renderengine.h"
#include "luxrays/core/intersectiondevice.h"

#include <boost/thread/thread.hpp>

class RenderingConfig;

//------------------------------------------------------------------------------
// Path
//------------------------------------------------------------------------------

class PathRenderEngine;

class Path {
public:
	enum PathState {
		EYE_VERTEX, EYE_VERTEX_VOLUME_STEP, NEXT_VERTEX, ONLY_SHADOW_RAYS,
		TRANSPARENT_SHADOW_RAYS_STEP, TRANSPARENT_ONLY_SHADOW_RAYS_STEP
	};

	Path(PathRenderEngine *renderEngine);
	~Path();

	void Init(PathRenderEngine *renderEngine, Sampler *sampler);

	bool FillRayBuffer(RayBuffer *rayBuffer);
	void AdvancePath(PathRenderEngine *renderEngine, Sampler *sampler, const RayBuffer *rayBuffer,
			SampleBuffer *sampleBuffer);

private:
	static unsigned int GetMaxShadowRaysCount(PathRenderEngine *renderEngine);

	void CalcNextStep(PathRenderEngine *renderEngine, Sampler *sampler, const RayBuffer *rayBuffer,
		SampleBuffer *sampleBuffer);

	Sample sample;

	Spectrum throughput, radiance;
	int depth;

	float *lightPdf;
	Spectrum *lightColor;

	// Used to save eye hit during EYE_VERTEX_VOLUME_STEP
	RayHit eyeHit;

	Ray pathRay, *shadowRay;
	unsigned int currentPathRayIndex, *currentShadowRayIndex;
	unsigned int tracedShadowRayCount;
	VolumeComputation *volumeComp;

	PathState state;
	bool specularBounce;
};

//------------------------------------------------------------------------------
// Path Integrator
//------------------------------------------------------------------------------

class PathIntegrator {
public:
	PathIntegrator(PathRenderEngine *renderEngine, Sampler *samp);
	~PathIntegrator();

	void ReInit();

	size_t PathCount() const { return paths.size(); }
	void ClearPaths();

	void FillRayBuffer(RayBuffer *rayBuffer);
	void AdvancePaths(const RayBuffer *rayBuffer);

	double statsRenderingStart;
	double statsTotalSampleCount;

private:
	PathRenderEngine *renderEngine;
	Sampler *sampler;

	SampleBuffer *sampleBuffer;
	int firstPath, lastPath;
	vector<Path *> paths;
};

//------------------------------------------------------------------------------
// Path Tracing render threads
//------------------------------------------------------------------------------

#define PATH_DEVICE_RENDER_BUFFER_COUNT 4

class PathRenderThread {
public:
	PathRenderThread(unsigned int index, PathRenderEngine *re);
	virtual ~PathRenderThread();

	virtual void Start() { started = true; }
    virtual void Interrupt() = 0;
	virtual void Stop() { started = false; }

	virtual void ClearPaths() = 0;
	virtual unsigned int GetPass() const = 0;

protected:
	unsigned int threadIndex;
	PathRenderEngine *renderEngine;

	bool started;
};

class PathNativeRenderThread : public PathRenderThread {
public:
	PathNativeRenderThread(unsigned int index, unsigned long seedBase, const float samplingStart,
			const unsigned int samplePerPixel, NativeThreadIntersectionDevice *device, PathRenderEngine *re);
	~PathNativeRenderThread();

	void Start();
    void Interrupt();
	void Stop();

	void ClearPaths();

	unsigned int GetPass() const { return sampler->GetPass(); }

private:
	static void RenderThreadImpl(PathNativeRenderThread *renderThread);

	NativeThreadIntersectionDevice *intersectionDevice;

	float samplingStart;
	Sampler *sampler;
	PathIntegrator *pathIntegrator;
	RayBuffer *rayBuffer;

	boost::thread *renderThread;
};

class PathDeviceRenderThread : public PathRenderThread {
public:
	PathDeviceRenderThread(unsigned int index,  unsigned long seedBase,
			const float samplingStart, const unsigned int samplePerPixel,
			IntersectionDevice *device, PathRenderEngine *re);
	~PathDeviceRenderThread();

	void Start();
    void Interrupt();
	void Stop();

	void ClearPaths();

	unsigned int GetPass() const { return sampler->GetPass(); }

private:
	static void RenderThreadImpl(PathDeviceRenderThread *renderThread);

	IntersectionDevice *intersectionDevice;

	float samplingStart;
	Sampler *sampler;
	PathIntegrator *pathIntegrators[PATH_DEVICE_RENDER_BUFFER_COUNT];
	RayBuffer *rayBuffers[PATH_DEVICE_RENDER_BUFFER_COUNT];

	boost::thread *renderThread;

	bool reportedPermissionError;
};

//------------------------------------------------------------------------------
// Path Tracing render engine
//------------------------------------------------------------------------------

class PathRenderEngine : public RenderEngine {
public:
	PathRenderEngine(SLGScene *scn, Film *flm, vector<IntersectionDevice *> intersectionDev,
		const bool onlySpecular, const Properties &cfg);
	virtual ~PathRenderEngine();

	void Start();
	void Interrupt();
	void Stop();

	void Reset();

	unsigned int GetPass() const;
	unsigned int GetThreadCount() const;
	RenderEngineType GetEngineType() const { return (onlySampleSpecular) ? DIRECTLIGHT : PATH; }

	friend class Path;
	friend class PathIntegrator;
	friend class PathNativeRenderThread;
	friend class PathDeviceRenderThread;

	unsigned int samplePerPixel;
	bool lowLatency;

	// Signed because of the delta parameter
	int maxPathDepth;
	bool onlySampleSpecular;
	DirectLightStrategy lightStrategy;
	unsigned int shadowRayCount;

	RussianRouletteStrategy rrStrategy;
	int rrDepth;
	float rrProb; // Used by PROBABILITY strategy
	float rrImportanceCap; // Used by IMPORTANCE strategy

private:
	vector<IntersectionDevice *> intersectionDevices;
	vector<PathRenderThread *> renderThreads;
};

#endif	/* _PATH_H */
