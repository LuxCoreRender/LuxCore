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

class Path {
public:
	enum PathState {
		EYE_VERTEX, EYE_VERTEX_VOLUME_STEP, NEXT_VERTEX, ONLY_SHADOW_RAYS,
		TRANSPARENT_SHADOW_RAYS_STEP, TRANSPARENT_ONLY_SHADOW_RAYS_STEP
	};

	Path(SLGScene *scene) {
		unsigned int shadowRayCount = GetMaxShadowRaysCount(scene);

		lightPdf = new float[shadowRayCount];
		lightColor = new Spectrum[shadowRayCount];
		shadowRay = new Ray[shadowRayCount];
		currentShadowRayIndex = new unsigned int[shadowRayCount];
		if (scene->volumeIntegrator)
			volumeComp = new VolumeComputation(scene->volumeIntegrator);
		else
			volumeComp = NULL;
	}

	~Path() {
		delete[] lightPdf;
		delete[] lightColor;
		delete[] shadowRay;
		delete[] currentShadowRayIndex;

		delete volumeComp;
	}

	void Init(SLGScene *scene, Film *film, Sampler *sampler) {
		throughput = Spectrum(1.f, 1.f, 1.f);
		radiance = Spectrum(0.f, 0.f, 0.f);
		sampler->GetNextSample(&sample);

		scene->camera->GenerateRay(
			sample.screenX, sample.screenY, film->GetWidth(), film->GetHeight(), &pathRay,
			sampler->GetLazyValue(&sample), sampler->GetLazyValue(&sample), sampler->GetLazyValue(&sample));
		state = EYE_VERTEX;
		depth = 0;
		specularBounce = true;
	}

	bool FillRayBuffer(SLGScene *scene, RayBuffer *rayBuffer);
	void AdvancePath(SLGScene *scene, Film *film, Sampler *sampler, const RayBuffer *rayBuffer,
			SampleBuffer *sampleBuffer);

private:
	static unsigned int GetMaxShadowRaysCount(const SLGScene *scene) {
		switch (scene->lightStrategy) {
			case ALL_UNIFORM:
				return scene->lights.size() * scene->shadowRayCount;
				break;
			case ONE_UNIFORM:
				return scene->shadowRayCount;
			default:
				throw runtime_error("Internal error in Path::GetMaxShadowRaysCount()");
		}
	}

	void CalcNextStep(SLGScene *scene, Sampler *sampler, const RayBuffer *rayBuffer,
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
	PathIntegrator(SLGScene *s, Film *f, Sampler *samp);
	~PathIntegrator();

	void ReInit();

	size_t PathCount() const { return paths.size(); }
	void ClearPaths();

	void FillRayBuffer(RayBuffer *rayBuffer);
	void AdvancePaths(const RayBuffer *rayBuffer);

	double statsRenderingStart;
	double statsTotalSampleCount;

private:
	SLGScene *scene;
	Film *film;
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
	PathRenderThread(unsigned int index, SLGScene *scn, Film *f);
	virtual ~PathRenderThread();

	virtual void Start() { started = true; }
    virtual void Interrupt() = 0;
	virtual void Stop() { started = false; }

	virtual void ClearPaths() = 0;
	virtual unsigned int GetPass() const = 0;

protected:
	unsigned int threadIndex;
	SLGScene *scene;
	Film *film;

	bool started;
};

class PathNativeRenderThread : public PathRenderThread {
public:
	PathNativeRenderThread(unsigned int index, unsigned long seedBase, const float samplingStart,
			const unsigned int samplePerPixel, NativeThreadIntersectionDevice *device, SLGScene *scn,
			Film *f, const bool lowLatency);
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
			IntersectionDevice *device, SLGScene *scn, Film *f,
			const bool lowLatency);
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
		const bool lowLatency, const unsigned int samplePerPixel);
	virtual ~PathRenderEngine();

	void Start();
	void Interrupt();
	void Stop();

	void Reset();

	unsigned int GetPass() const;
	unsigned int GetThreadCount() const;

private:
	vector<IntersectionDevice *> intersectionDevices;
	vector<PathRenderThread *> renderThreads;
};

#endif	/* _PATH_H */
