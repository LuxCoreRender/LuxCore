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
#include "slg/engines/renderengineregistry.h"
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
	pauseMode = false;

	if (renderConfig->cfg.IsDefined("renderengine.seed")) {
		const u_int seed = Max(1u, renderConfig->cfg.Get("renderengine.seed").Get<u_int>());
		seedBaseGenerator.init(seed);
	}
	GenerateNewSeed();

	// Create LuxRays context
	const Properties cfgProps = renderConfig->ToProperties();
	ctx = new Context(LuxRays_DebugHandler ? LuxRays_DebugHandler : NullDebugHandler,
			Properties() <<
			cfgProps.Get("opencl.platform.index") <<
			cfgProps.GetAllProperties("accelerator.") <<
			cfgProps.GetAllProperties("context."));

	// Force a complete preprocessing
	renderConfig->scene->editActions.AddAllAction();
	renderConfig->scene->Preprocess(ctx, film->GetWidth(), film->GetHeight(), film->GetSubRegion());

	samplesCount = 0;
	elapsedTime = 0.0;
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

	const float epsilonMin = renderConfig->GetProperty("scene.epsilon.min").Get<float>();
	MachineEpsilon::SetMin(epsilonMin);
	const float epsilonMax = renderConfig->GetProperty("scene.epsilon.max").Get<float>();
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
			(editActions.Has(GEOMETRY_TRANS_EDIT) &&
			!renderConfig->scene->dataSet->DoesAllAcceleratorsSupportUpdate())) {
		// Stop all intersection devices
		ctx->Stop();

		// To avoid reference to the DataSet de-allocated inside UpdateDataSet()
		ctx->SetDataSet(NULL);
		
		contextStopped = true;
	} else
		contextStopped = false;

	// Pre-process scene data
	renderConfig->scene->Preprocess(ctx, film->GetWidth(), film->GetHeight(), film->GetSubRegion());

	if (contextStopped) {
		// Set the LuxRays DataSet
		ctx->SetDataSet(renderConfig->scene->dataSet);

		// Restart all intersection devices
		ctx->Start();
	} else if (renderConfig->scene->dataSet->DoesAllAcceleratorsSupportUpdate() &&
			editActions.Has(GEOMETRY_TRANS_EDIT)) {
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

void RenderEngine::Pause() {
	assert (!pauseMode);	
	pauseMode = true;
}

void RenderEngine::Resume() {
	assert (pauseMode);
	pauseMode = false;
}

void RenderEngine::BeginFilmEdit() {
	Stop();
}

void RenderEngine::EndFilmEdit(Film *flm) {
	// Update the film pointer
	film = flm;
	InitFilm();
	
	Start();
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

Properties RenderEngine::ToProperties() const {
	throw runtime_error("Called RenderEngine::ToProperties()");
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties RenderEngine::ToProperties(const Properties &cfg) {
	const string type = cfg.Get(Property("renderengine.type")(PathCPURenderEngine::GetObjectTag())).Get<string>();

	RenderEngineRegistry::ToProperties func;

	if (RenderEngineRegistry::STATICTABLE_NAME(ToProperties).Get(type, func)) {
		return func(cfg) <<
				Filter::ToProperties(cfg) <<
				cfg.Get(GetDefaultProps().Get("opencl.platform.index"));
	} else
		throw runtime_error("Unknown render engine type in RenderEngine::ToProperties(): " + type);
}

RenderEngine *RenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	const string type = rcfg->cfg.Get(Property("renderengine.type")(PathCPURenderEngine::GetObjectTag())).Get<string>();
	RenderEngineRegistry::FromProperties func;
	if (RenderEngineRegistry::STATICTABLE_NAME(FromProperties).Get(type, func))
		return func(rcfg, flm, flmMutex);
	else
		throw runtime_error("Unknown render engine type in RenderEngine::FromProperties(): " + type);
}

RenderEngineType RenderEngine::String2RenderEngineType(const string &type) {
	RenderEngineRegistry::GetObjectType func;
	if (RenderEngineRegistry::STATICTABLE_NAME(GetObjectType).Get(type, func))
		return func();
	else
		throw runtime_error("Unknown render engine type in RenderEngine::String2RenderEngineType(): " + type);
}

string RenderEngine::FromPropertiesOCL(const luxrays::Properties &cfg) {
	throw runtime_error("Called RenderEngine::FromPropertiesOCL()");
}

string RenderEngine::RenderEngineType2String(const RenderEngineType type) {
	RenderEngineRegistry::GetObjectTag func;
	if (RenderEngineRegistry::STATICTABLE_NAME(GetObjectTag).Get(type, func))
		return func();
	else
		throw runtime_error("Unknown render engine type in RenderEngine::RenderEngineType2String(): " + boost::lexical_cast<string>(type));
}

const Properties &RenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
		Property("opencl.platform.index")(-1);

	return props;
}

//------------------------------------------------------------------------------
// RenderEngineRegistry
//
// For the registration of each RenderEngine sub-class with RenderEngine StaticTables
//
// NOTE: you have to place all STATICTABLE_REGISTER() in the same .cpp file of the
// main base class (i.e. the one holding the StaticTable) because otherwise
// static members initialization order is not defined.
//------------------------------------------------------------------------------

OBJECTSTATICREGISTRY_STATICFIELDS(RenderEngineRegistry);

//------------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
OBJECTSTATICREGISTRY_REGISTER(RenderEngineRegistry, PathOCLRenderEngine);
#endif
OBJECTSTATICREGISTRY_REGISTER(RenderEngineRegistry, LightCPURenderEngine);
OBJECTSTATICREGISTRY_REGISTER(RenderEngineRegistry, PathCPURenderEngine);
OBJECTSTATICREGISTRY_REGISTER(RenderEngineRegistry, BiDirCPURenderEngine);
OBJECTSTATICREGISTRY_REGISTER(RenderEngineRegistry, BiDirVMCPURenderEngine);
OBJECTSTATICREGISTRY_REGISTER(RenderEngineRegistry, FileSaverRenderEngine);
#if !defined(LUXRAYS_DISABLE_OPENCL)
OBJECTSTATICREGISTRY_REGISTER(RenderEngineRegistry, RTPathOCLRenderEngine);
#endif
OBJECTSTATICREGISTRY_REGISTER(RenderEngineRegistry, BiasPathCPURenderEngine);
#if !defined(LUXRAYS_DISABLE_OPENCL)
OBJECTSTATICREGISTRY_REGISTER(RenderEngineRegistry, BiasPathOCLRenderEngine);
OBJECTSTATICREGISTRY_REGISTER(RenderEngineRegistry, RTBiasPathOCLRenderEngine);
#endif
OBJECTSTATICREGISTRY_REGISTER(RenderEngineRegistry, RTPathCPURenderEngine);
// Just add here any new RenderEngine (don't forget in the .h too)
