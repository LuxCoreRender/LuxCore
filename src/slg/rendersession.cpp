/***************************************************************************
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

#include <boost/algorithm/string/predicate.hpp>

#include "slg/rendersession.h"
#include "slg/renderstate.h"
#include "luxrays/utils/safesave.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void (*slg::LuxRays_DebugHandler)(const char *msg) = NULL;
void (*slg::SLG_DebugHandler)(const char *msg) = NULL;

// Empty debug handler
void slg::NullDebugHandler(const char *msg) {
}

RenderSession::RenderSession(RenderConfig *rcfg, RenderState *startState, Film *startFilm) {
	renderConfig = rcfg;

	const double now = WallClockTime();
	lastPeriodicFilmOutputsSave = now;
	lastPeriodicFilmSave = now;
	lastResumeRenderingSave = now;

	//--------------------------------------------------------------------------
	// Create the Film
	//--------------------------------------------------------------------------

	film = renderConfig->AllocFilm();

	//--------------------------------------------------------------------------
	// Create the RenderEngine
	//--------------------------------------------------------------------------

	renderEngine = renderConfig->AllocRenderEngine();
	renderEngine->SetRenderState(startState, startFilm);
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
	if (film->IsInitiliazed()) {
		// I need to allocate a new film because the current one has already been
		// used. For instance, it can happen when stopping and starting the
		// same session.

		// Delete the old film
		delete film;
		film = NULL;

		// Create the new film
		film = renderConfig->AllocFilm();
	}

	renderEngine->Start(film, &filmMutex);
}

void RenderSession::Stop() {
	// Force the last update of periodic saves
	CheckPeriodicSave(true);

	renderEngine->Stop();
}

void RenderSession::BeginSceneEdit() {
	renderEngine->BeginSceneEdit();
}

void RenderSession::EndSceneEdit() {
	// Make a copy of the edit actions
	const EditActionList editActions = renderConfig->scene->editActions;
	
	if ((renderEngine->GetType() != RTPATHOCL) &&
			(renderEngine->GetType() != RTPATHCPU)) {
		SLG_LOG("[RenderSession] Edit actions: " << editActions);

		// RTPATHs handle film Reset on their own
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

bool RenderSession::HasPeriodicFilmOutputsSave() {
	const double period = renderConfig->GetProperty("periodicsave.film.outputs.period").Get<double>();

	return (period > 0.0);
}

bool RenderSession::HasPeriodicFilmSave() {
	const double period = renderConfig->GetProperty("periodicsave.film.period").Get<double>();

	return (period > 0.0);
}

bool RenderSession::HasResumeRenderingSave() {
	const double period = renderConfig->GetProperty("periodicsave.resumerendering.period").Get<double>();

	return (period > 0.0);
}

bool RenderSession::NeedPeriodicFilmOutputsSave(const bool force) {
	const double period = renderConfig->GetProperty("periodicsave.film.outputs.period").Get<double>();
	if (period > 0.0) {
		if (force)
			return true;

		const double now = WallClockTime();
		if (now - lastPeriodicFilmOutputsSave > period) {
			lastPeriodicFilmOutputsSave = now;
			return true;
		} else
			return false;
	} else
		return false;
}

bool RenderSession::NeedPeriodicFilmSave(const bool force) {
	const double period = renderConfig->GetProperty("periodicsave.film.period").Get<double>();
	if (period > 0.0) {
		if (force)
			return true;

		const double now = WallClockTime();
		if (now - lastPeriodicFilmSave > period) {
			lastPeriodicFilmSave = now;
			return true;
		} else
			return false;
	} else
		return false;
}

bool RenderSession::NeedResumeRenderingSave(const bool force) {
	const double period = renderConfig->GetProperty("periodicsave.resumerendering.period").Get<double>();
	if (period > 0.0) {
		if (force)
			return true;

		const double now = WallClockTime();
		if (now - lastResumeRenderingSave > period) {
			lastResumeRenderingSave = now;
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

	if (renderConfig->GetProperty("film.safesave").Get<bool>()) {
		SafeSave safeSave(fileName);

		Film::SaveSerialized(safeSave.GetSaveFileName(), film);

		safeSave.Process();
	} else
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

RenderState *RenderSession::GetRenderState() {
	// Check if we are in the right state
	if (!IsInPause())
		throw runtime_error("A rendering state can be retrieved only while the rendering session is paused");

	return renderEngine->GetRenderState();
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

		renderEngine->EndFilmEdit(film, &filmMutex);
	} else {
		boost::unique_lock<boost::mutex> lock(filmMutex);
		film->Parse(props);

		// Update render config properties
		renderConfig->UpdateFilmProperties(props);
	}
}

void RenderSession::CheckPeriodicSave(const bool force) {
	// Film outputs periodic save
	if (NeedPeriodicFilmOutputsSave(force))
		SaveFilmOutputs();

	// Film periodic save
	if (NeedPeriodicFilmSave(force)) {
		const string fileName = renderConfig->GetProperty("periodicsave.film.filename").Get<string>();

		SaveFilm(fileName);
	}

	// Rendering resume periodic save
	if (NeedResumeRenderingSave(force)) {
		// The .rsm file can be save only during a pause
		Pause();

		const string fileName = renderConfig->GetProperty("periodicsave.resumerendering.filename").Get<string>();
		SaveResumeFile(fileName);
		
		Resume();
	}
}

static size_t SaveRsmFile(RenderSession *renderSession, const std::string &fileName) {
	SerializationOutputFile sof(fileName);

	// Save the render configuration and the scene
	sof.GetArchive() << renderSession->renderConfig;

	// Save the render state
	RenderState *renderState = renderSession->GetRenderState();
	sof.GetArchive() << renderState;
	delete renderState;

	// Save the film
	sof.GetArchive() << renderSession->film;

	if (!sof.IsGood())
		throw runtime_error("Error while saving serialized render configuration: " + fileName);

	sof.Flush();

	return sof.GetPosition();
}

void RenderSession::SaveResumeFile(const string &fileName) {
	size_t fileSize;

	if (renderConfig->GetProperty("resumerendering.filesafe").Get<bool>()) {
		SafeSave safeSave(fileName);
		
		fileSize = SaveRsmFile(this, safeSave.GetSaveFileName());
	
		safeSave.Process();
	} else
		fileSize = SaveRsmFile(this, fileName);

	SLG_LOG("Render configuration saved: " << (fileSize / 1024) << " Kbytes");
}
