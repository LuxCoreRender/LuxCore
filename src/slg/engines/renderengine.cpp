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

#include <limits>
#include <boost/format.hpp>
#include <boost/thread/condition_variable.hpp>

#include "slg/renderconfig.h"
#include "slg/engines/renderengine.h"
#include "slg/bsdf/bsdf.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/tonemaps/linear.h"
#include "slg/film/imagepipeline/plugins/tonemaps/autolinear.h"

#include "luxrays/core/intersectiondevice.h"
#if !defined(LUXRAYS_DISABLE_OPENCL)
#include "luxrays/core/ocldevice.h"
#endif

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RenderEngine
//------------------------------------------------------------------------------

RenderEngine::RenderEngine(const RenderConfig *cfg, Film *flm, boost::mutex *flmMutex) :
	seedBaseGenerator(131) {
	renderConfig = cfg;
	pixelFilter = NULL;
	film = flm;
	filmMutex = flmMutex;
	started = false;
	editMode = false;
	GenerateNewSeed();

	film->AddChannel(Film::RGB_TONEMAPPED);

	// Create LuxRays context
	const int oclPlatformIndex = renderConfig->cfg.Get(Property("opencl.platform.index")(-1)).Get<int>();
	ctx = new Context(LuxRays_DebugHandler ? LuxRays_DebugHandler : NullDebugHandler, oclPlatformIndex);

	// Force a complete preprocessing
	renderConfig->scene->editActions.AddAllAction();
	renderConfig->scene->Preprocess(ctx, film->GetWidth(), film->GetHeight());

	samplesCount = 0;
	elapsedTime = 0.0f;
}

RenderEngine::~RenderEngine() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();

	delete ctx;

	delete pixelFilter;
	pixelFilter = NULL;
}

void RenderEngine::Start() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	assert (!started);
	started = true;

	delete pixelFilter;
	pixelFilter = renderConfig->AllocPixelFilter();

	const float epsilonMin = renderConfig->cfg.Get(Property("scene.epsilon.min")(DEFAULT_EPSILON_MIN)).Get<float>();
	MachineEpsilon::SetMin(epsilonMin);
	const float epsilonMax = renderConfig->cfg.Get(Property("scene.epsilon.max")(DEFAULT_EPSILON_MAX)).Get<float>();
	MachineEpsilon::SetMax(epsilonMax);

	ctx->Start();
	
	// Only at this point I can safely trace the auto-focus ray
	renderConfig->scene->camera->UpdateFocus(renderConfig->scene);

	StartLockLess();

	samplesCount = 0;
	elapsedTime = 0.0f;

	startTime = WallClockTime();
	film->ResetConvergenceTest();
	convergence = 0.f;
	lastConvergenceTestTime = startTime;
	lastConvergenceTestSamplesCount = 0;
}

void RenderEngine::Stop() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	StopLockLess();

	assert (started);
	started = false;

	ctx->Stop();

	UpdateFilmLockLess();

	delete pixelFilter;
	pixelFilter = NULL;
}

void RenderEngine::BeginSceneEdit() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	assert (started);
	assert (!editMode);
	editMode = true;

	BeginSceneEditLockLess();
}

void RenderEngine::EndSceneEdit(const EditActionList &editActions) {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	assert (started);
	assert (editMode);

	// Check if I have to stop the LuxRays Context
	bool contextStopped;
	if (editActions.Has(GEOMETRY_EDIT) ||
			(editActions.Has(INSTANCE_TRANS_EDIT) &&
			!renderConfig->scene->dataSet->DoesAllAcceleratorsSupportUpdate())) {
		// Stop all intersection devices
		ctx->Stop();

		// To avoid reference to the DataSet de-allocated inside UpdateDataSet()
		ctx->SetDataSet(NULL);
		
		contextStopped = true;
	} else
		contextStopped = false;

	// Pre-process scene data
	renderConfig->scene->Preprocess(ctx, film->GetWidth(), film->GetHeight());

	if (contextStopped) {
		// Set the LuxRays DataSet
		ctx->SetDataSet(renderConfig->scene->dataSet);

		// Restart all intersection devices
		ctx->Start();
	} else if (renderConfig->scene->dataSet->DoesAllAcceleratorsSupportUpdate() &&
			editActions.Has(INSTANCE_TRANS_EDIT)) {
		// Update the DataSet
		ctx->UpdateDataSet();
	}

	// Only at this point I can safely trace the auto-focus ray
	if (editActions.Has(CAMERA_EDIT))
		renderConfig->scene->camera->UpdateFocus(renderConfig->scene);

	samplesCount = 0;
	elapsedTime = 0.0f;

	startTime = WallClockTime();
	film->ResetConvergenceTest();
	convergence = 0.f;
	lastConvergenceTestTime = startTime;
	lastConvergenceTestSamplesCount = 0;

	editMode = false;

	EndSceneEditLockLess(editActions);
}

void RenderEngine::SetSeed(const unsigned long seed) {
	seedBaseGenerator.init(seed);

	GenerateNewSeed();
}

void RenderEngine::GenerateNewSeed() {
	seedBase = seedBaseGenerator.uintValue();
}

void RenderEngine::UpdateFilm() {
	boost::unique_lock<boost::mutex> lock(engineMutex);

	if (started) {
		UpdateFilmLockLess();
		UpdateCounters();

		const float haltthreshold = renderConfig->GetProperty("batch.haltthreshold").Get<float>();
		if (haltthreshold >= 0.f) {
			// Check if it is time to run the convergence test again
			const u_int imgWidth = film->GetWidth();
			const u_int imgHeight = film->GetHeight();
			const u_int pixelCount = imgWidth * imgHeight;
			const double now = WallClockTime();

			// Do not run the test if we don't have at least 16 new samples per pixel
			if ((samplesCount  - lastConvergenceTestSamplesCount > pixelCount * 16) &&
					((now - lastConvergenceTestTime) * 1000.0 >= renderConfig->GetProperty("screen.refresh.interval").Get<u_int>())) {
				convergence = 1.f - film->RunConvergenceTest() / (float)pixelCount;
				lastConvergenceTestTime = now;
				lastConvergenceTestSamplesCount = samplesCount;
			}
		}
	}
}

RenderEngineType RenderEngine::String2RenderEngineType(const string &type) {
	if ((type.compare("4") == 0) || (type.compare("PATHOCL") == 0))
		return PATHOCL;
	if ((type.compare("5") == 0) || (type.compare("LIGHTCPU") == 0))
		return LIGHTCPU;
	if ((type.compare("6") == 0) || (type.compare("PATHCPU") == 0))
		return PATHCPU;
	if ((type.compare("7") == 0) || (type.compare("BIDIRCPU") == 0))
		return BIDIRCPU;
	if ((type.compare("10") == 0) || (type.compare("BIDIRVMCPU") == 0))
		return BIDIRVMCPU;
	if ((type.compare("11") == 0) || (type.compare("FILESAVER") == 0))
		return FILESAVER;
	if ((type.compare("12") == 0) || (type.compare("RTPATHOCL") == 0))
		return RTPATHOCL;
	if ((type.compare("14") == 0) || (type.compare("BIASPATHCPU") == 0))
		return BIASPATHCPU;
	if ((type.compare("15") == 0) || (type.compare("BIASPATHOCL") == 0))
		return BIASPATHOCL;
	if ((type.compare("16") == 0) || (type.compare("RTBIASPATHOCL") == 0))
		return RTBIASPATHOCL;
	throw runtime_error("Unknown render engine type: " + type);
}

const string RenderEngine::RenderEngineType2String(const RenderEngineType type) {
	switch (type) {
		case PATHOCL:
			return "PATHOCL";
		case LIGHTCPU:
			return "LIGHTCPU";
		case PATHCPU:
			return "PATHCPU";
		case BIDIRCPU:
			return "BIDIRCPU";
		case BIDIRVMCPU:
			return "BIDIRVMCPU";
		case FILESAVER:
			return "FILESAVER";
		case RTPATHOCL:
			return "RTPATHOCL";
		case BIASPATHCPU:
			return "BIASPATHCPU";
		case BIASPATHOCL:
			return "BIASPATHOCL";
		case RTBIASPATHOCL:
			return "RTBIASPATHOCL";
		default:
			throw runtime_error("Unknown render engine type: " + boost::lexical_cast<string>(type));
	}
}
