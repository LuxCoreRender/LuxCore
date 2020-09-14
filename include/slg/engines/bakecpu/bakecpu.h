/**************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_BAKECPU_H
#define	_SLG_BAKECPU_H

#include "slg/slg.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/engines/pathtracer.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/samplers/sampler.h"
#include "slg/film/film.h"
#include "slg/film/filmsamplesplatter.h"
#include "slg/bsdf/bsdf.h"
#include "slg/utils/pathdepthinfo.h"

namespace slg {

//------------------------------------------------------------------------------
// Render baking PU render engine
//------------------------------------------------------------------------------

typedef struct {
	BakeMapType type;
	std::string fileName;
	u_int imagePipelineIndex;
	u_int width, height;
	u_int uvindex;

	std::vector<std::string> objectNames;

	bool useAutoMapSize;
} BakeMapInfo;

class BakeCPURenderEngine;

class BakeCPURenderThread : public CPUNoTileRenderThread {
public:
	BakeCPURenderThread(BakeCPURenderEngine *engine, const u_int index,
			luxrays::IntersectionDevice *device);

	friend class BakeCPURenderEngine;

protected:
	void InitBakeWork(const BakeMapInfo &mapInfo);
	void SetSampleResultXY(const BakeMapInfo &mapInfo, const HitPoint &hitPoint,
			const Film &film, SampleResult &sampleResult) const;
	void RenderEyeSample(const BakeMapInfo &mapInfo, PathTracerThreadState &state) const;
	void RenderConnectToEyeCallBack(const BakeMapInfo &mapInfo,
			const LightPathInfo &pathInfo, const BSDF &bsdf, const u_int lightID,
			const luxrays::Spectrum &lightPathFlux, std::vector<SampleResult> &sampleResults) const;
	void RenderLightSample(const BakeMapInfo &mapInfo, PathTracerThreadState &state) const;
	void RenderSample(const BakeMapInfo &mapInfo, PathTracerThreadState &state) const;
	void RenderFunc();

	virtual boost::thread *AllocRenderThread() { return new boost::thread(&BakeCPURenderThread::RenderFunc, this); }
};

class BakeCPURenderEngine : public CPUNoTileRenderEngine {
public:
	BakeCPURenderEngine(const RenderConfig *cfg);
	virtual ~BakeCPURenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	virtual RenderState *GetRenderState();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return BAKECPU; }
	static std::string GetObjectTag() { return "BAKECPU"; }
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(const RenderConfig *rcfg);

	friend class BakeCPURenderThread;

protected:
	static const luxrays::Properties &GetDefaultProps();

	CPURenderThread *NewRenderThread(const u_int index,
			luxrays::IntersectionDevice *device) {
		return new BakeCPURenderThread(this, index, device);
	}

	virtual void InitFilm();
	virtual void StartLockLess();
	virtual void StopLockLess();
	
	virtual void UpdateFilmLockLess();

	u_int minMapAutoSize, maxMapAutoSize;
	bool powerOf2AutoSize, skipExistingMapFiles;
	u_int marginPixels;
	float marginSamplesThreshold;
	std::vector<BakeMapInfo> mapInfos;

	PhotonGICache *photonGICache;
	FilmSampleSplatter *sampleSplatter;
	PathTracer pathTracer;
	SamplerSharedData *lightSamplerSharedData;

	Film *mapFilm;
	std::vector<const SceneObject *> currentSceneObjsToBake;
	std::vector<float> currentSceneObjsToBakeArea;
	luxrays::Distribution1D *currentSceneObjsDist;
	std::vector<luxrays::Distribution1D *> currentSceneObjDist;

	boost::barrier *threadsSyncBarrier;
};

}

#endif	/* _SLG_BAKECPU_H */
