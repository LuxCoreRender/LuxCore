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

#ifndef _SPPM_H
#define	_SPPM_H

#include "smalllux.h"
#include "slgscene.h"
#include "renderengine.h"
#include "sppm/hitpoints.h"

#include "luxrays/core/intersectiondevice.h"

#include <boost/thread/thread.hpp>

class RenderingConfig;

//------------------------------------------------------------------------------
// SPPM render thread
//------------------------------------------------------------------------------

#define SPPM_DEVICE_RENDER_BUFFER_COUNT 4

class SPPMRenderEngine;

class SPPMRenderThread {
public:
	SPPMRenderThread(unsigned int index, SPPMRenderEngine *re);
	virtual ~SPPMRenderThread();

	virtual void Start() { started = true; }
    virtual void Interrupt() = 0;
	virtual void Stop() { started = false; }

protected:
	unsigned int threadIndex;
	SPPMRenderEngine *renderEngine;

	bool started;
};

class SPPMDeviceRenderThread : public SPPMRenderThread {
public:
	SPPMDeviceRenderThread(unsigned int index,  unsigned long seedBase,
			IntersectionDevice *device, SPPMRenderEngine *re);
	~SPPMDeviceRenderThread();

	void Start();
    void Interrupt();
	void Stop();

private:
	static void UpdateFilm(Film *film, boost::mutex *filmMutex,
			HitPoints *hitPoints, SampleBuffer *&sampleBuffer);
	static void InitPhotonPath(Scene *scene,
			luxrays::RandomGenerator *rndGen,
			PhotonPath *photonPath, Ray *ray,
			unsigned int *photonTracedPass);
	static void AdvancePhotonPaths(
		SPPMRenderEngine *renderEngine, HitPoints *hitPoints,
		Scene *scene, RandomGenerator *rndGen,
		RayBuffer *rayBuffer, std::vector<PhotonPath> &photonPaths);
	static void RenderThreadImpl(SPPMDeviceRenderThread *renderThread);

	IntersectionDevice *intersectionDevice;
	RayBuffer *rayBufferHitPoints;
	std::vector<RayBuffer *> rayBuffersList;
	std::vector<std::vector<PhotonPath> *> photonPathsList;

	boost::thread *renderThread;

	bool reportedPermissionError;
};

//------------------------------------------------------------------------------
// SPPM render engine
//------------------------------------------------------------------------------

class SPPMRenderEngine : public RenderEngine {
public:
	SPPMRenderEngine(SLGScene *scn, Film *flm, boost::mutex *filmMutex,
			vector<IntersectionDevice *> intersectionDev, const Properties &cfg);
	virtual ~SPPMRenderEngine();

	void Start();
	void Interrupt();
	void Stop();

	void Reset();

	unsigned int GetPass() const;
	unsigned int GetThreadCount() const;
	RenderEngineType GetEngineType() const { return SPPM; }

	unsigned int GetMaxEyePathDepth() const { return maxEyePathDepth; }
	unsigned int GetMaxPhotonPathDepth() const { return maxPhotonPathDepth; }
	unsigned int GetStocasticInterval() const { return stochasticInterval; }
	unsigned long long GetTotalPhotonCount() const { return photonTracedTotal + photonTracedPass; }
	double GetRenderingTime() const { return (startTime == 0.0) ? 0.0 : (WallClockTime() - startTime); }
	double GetTotalPhotonSec() const {
		return (startTime == 0.0) ? 0.0 :
			(photonTracedTotal + photonTracedPass) / (WallClockTime() - startTime);
	}

	friend class SPPMDeviceRenderThread;
	friend class HitPoints;

private:
	unsigned long seedBase;
	LookUpAccelType lookupAccelType;
	// double instead of float because photon counters declared as int 64bit
	double photonAlpha;
	float photonStartRadiusScale;
	unsigned int maxEyePathDepth;
	unsigned int maxPhotonPathDepth;
	unsigned int stochasticInterval;
	bool useDirectLightSampling;

	vector<IntersectionDevice *> intersectionDevices;
	vector<SPPMRenderThread *> renderThreads;

	boost::barrier *barrier;
	boost::barrier *barrierExit;

	double startTime;
	unsigned long long photonTracedTotal;
	unsigned int photonTracedPass;
	HitPoints *hitPoints;
	SampleBuffer *sampleBuffer;
};

#endif	/* _SPPM_H */
