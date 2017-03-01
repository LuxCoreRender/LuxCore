/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/thread/once.hpp>
#include <boost/thread/mutex.hpp>

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/virtualdevice.h"
#include "slg/slg.h"
#include "slg/engines/tilerepository.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/engines/oclrenderengine.h"
#include "slg/engines/tilepathocl/tilepathocl.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// FilmImpl
//------------------------------------------------------------------------------

FilmImpl::FilmImpl(const RenderSession &session) : renderSession(&session),
		standAloneFilm(NULL) {
}

FilmImpl::FilmImpl(const std::string &fileName) : renderSession(NULL) {
	standAloneFilm = slg::Film::LoadSerialized(fileName);
}

FilmImpl::~FilmImpl() {
	delete standAloneFilm;
}

slg::Film *FilmImpl::GetSLGFilm() const {
	if (renderSession)
		return renderSession->renderSession->film;
	else
		return standAloneFilm;
}

unsigned int FilmImpl::GetWidth() const {
	return GetSLGFilm()->GetWidth();
}

unsigned int FilmImpl::GetHeight() const {
	return GetSLGFilm()->GetHeight();
}

void FilmImpl::SaveOutputs() const {
	if (renderSession)
		renderSession->renderSession->SaveFilmOutputs();
	else
		throw runtime_error("Film::SaveOutputs() can not be used with a stand alone Film");
}

void FilmImpl::SaveOutput(const std::string &fileName, const FilmOutputType type, const Properties &props) const {
	GetSLGFilm()->Output(fileName, (slg::FilmOutputs::FilmOutputType)type, &props);
}

void FilmImpl::SaveFilm(const string &fileName) const {
	if (renderSession)
		renderSession->renderSession->SaveFilm(fileName);
	else
		slg::Film::SaveSerialized(fileName, standAloneFilm);
}

double FilmImpl::GetTotalSampleCount() const {
	return GetSLGFilm()->GetTotalSampleCount(); 
}

bool FilmImpl::HasOutput(const FilmOutputType type) const {
	return GetSLGFilm()->HasOutput((slg::FilmOutputs::FilmOutputType)type);
}

size_t FilmImpl::GetOutputSize(const FilmOutputType type) const {
	return GetSLGFilm()->GetOutputSize((slg::FilmOutputs::FilmOutputType)type);
}

unsigned int FilmImpl::GetRadianceGroupCount() const {
	return GetSLGFilm()->GetRadianceGroupCount();
}

void FilmImpl::GetOutputFloat(const FilmOutputType type, float *buffer, const unsigned int index) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->GetOutput<float>((slg::FilmOutputs::FilmOutputType)type, buffer, index);
	} else
		standAloneFilm->GetOutput<float>((slg::FilmOutputs::FilmOutputType)type, buffer, index);
}

void FilmImpl::GetOutputUInt(const FilmOutputType type, unsigned int *buffer, const unsigned int index) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->GetOutput<unsigned int>((slg::FilmOutputs::FilmOutputType)type, buffer, index);
	} else
		standAloneFilm->GetOutput<unsigned int>((slg::FilmOutputs::FilmOutputType)type, buffer, index);
}

unsigned int FilmImpl::GetChannelCount(const FilmChannelType type) const {
	return GetSLGFilm()->GetChannelCount((slg::Film::FilmChannelType)type);
}

const float *FilmImpl::GetChannelFloat(const FilmChannelType type, const unsigned int index) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		return renderSession->renderSession->film->GetChannel<float>((slg::Film::FilmChannelType)type, index);
	} else
		return standAloneFilm->GetChannel<float>((slg::Film::FilmChannelType)type, index);
}

const unsigned int *FilmImpl::GetChannelUInt(const FilmChannelType type, const unsigned int index) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		return renderSession->renderSession->film->GetChannel<unsigned int>((slg::Film::FilmChannelType)type, index);
	} else
		return standAloneFilm->GetChannel<unsigned int>((slg::Film::FilmChannelType)type, index);
}

void FilmImpl::Parse(const luxrays::Properties &props) {
	if (renderSession)
		throw runtime_error("Film::Parse() can be used only with a stand alone Film");
	else
		standAloneFilm->Parse(props);
}

//------------------------------------------------------------------------------
// CameraImpl
//------------------------------------------------------------------------------

CameraImpl::CameraImpl(const Scene &scn) : scene(scn) {
}

CameraImpl::~CameraImpl() {
}

const CameraImpl::CameraType CameraImpl::GetType() const {
	return (Camera::CameraType)scene.scene->camera->GetType();
}

void CameraImpl::Translate(const float x, const float y, const float z) const {
	scene.scene->camera->Translate(Vector(x, y, z));
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void CameraImpl::TranslateLeft(const float t) const {
	scene.scene->camera->TranslateLeft(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void CameraImpl::TranslateRight(const float t) const {
	scene.scene->camera->TranslateRight(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void CameraImpl::TranslateForward(const float t) const {
	scene.scene->camera->TranslateForward(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void CameraImpl::TranslateBackward(const float t) const {
	scene.scene->camera->TranslateBackward(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void CameraImpl::Rotate(const float angle, const float x, const float y, const float z) const {
	scene.scene->camera->Rotate(angle, Vector(x, y, z));
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void CameraImpl::RotateLeft(const float angle) const {
	scene.scene->camera->RotateLeft(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void CameraImpl::RotateRight(const float angle) const {
	scene.scene->camera->RotateRight(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void CameraImpl::RotateUp(const float angle) const {
	scene.scene->camera->RotateUp(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void CameraImpl::RotateDown(const float angle) const {
	scene.scene->camera->RotateDown(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}
