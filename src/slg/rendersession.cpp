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

#include <boost/algorithm/string/predicate.hpp>

#include "slg/rendersession.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void (*slg::LuxRays_DebugHandler)(const char *msg) = NULL;
void (*slg::SLG_DebugHandler)(const char *msg) = NULL;

// Empty debug handler
void slg::NullDebugHandler(const char *msg) {
}

RenderSession::RenderSession(RenderConfig *rcfg) {
	renderConfig = rcfg;

	periodiceSaveTime = renderConfig->cfg.Get(Property("batch.periodicsave")(0.f)).Get<float>();
	lastPeriodicSave = WallClockTime();
	periodicSaveEnabled = (periodiceSaveTime > 0.f);

	//--------------------------------------------------------------------------
	// Create the Film
	//--------------------------------------------------------------------------

	film = renderConfig->AllocFilm();

	//--------------------------------------------------------------------------
	// Create the RenderEngine
	//--------------------------------------------------------------------------

	renderEngine = renderConfig->AllocRenderEngine(film, &filmMutex);
}

RenderSession::~RenderSession() {
	if (renderEngine->IsInSceneEdit())
		EndSceneEdit();
	if (renderEngine->IsStarted())
		Stop();

	delete renderEngine;
	delete film;
}

void RenderSession::Start() {
	renderEngine->Start();
}

void RenderSession::Stop() {
	renderEngine->Stop();
}

void RenderSession::BeginSceneEdit() {
	renderEngine->BeginSceneEdit();
}

void RenderSession::EndSceneEdit() {
	// Make a copy of the edit actions
	const EditActionList editActions = renderConfig->scene->editActions;
	
	if ((renderEngine->GetType() != RTPATHOCL) &&
			(renderEngine->GetType() != RTBIASPATHOCL)) {
		SLG_LOG("[RenderSession] Edit actions: " << editActions);

		// RTPATHOCL and RTBIASPATHOCL handle film Reset on their own
		if (editActions.HasAnyAction())
			film->Reset();
	}

	renderEngine->EndSceneEdit(editActions);
}

void RenderSession::Pause() {
	renderEngine->Pause();
}

void RenderSession::Resume() {
	renderEngine->Resume();
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

void RenderSession::SaveFilm(const string &fileName) {
	SLG_LOG("Saving film: " << fileName);

	// Ask the RenderEngine to update the film
	renderEngine->UpdateFilm();

	// renderEngine->UpdateFilm() uses the film lock on its own
	boost::unique_lock<boost::mutex> lock(filmMutex);

	// Serialize the film
	Film::SaveSerialized(fileName, film);
}

void RenderSession::SaveFilmOutputs() {
	// Ask the RenderEngine to update the film
	renderEngine->UpdateFilm();

	// renderEngine->UpdateFilm() uses the film lock on its own
	boost::unique_lock<boost::mutex> lock(filmMutex);

	// Save the film
	film->Output();
}

void RenderSession::Parse(const luxrays::Properties &props) {
	assert (renderEngine->IsStarted());

	if ((props.IsDefined("film.width") && (props.Get("film.width").Get<u_int>() != film->GetWidth())) ||
			(props.IsDefined("film.height") && (props.Get("film.height").Get<u_int>() != film->GetHeight()))) {
		// I have to use a special procedure if the parsed props include
		// a film resize
		renderEngine->BeginFilmEdit();

		// Update render config properties
		renderConfig->UpdateFilmProperties(props);

		// Delete the old film
		delete film;
		film = NULL;

		// Create the new film
		film = renderConfig->AllocFilm();

		// I have to update the camera
		renderConfig->scene->PreprocessCamera(film->GetWidth(), film->GetHeight(), film->GetSubRegion());

		renderEngine->EndFilmEdit(film);
	} else {
		boost::unique_lock<boost::mutex> lock(filmMutex);
		film->Parse(props);

		// Update render config properties
		renderConfig->UpdateFilmProperties(props);
	}
}
