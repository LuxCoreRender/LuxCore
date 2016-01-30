/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#ifndef _SLG_RENDERENGINE_H
#define	_SLG_RENDERENGINE_H

#include <deque>
#include <boost/heap/priority_queue.hpp>
#include <boost/thread/condition_variable.hpp>

#include "luxrays/core/utils.h"

#include "slg/slg.h"
#include "slg/renderconfig.h"
#include "slg/editaction.h"
#include "slg/film/film.h"
#include "slg/bsdf/bsdf.h"
#include "slg/utils/varianceclamping.h"

namespace slg {

typedef enum {
	PATHOCL,
	LIGHTCPU,
	PATHCPU,
	BIDIRCPU,
	BIDIRVMCPU,
	FILESAVER,
	RTPATHOCL,
	BIASPATHCPU,
	BIASPATHOCL,
	RTBIASPATHOCL,
	RENDER_ENGINE_TYPE_COUNT
} RenderEngineType;

//------------------------------------------------------------------------------
// Base class for render engines
//------------------------------------------------------------------------------

class RenderEngine {
public:
	RenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex);
	virtual ~RenderEngine();

	bool IsStarted() const { return started; }
	virtual void Start();
	virtual void Stop();

	bool IsInSceneEdit() const { return editMode; }
	virtual void BeginSceneEdit();
	virtual void EndSceneEdit(const EditActionList &editActions);

	virtual void BeginFilmEdit();
	virtual void EndFilmEdit(Film *flm);

	bool IsInPause() const { return pauseMode; }
	virtual void Pause();
	virtual void Resume();

	virtual bool HasDone() const = 0;
	virtual void WaitForDone() const = 0;

	void UpdateFilm();
	virtual void WaitNewFrame() { }

	virtual RenderEngineType GetType() const = 0;
	virtual std::string GetTag() const = 0;

	void SetSeed(const unsigned long seed);
	void GenerateNewSeed();

	virtual bool IsMaterialCompiled(const MaterialType type) const {
		return true;
	}

	const vector<luxrays::IntersectionDevice *> &GetIntersectionDevices() const {
		return intersectionDevices;
	}

	const std::vector<luxrays::DeviceDescription *> &GetAvailableDeviceDescriptions() const {
		return ctx->GetAvailableDeviceDescriptions();
	}

	//--------------------------------------------------------------------------
	// Statistics related methods
	//--------------------------------------------------------------------------

	u_int GetPass() const {
		return static_cast<u_int>(samplesCount / (film->GetWidth() * film->GetHeight()));
	}
	double GetTotalSampleCount() const { return samplesCount; }
	float GetConvergence() const { return convergence; }
	double GetTotalSamplesSec() const {
		return (elapsedTime == 0.0) ? 0.0 : (samplesCount / elapsedTime);
	}
	double GetTotalRaysSec() const { return (elapsedTime == 0.0) ? 0.0 : (raysCount / elapsedTime); }
	double GetRenderingTime() const { return elapsedTime; }

	//--------------------------------------------------------------------------

	static float RussianRouletteProb(const luxrays::Spectrum &color, const float cap) {
		return luxrays::Clamp(color.Filter(), cap, 1.f);
	}

	// Transform the current object in Properties
	virtual luxrays::Properties ToProperties() const;

	//--------------------------------------------------------------------------
	// Static methods used by RenderEngineRegistry
	//--------------------------------------------------------------------------

	// This method is not used at the moment
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	// Allocate a Object based on the cfg definition
	static RenderEngine *FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex);
	// This method is not used at the moment
	static std::string FromPropertiesOCL(const luxrays::Properties &cfg);

	static RenderEngineType String2RenderEngineType(const std::string &type);
	static std::string RenderEngineType2String(const RenderEngineType type);

protected:
	static const luxrays::Properties &GetDefaultProps();

	virtual void InitFilm() = 0;
	virtual void StartLockLess() = 0;
	virtual void StopLockLess() = 0;

	virtual void BeginSceneEditLockLess() = 0;
	virtual void EndSceneEditLockLess(const EditActionList &editActions) = 0;

	virtual void UpdateFilmLockLess() = 0;
	virtual void UpdateCounters() = 0;

	int oclPlatformIndex;

	boost::mutex engineMutex;
	luxrays::Context *ctx;
	vector<luxrays::DeviceDescription *> selectedDeviceDescs;
	vector<luxrays::IntersectionDevice *> intersectionDevices;

	const RenderConfig *renderConfig;
	Filter *pixelFilter;
	Film *film;
	boost::mutex *filmMutex;

	u_int seedBase;
	luxrays::RandomGenerator seedBaseGenerator;

	double startTime, elapsedTime;
	double samplesCount, raysCount;

	float convergence;
	double lastConvergenceTestTime;
	double lastConvergenceTestSamplesCount;

	bool started, editMode, pauseMode;
};

}

#endif	/* _SLG_RENDERENGINE_H */
