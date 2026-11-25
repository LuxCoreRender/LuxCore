/**************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
#include "luxrays/usings.h"
#include "luxrays/utils/thread.h"
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
	BakeCPURenderThread(
		BakeCPURenderEngine *engine,
		const u_int index,
		luxrays::IntersectionDeviceRef device
	);

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
	void RenderFunc(std::stop_token stop_token);

	virtual luxrays::JThreadUPtr AllocRenderThread() {
		auto t = std::make_unique<luxrays::JThread>(
			std::bind_front(std::bind_front(&BakeCPURenderThread::RenderFunc, this))
		);
		luxrays::SetThreadName(t, "LxBakeCPU");
		return std::move(t);
	}
};

class BakeCPURenderEngine : public CPUNoTileRenderEngine {
public:
	BakeCPURenderEngine(RenderConfigRef cfg);
	virtual ~BakeCPURenderEngine();

	virtual RenderEngineType GetType() const { return GetObjectType(); }
	virtual std::string GetTag() const { return GetObjectTag(); }

	virtual RenderStateSPtr GetRenderState();

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	static RenderEngineType GetObjectType() { return BAKECPU; }
	static std::string GetObjectTag() { return "BAKECPU"; }
	static luxrays::PropertiesUPtr ToProperties(const luxrays::Properties &cfg);
	static RenderEngine *FromProperties(RenderConfigRef rcfg);

	friend class BakeCPURenderThread;
    struct completion_t {
        void operator()() noexcept { }
    };

	FilmRef GetMapFilm() { return *mapFilm; }
	FilmConstRef GetMapFilm() const { return *mapFilm; }

protected:
	static luxrays::PropertiesUPtr GetDefaultProps();

	CPURenderThreadUPtr NewRenderThread(
		const u_int index,
		luxrays::IntersectionDeviceRef device
	) override
	{
		return std::make_unique<BakeCPURenderThread>(this, index, device);
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
	PathTracer pathTracer;
	std::shared_ptr<SamplerSharedData> lightSamplerSharedData;

	std::vector<const SceneObject *> currentSceneObjsToBake;
	std::vector<float> currentSceneObjsToBakeArea;
	luxrays::Distribution1DUPtr currentSceneObjsDist;
	std::vector<luxrays::Distribution1DUPtr> currentSceneObjDist;

	std::barrier<completion_t> *threadsSyncBarrier;

private:
	FilmUPtr mapFilm;  // Owned by this engine
};

}

#endif	/* _SLG_BAKECPU_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
