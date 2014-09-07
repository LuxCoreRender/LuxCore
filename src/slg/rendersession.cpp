/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include "slg/rendersession.h"

using namespace std;
using namespace luxrays;
using namespace slg;

string slg::SLG_LABEL = "SmallLuxGPU v" SLG_VERSION_MAJOR "." SLG_VERSION_MINOR " (LuxCore demo: http://www.luxrender.net)";
string slg::LUXVR_LABEL = "LuxVR v" SLG_VERSION_MAJOR "." SLG_VERSION_MINOR " (http://www.luxrender.net)";

void (*slg::LuxRays_DebugHandler)(const char *msg) = NULL;
void (*slg::SLG_DebugHandler)(const char *msg) = NULL;

// Empty debug handler
void slg::NullDebugHandler(const char *msg) {
}

RenderSession::RenderSession(RenderConfig *rcfg) {
	renderConfig = rcfg;
	started = false;
	editMode = false;

	const Properties &cfg = renderConfig->cfg;

	periodiceSaveTime = cfg.Get(Property("batch.periodicsave")(0.f)).Get<float>();
	lastPeriodicSave = WallClockTime();
	periodicSaveEnabled = (periodiceSaveTime > 0.f);

	//--------------------------------------------------------------------------
	// Create the Film
	//--------------------------------------------------------------------------

	film = renderConfig->AllocFilm(filmOutputs);

	//--------------------------------------------------------------------------
	// Create the RenderEngine
	//--------------------------------------------------------------------------

	renderEngine = renderConfig->AllocRenderEngine(film, &filmMutex);
}

RenderSession::~RenderSession() {
	if (editMode)
		EndSceneEdit();
	if (started)
		Stop();

	delete renderEngine;
	delete film;
}

void RenderSession::Start() {
	assert (!started);
	started = true;

	renderEngine->Start();
}

void RenderSession::Stop() {
	assert (started);
	started = false;

	renderEngine->Stop();
}

void RenderSession::BeginSceneEdit() {
	assert (started);
	assert (!editMode);

	renderEngine->BeginSceneEdit();

	editMode = true;
}

void RenderSession::EndSceneEdit() {
	assert (started);
	assert (editMode);

	// Make a copy of the edit actions
	const EditActionList editActions = renderConfig->scene->editActions;
	
	if ((renderEngine->GetEngineType() != RTPATHOCL) &&
			(renderEngine->GetEngineType() != RTBIASPATHOCL)) {
		SLG_LOG("[RenderSession] Edit actions: " << editActions);

		// RTPATHOCL and RTBIASPATHOCL handle film Reset on their own
		if (editActions.HasAnyAction())
			film->Reset();
	}

	renderEngine->EndSceneEdit(editActions);
	editMode = false;
}

bool RenderSession::HasDone() const {
	return renderEngine->HasDone();
}

bool RenderSession::NeedPeriodicFilmSave() {
	if (periodicSaveEnabled) {
		const double now = WallClockTime();
		if (now - lastPeriodicSave > periodiceSaveTime) {
			lastPeriodicSave = now;
			return true;
		} else
			return false;
	} else
		return false;
}

void RenderSession::SaveFilm() {
	// Ask the RenderEngine to update the film
	renderEngine->UpdateFilm();

	// renderEngine->UpdateFilm() uses the film lock on its own
	boost::unique_lock<boost::mutex> lock(filmMutex);

	// Save the film
	film->Output(filmOutputs);
}
