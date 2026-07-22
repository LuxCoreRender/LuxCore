/***************************************************************************
 * Copyright 1998-2025 by authors (see AUTHORS.txt)                        *
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

#include <boost/format.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <memory>
#include <stdexcept>
#include <typeinfo>

#include "luxcore/luxcorelogger.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/fileext.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/serializationutils.h"
#include "luxrays/utils/safesave.h"
#include "slg/slg.h"
#include "slg/renderconfig.h"
#include "slg/rendersession.h"
#include "slg/engines/tilerepository.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/engines/oclrenderengine.h"
#include "slg/engines/tilepathocl/tilepathocl.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/engines/filesaver/filesaver.h"
#include "slg/film/imagepipeline/plugins/intel_oidn.h"
#include "luxcore/luxcore.h"
#include "slg/usings.h"
#include "luxcore/luxcoreimpl.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;
using namespace luxcore::detail;

//------------------------------------------------------------------------------
// FilmImpl
//------------------------------------------------------------------------------

// Standalone film
std::unique_ptr<FilmImpl> FilmImpl::Create(const std::string &fileName) {
	return std::make_unique<FilmImplStandalone>(fileName);
}
std::unique_ptr<FilmImpl> FilmImpl::Create(
	luxrays::PropertiesRPtr props,
	const bool hasPixelNormalizedChannel,
	const bool hasScreenNormalizedChannel
) {
	return std::make_unique<FilmImplStandalone>(
		props, hasPixelNormalizedChannel, hasScreenNormalizedChannel
	);
}
// The below factory is mainly intended for deserialization, when
// the archive yields a FilmUPtr
std::unique_ptr<FilmImpl> FilmImpl::Create(slg::FilmUPtr&& film) {
	auto res = std::make_unique<FilmImplStandalone>();
	res->standAloneFilm = std::move(film);
	return res;
}


// Session film
std::unique_ptr<FilmImpl> FilmImpl::Create(RenderSessionImplRef session) {
	return std::make_unique<FilmImplSession>(session);
}

unsigned int FilmImpl::GetWidth() const {
	API_BEGIN_NOARGS();

	const unsigned int result = GetSLGFilm().GetWidth();

	API_RETURN("{}", result);

	return result;
}

unsigned int FilmImpl::GetHeight() const {
	API_BEGIN_NOARGS();

	const unsigned int result = GetSLGFilm().GetHeight();

	API_RETURN("{}", result);

	return result;
}

PropertiesUPtr FilmImpl::GetStats() const {
	API_BEGIN_NOARGS();

	//std::unique_ptr<slg::Film> film = std::make_unique<slg::Film>(
		
		//GetSLGFilm()
	//);
	auto& film = GetSLGFilm();

	auto stats = std::make_unique<Properties>();

	stats->Set(Property("stats.film.total.samplecount")(film.GetTotalSampleCount()));
	stats->Set(Property("stats.film.spp")(film.GetTotalSampleCount() / static_cast<float>(film.GetWidth() * film.GetHeight())));
	stats->Set(Property("stats.film.radiancegorup.count")(film.GetRadianceGroupCount()));

	API_RETURN("{}", ToArgString(stats));

	return stats;
}

float FilmImpl::GetFilmY(const u_int imagePipelineIndex) const {
	API_BEGIN_NOARGS();

	const float result = GetSLGFilm().GetFilmY(imagePipelineIndex);

	API_RETURN("{}", result);

	return result;
}

void FilmImpl::Clear() {
	API_BEGIN_NOARGS();

	GetSLGFilm().Clear();

	API_END();
}

void FilmImpl::AddFilm(FilmConstRef film) {
	auto& filmImpl = dynamic_cast<const FilmImpl&>(film);

	API_BEGIN("{}", (void *)std::addressof(filmImpl));

	AddFilm(film, 0, 0, filmImpl.GetWidth(), filmImpl.GetHeight(), 0, 0);

	API_END();
}

void FilmImpl::AddFilm(FilmConstRef film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY) {
	auto& srcFilmImpl = dynamic_cast<const FilmImpl&>(film);

	API_BEGIN("{}, {}, {}, {}, {}, {}, {}", (void *)std::addressof(srcFilmImpl), srcOffsetX, srcOffsetY, srcWidth, srcHeight, dstOffsetX, dstOffsetY);

	const FilmImpl& dstFilmImpl = *this;

	// I have to clip the parameters to avoid an out of bound memory access

	// Check the cases where I have nothing to do
	if (srcOffsetX >= srcFilmImpl.GetWidth())
		return;
	if (srcOffsetY >= srcFilmImpl.GetHeight())
		return;
	if (dstOffsetX >= dstFilmImpl.GetWidth())
		return;
	if (dstOffsetY >= dstFilmImpl.GetHeight())
		return;

	u_int clippedSrcWidth;
	// Clip with the src film
	clippedSrcWidth = Min(srcOffsetX + srcWidth, srcFilmImpl.GetWidth()) - srcOffsetX;
	// Clip with the dst film
	clippedSrcWidth = Min(dstOffsetX + clippedSrcWidth, dstFilmImpl.GetWidth()) - dstOffsetX;

	u_int clippedSrcHeight;
	// Clip with the src film
	clippedSrcHeight = Min(srcOffsetY + srcHeight, srcFilmImpl.GetHeight()) - srcOffsetY;
	// Clip with the dst film
	clippedSrcHeight = Min(dstOffsetY + clippedSrcHeight, dstFilmImpl.GetHeight()) - dstOffsetY;

	GetSLGFilm().AddFilm(srcFilmImpl.GetSLGFilm(), srcOffsetX, srcOffsetY,
			clippedSrcWidth, clippedSrcHeight, dstOffsetX, dstOffsetY);

	API_END();
}

void FilmImpl::SaveOutput(
	const std::string &fileName,
	const FilmOutputType type,
	PropertiesRPtr props
) const {
	API_BEGIN("{}, {}, {}", ToArgString(fileName),ToArgString(type), ToArgString(*props));

	GetSLGFilm().Output(
		fileName,
		static_cast<slg::FilmOutputs::FilmOutputType>(type),
		props
	);

	API_END();
}

double FilmImpl::GetTotalSampleCount() const {
	API_BEGIN_NOARGS();

	const double result = GetSLGFilm().GetTotalSampleCount();

	API_RETURN("{}", result);

	return result;
}

bool FilmImpl::HasOutput(const FilmOutputType type) const {
	API_BEGIN("{}", ToArgString(type));

	const bool result = GetSLGFilm().HasOutput(static_cast<slg::FilmOutputs::FilmOutputType>(type));

	API_RETURN("{}", result);

	return result;
}

unsigned int FilmImpl::GetOutputCount(const FilmOutputType type) const {
	API_BEGIN("{}", ToArgString(type));

	const unsigned int result = GetSLGFilm().GetOutputCount(static_cast<slg::FilmOutputs::FilmOutputType>(type));

	API_RETURN("{}", result);

	return result;
}

size_t FilmImpl::GetOutputSize(const FilmOutputType type) const {
	API_BEGIN("{}", ToArgString(type));

	const size_t result = GetSLGFilm().GetOutputSize(
		static_cast<slg::FilmOutputs::FilmOutputType>(type)
	);

	API_RETURN("{}", result);

	return result;
}

unsigned int FilmImpl::GetRadianceGroupCount() const {
	API_BEGIN_NOARGS();

	const unsigned int result = GetSLGFilm().GetRadianceGroupCount();

	API_RETURN("{}", result);

	return result;
}

void FilmImpl::UpdateOutputUInt(const FilmOutputType type, const unsigned int *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	throw runtime_error("No channel can be updated with Film::UpdateOutput<unsigned int>()");

	API_END();
}

bool FilmImpl::HasChannel(const FilmChannelType type) const {
	API_BEGIN("{}", ToArgString(type));

	const bool result = GetSLGFilm().HasChannel(static_cast<slg::Film::FilmChannelType>(type));

	API_RETURN("{}", result);

	return result;
}

unsigned int FilmImpl::GetChannelCount(const FilmChannelType type) const {
	API_BEGIN("{}", ToArgString(type));

	const unsigned int result = GetSLGFilm().GetChannelCount(static_cast<slg::Film::FilmChannelType>(type));

	API_RETURN("{}", result);

	return result;
}

unsigned int *FilmImpl::UpdateChannelUInt(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	throw runtime_error(
		"No channel can be updated with Film::UpdateChannel<unsigned int>()"
	);

	API_END();
}

//------------------------------------------------------------------------------
// FilmImplSession
//------------------------------------------------------------------------------

FilmImplSession::FilmImplSession(RenderSessionImplRef session) :
	renderSession(session)
{}

void FilmImplSession::GetOutputFloat(const FilmOutputType type, float *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);

	renderSession.GetSLGRenderSession().film->GetOutput<float>(
		static_cast<slg::FilmOutputs::FilmOutputType>(type),
		buffer, index, executeImagePipeline
	);

	API_END();
}

void FilmImplSession::GetOutputUInt(const FilmOutputType type, unsigned int *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);

	renderSession.GetSLGRenderSession().film->GetOutput<u_int>(
		static_cast<slg::FilmOutputs::FilmOutputType>(type),
		buffer, index, executeImagePipeline
	);

	API_END();
}

void FilmImplSession::UpdateOutputFloat(const FilmOutputType type, const float *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	if (type != OUTPUT_USER_IMPORTANCE)
		throw runtime_error("Currently, only USER_IMPORTANCE channel can be updated with Film::UpdateOutput<float>()");

	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);

	const auto& film = renderSession.GetSLGRenderSession().film;
	const unsigned int pixelsCount = film->GetWidth() * film->GetHeight();

		// Only USER_IMPORTANCE can be updated
	auto destBuffer = renderSession.GetSLGRenderSession().film->GetChannel<float>(slg::Film::USER_IMPORTANCE,
				index, executeImagePipeline);
		copy(buffer, buffer + pixelsCount, destBuffer);

	API_END();
}

const float * FilmImplSession::GetChannelFloat(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	const float *result;
	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);

	result = renderSession.GetSLGRenderSession().film->GetChannel<float>(
		static_cast<slg::Film::FilmChannelType>(type),
		index, executeImagePipeline
	);

	API_RETURN("{}", (void *)result);

	return result;
}

const unsigned int * FilmImplSession::GetChannelUInt(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	const unsigned int *result;
	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);

	result = renderSession.GetSLGRenderSession().film->GetChannel<unsigned int>(static_cast<slg::Film::FilmChannelType>(type),
			index, executeImagePipeline);

	API_RETURN("{}", (void *)result);

	return result;
}

float * FilmImplSession::UpdateChannelFloat(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	if (type != CHANNEL_USER_IMPORTANCE)
		throw runtime_error(
			"Only USER_IMPORTANCE channel can be updated with Film::UpdateChannel<float>()"
		);

	float *result;

	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);

	result = renderSession.GetSLGRenderSession().film->GetChannel<float>(
		static_cast<slg::Film::FilmChannelType>(type),
		index, executeImagePipeline
	);

	API_RETURN("{}", (void *)result);

	return result;
}

void FilmImplSession::Parse(PropertiesRPtr props) {
	API_BEGIN("{}", ToArgString(props));

	throw runtime_error("Film::Parse() can be used only with a stand alone Film");

	API_END();
}

void FilmImplSession::DeleteAllImagePipelines()  {
	API_BEGIN_NOARGS();

	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);

	renderSession.GetSLGRenderSession().film->SetImagePipelines(nullptr);
	renderSession.GetSLGRenderSession().renderConfig.DeleteAllFilmImagePipelinesProperties();

	API_END();
}

void FilmImplSession::ExecuteImagePipeline(const u_int index) {
	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);

	renderSession.GetSLGRenderSession().film->ExecuteImagePipeline(index);
}

void FilmImplSession::AsyncExecuteImagePipeline(const u_int index) {
	API_BEGIN("{}", index);

	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);
	renderSession.GetSLGRenderSession().film->AsyncExecuteImagePipeline(index);

	API_END();
}

void FilmImplSession::WaitAsyncExecuteImagePipeline() {
	API_BEGIN_NOARGS();

	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);
	renderSession.GetSLGRenderSession().film->WaitAsyncExecuteImagePipeline();

	API_END();
}

bool FilmImplSession::HasDoneAsyncExecuteImagePipeline() {
	API_BEGIN_NOARGS();

	bool result;
	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);

	result = renderSession.GetSLGRenderSession().film->HasDoneAsyncExecuteImagePipeline();

	API_RETURN("{}", result);

	return result;
}

void FilmImplSession::ApplyOIDN(const u_int index) {
	API_BEGIN("{}", index);
	slg::IntelOIDN oidn("RT", 6000, 0.f, true);

	std::unique_lock<std::mutex> lock(renderSession.GetSLGRenderSession().filmMutex);
	oidn.Apply(*renderSession.GetSLGRenderSession().film, index);

	API_END();
}

void FilmImplSession::SaveOutputs() const {
	API_BEGIN_NOARGS();

	renderSession.GetSLGRenderSession().SaveFilmOutputs();

	API_END();
}

void FilmImplSession::SaveFilm(const string &fileName) const {
	API_BEGIN("{}", ToArgString(fileName));

	renderSession.GetSLGRenderSession().SaveFilm(fileName);

	API_END();
}

slg::FilmRef FilmImplSession::GetSLGFilm() const {
	return *renderSession.GetSLGRenderSession().film;
}

//------------------------------------------------------------------------------
// FilmImplStandalone
//------------------------------------------------------------------------------

FilmImplStandalone::FilmImplStandalone(const std::string &fileName) {
	standAloneFilm = slg::Film::LoadSerialized(fileName);
}

FilmImplStandalone::FilmImplStandalone(
	luxrays::PropertiesRPtr props,
	const bool hasPixelNormalizedChannel,
	const bool hasScreenNormalizedChannel
) {
	standAloneFilm = slg::Film::FromProperties(props);

	if (hasPixelNormalizedChannel)
		standAloneFilm->AddChannel(slg::Film::RADIANCE_PER_PIXEL_NORMALIZED);
	if (hasScreenNormalizedChannel)
		standAloneFilm->AddChannel(slg::Film::RADIANCE_PER_SCREEN_NORMALIZED);
	standAloneFilm->SetRadianceGroupCount(standAloneFilm->GetRadianceGroupCount());

	standAloneFilm->Init();
}

slg::FilmRef FilmImplStandalone::GetSLGFilm() const {

	return *standAloneFilm;
}

void FilmImplStandalone::SaveOutputs() const {
	API_BEGIN_NOARGS();

	throw runtime_error("Film::SaveOutputs() can not be used with a stand alone Film");

	API_END();
}

void FilmImplStandalone::SaveFilm(const string &fileName) const {
	API_BEGIN("{}", ToArgString(fileName));

	slg::Film::SaveSerialized(fileName, *standAloneFilm);

	API_END();
}

void FilmImplStandalone::GetOutputFloat(
	const FilmOutputType type, float *buffer,
	const unsigned int index, const bool executeImagePipeline
) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	standAloneFilm->GetOutput<float>(
		static_cast<slg::FilmOutputs::FilmOutputType>(type),
		buffer, index, executeImagePipeline
	);

	API_END();
}

void FilmImplStandalone::GetOutputUInt(const FilmOutputType type, unsigned int *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	standAloneFilm->GetOutput<unsigned int>(
		static_cast<slg::FilmOutputs::FilmOutputType>(type),
		buffer, index, executeImagePipeline
	);

	API_END();
}

void FilmImplStandalone::UpdateOutputFloat(const FilmOutputType type, const float *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	if (type != OUTPUT_USER_IMPORTANCE)
		throw runtime_error("Currently, only USER_IMPORTANCE channel can be updated with Film::UpdateOutput<float>()");

	const unsigned int pixelsCount = standAloneFilm->GetWidth() * standAloneFilm->GetHeight();

	// Only USER_IMPORTANCE can be updated
	auto destBuffer = standAloneFilm->GetChannel<float>(slg::Film::USER_IMPORTANCE,
				index, executeImagePipeline);
		copy(buffer, buffer + pixelsCount, destBuffer);

	API_END();
}

const float * FilmImplStandalone::GetChannelFloat(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	const float *result;
	result = standAloneFilm->GetChannel<float>(
		static_cast<slg::Film::FilmChannelType>(type),
		index, executeImagePipeline
	);

	API_RETURN("{}", (void *)result);

	return result;
}

const unsigned int *FilmImplStandalone::GetChannelUInt(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	const unsigned int *result;
	result = standAloneFilm->GetChannel<unsigned int>(
		static_cast<slg::Film::FilmChannelType>(type),
		index, executeImagePipeline
	);

	API_RETURN("{}", (void *)result);

	return result;
}

float *FilmImplStandalone::UpdateChannelFloat(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	if (type != CHANNEL_USER_IMPORTANCE)
		throw runtime_error("Only USER_IMPORTANCE channel can be updated with Film::UpdateChannel<float>()");

	float *result;
	result = standAloneFilm->GetChannel<float>(static_cast<slg::Film::FilmChannelType>(type),
				index, executeImagePipeline);

	API_RETURN("{}", (void *)result);

	return result;
}

void FilmImplStandalone::Parse(PropertiesRPtr props) {
	API_BEGIN("{}", ToArgString(props));

	standAloneFilm->Parse(props);

	API_END();
}

void FilmImplStandalone::DeleteAllImagePipelines()  {
	API_BEGIN_NOARGS();

	standAloneFilm->SetImagePipelines(nullptr);

	API_END();
}

void FilmImplStandalone::ExecuteImagePipeline(const u_int index) {
	standAloneFilm->ExecuteImagePipeline(index);
}

void FilmImplStandalone::AsyncExecuteImagePipeline(const u_int index) {
	API_BEGIN("{}", index);

	standAloneFilm->AsyncExecuteImagePipeline(index);

	API_END();
}

void FilmImplStandalone::WaitAsyncExecuteImagePipeline() {
	API_BEGIN_NOARGS();

	standAloneFilm->WaitAsyncExecuteImagePipeline();

	API_END();
}

bool FilmImplStandalone::HasDoneAsyncExecuteImagePipeline() {
	API_BEGIN_NOARGS();

	bool result;
	result = standAloneFilm->HasDoneAsyncExecuteImagePipeline();

	API_RETURN("{}", result);

	return result;
}

void FilmImplStandalone::ApplyOIDN(const u_int index) {
	API_BEGIN("{}", index);
	slg::IntelOIDN oidn("RT", 6000, 0.f, true);

	oidn.Apply(*standAloneFilm, index);

	API_END();
}

//------------------------------------------------------------------------------
// CameraImpl
//------------------------------------------------------------------------------

CameraImpl::CameraImpl(SceneImpl &scn) : scene(scn) {
}

CameraImpl::~CameraImpl() {
}

const CameraImpl::CameraType CameraImpl::GetType() const {
	API_BEGIN_NOARGS();

	const CameraImpl::CameraType type = static_cast<Camera::CameraType>(scene.GetSlgScene().GetCamera().GetType());

	API_RETURN("{}", type);

	return type;
}

void CameraImpl::Translate(const float x, const float y, const float z) {
	API_BEGIN("{}, {}, {}", x, y, z);

	scene.GetSlgScene().GetCamera().Translate(Vector(x, y, z));
	scene.GetSlgScene().GetEditActions().AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::TranslateLeft(const float t) {
	API_BEGIN("{}", t);

	scene.GetSlgScene().GetCamera().TranslateLeft(t);
	scene.GetSlgScene().GetEditActions().AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::TranslateRight(const float t) {
	API_BEGIN("{}", t);

	scene.GetSlgScene().GetCamera().TranslateRight(t);
	scene.GetSlgScene().GetEditActions().AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::TranslateForward(const float t) {
	API_BEGIN("{}", t);

	scene.GetSlgScene().GetCamera().TranslateForward(t);
	scene.GetSlgScene().GetEditActions().AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::TranslateBackward(const float t) {
	API_BEGIN("{}", t);

	scene.GetSlgScene().GetCamera().TranslateBackward(t);
	scene.GetSlgScene().GetEditActions().AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::Rotate(const float angle, const float x, const float y, const float z) {
	API_BEGIN("{}, {}, {}, {}", angle, x ,y ,z);

	scene.GetSlgScene().GetCamera().Rotate(angle, Vector(x, y, z));
	scene.GetSlgScene().GetEditActions().AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::RotateLeft(const float angle) {
	API_BEGIN("{}", angle);

	scene.GetSlgScene().GetCamera().RotateLeft(angle);
	scene.GetSlgScene().GetEditActions().AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::RotateRight(const float angle) {
	API_BEGIN("{}", angle);

	scene.GetSlgScene().GetCamera().RotateRight(angle);
	scene.GetSlgScene().GetEditActions().AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::RotateUp(const float angle) {
	API_BEGIN("{}", angle);

	scene.GetSlgScene().GetCamera().RotateUp(angle);
	scene.GetSlgScene().GetEditActions().AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::RotateDown(const float angle) {
	API_BEGIN("{}", angle);

	scene.GetSlgScene().GetCamera().RotateDown(angle);
	scene.GetSlgScene().GetEditActions().AddAction(slg::CAMERA_EDIT);

	API_END();
}

//------------------------------------------------------------------------------
// SceneImpl
//------------------------------------------------------------------------------

// Case #1: non-owning construction. The scene is owned externally, SceneImpl
// is just requested to keep a reference on it
SceneImpl::SceneImpl(Private p, slg::SceneRef scn) :
	camera(std::make_unique<CameraImpl>(*this)),
	scenePropertiesCache(std::make_unique<luxrays::Properties>()),
	internalScene(nullptr),
	sceneRef(scn)
{}

// Other cases: SceneImpl owns the scene.
SceneImpl::SceneImpl(Private p, luxrays::PropertiesRPtr resizePolicyProps) :
	camera(std::make_unique<CameraImpl>(*this)),
	scenePropertiesCache(std::make_unique<luxrays::Properties>()),
	internalScene(std::make_unique<slg::Scene>(resizePolicyProps)),
	sceneRef(*internalScene)
{}

SceneImpl::SceneImpl(
	Private p,
	luxrays::PropertiesRPtr props,
	luxrays::PropertiesRPtr resizePolicyProps
) :
	camera(std::make_unique<CameraImpl>(*this)),
	scenePropertiesCache(std::make_unique<luxrays::Properties>()),
	internalScene(std::make_unique<slg::Scene>(props, resizePolicyProps)),
	sceneRef(*internalScene)
{}

static slg::SceneUPtr LoadScene(
	const string fileName,
	luxrays::PropertiesRPtr resizePolicyProps
) {
	const string ext = luxrays::GetFileNameExt(fileName);
	if (ext == ".bsc") {
		// The file is in a binary format
		return slg::Scene::LoadSerialized(fileName);
	} else if (ext == ".scn") {
		// The file is in a text format
		return std::make_unique<slg::Scene>(
			std::make_unique<Properties>(fileName), resizePolicyProps
		);
	} else {
		throw runtime_error("Unknown scene file extension: " + fileName);
	}

}


SceneImpl::SceneImpl(
	Private p,
	const string fileName,
	luxrays::PropertiesRPtr resizePolicyProps
) :
	internalScene(LoadScene(fileName, resizePolicyProps)),
	sceneRef(*internalScene),
	camera(std::make_unique<CameraImpl>(*this)),
	scenePropertiesCache(std::make_unique<luxrays::Properties>())
{}



void SceneImpl::GetBBox(float min[3], float max[3]) const {
	API_BEGIN("{}, {}", (void *)min, (void *)max);

	const BBox &worldBBox = GetSlgScene().GetDataSet().GetBBox();

	min[0] = worldBBox.pMin.x;
	min[1] = worldBBox.pMin.y;
	min[2] = worldBBox.pMin.z;

	max[0] = worldBBox.pMax.x;
	max[1] = worldBBox.pMax.y;
	max[2] = worldBBox.pMax.z;
	
	API_RETURN("({}, {}, {}), ({}, {}, {})", min[0], min[1], min[2], max[0], max[1], max[2]);
}

const Camera &SceneImpl::GetCamera() const {
	API_BEGIN_NOARGS();
	API_RETURN("{}", (void *)camera.get());

	return *camera;
}

Camera &SceneImpl::GetCamera() {
	API_BEGIN_NOARGS();
	API_RETURN("{}", (void *)camera.get());

	return *camera;
}

bool SceneImpl::IsImageMapDefined(const std::string &imgMapName) const {
	API_BEGIN("{}", ToArgString(imgMapName));

	const bool result = GetSlgScene().IsImageMapDefined(imgMapName);

	API_RETURN("{}", result);

	return result;
}

void SceneImpl::SetDeleteMeshData(const bool v) {
	API_BEGIN("{}", v);

	GetSlgScene().GetExtMeshes().SetDeleteMeshData(v);

	API_END();
}

void SceneImpl::SetMeshAppliedTransformation(const std::string &meshName,
			const float appliedTransMat[16]) {
	API_BEGIN("{}, {}", ToArgString(meshName), ToArgString(appliedTransMat, 16));

	auto& mesh = GetSlgScene().GetExtMeshes().GetExtMesh(meshName);

	auto getExtTriMesh = [&]() -> ExtTriangleMesh& {
		try {
			auto& m = dynamic_cast<ExtTriangleMesh&>(mesh);
			return std::ref(m);
		}
		catch(std::bad_cast&) {
			throw runtime_error(
				"Applied transformation can be set only for normal meshes: "
				+ meshName
			);
		}
	};

	ExtTriangleMesh& extTriMesh = getExtTriMesh();

	// I have to transpose the matrix
	const Matrix4x4 mat(
		appliedTransMat[0], appliedTransMat[4], appliedTransMat[8], appliedTransMat[12],
		appliedTransMat[1], appliedTransMat[5], appliedTransMat[9], appliedTransMat[13],
		appliedTransMat[2], appliedTransMat[6], appliedTransMat[10], appliedTransMat[14],
		appliedTransMat[3], appliedTransMat[7], appliedTransMat[11], appliedTransMat[15]);
	const Transform trans(mat);

	extTriMesh.SetLocal2World(trans);

	API_END();
}

void SceneImpl::DefineMesh(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		float *p, unsigned int *vi, float *n,
		float *uvs, float *cols, float *alphas) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}, {}, {}", ToArgString(meshName),
			plyNbVerts, plyNbTris,
			(void *)p, (void *)vi, (void *)n,
			(void *)uvs, (void *)cols, (void *)alphas);

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	auto toSpan = [plyNbVerts]<typename T>(float * data) {
		if (!plyNbVerts || !data) return std::span<T>();
		return std::span<T>(reinterpret_cast<T*>(data), plyNbVerts);
	};

	GetSlgScene().DefineMesh(
		meshName, plyNbVerts, plyNbTris,
		(Point *)p,
		(Triangle *)vi,
		(Normal *)n,
		toSpan.template operator()<UV>(uvs),
		toSpan.template operator()<Spectrum>(cols),
		toSpan.template operator()<float>(alphas)
	);

	API_END();
}

void SceneImpl::DefineMeshExt(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		float *p, unsigned int *vi, float *n,
		array<float *, LC_MESH_MAX_DATA_COUNT> *uvs,
		array<float *, LC_MESH_MAX_DATA_COUNT> *cols,
		array<float *, LC_MESH_MAX_DATA_COUNT> *alphas) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}, {}, {}", ToArgString(meshName),
			plyNbVerts, plyNbTris,
			(void *)p, (void *)vi, (void *)n,
			(void *)uvs, (void *)cols, (void *)alphas);

	// A safety check
	static_assert(LC_MESH_MAX_DATA_COUNT == EXTMESH_MAX_DATA_COUNT,
			"LC_MESH_MAX_DATA_COUNT and EXTMESH_MAX_DATA_COUNT must have the same value");

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	auto initProp = [plyNbVerts]<typename T>(
		std::array<float *, LC_MESH_MAX_DATA_COUNT> * propPtr
	) -> std::optional<ExtMeshProp<T>> {
		ExtMeshProp<T> res;
		if (!propPtr) return std::nullopt;

		auto& prop = *propPtr;
		for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
			auto p = reinterpret_cast<T*>(prop[i]);
			if (p) {
				res.SetLayer(i, std::span<T>(p, plyNbVerts));
			} else {
				res.SetLayer(i, std::span<T>());
			}
		}
		return std::make_optional(res);
	};

	auto slgUVs = initProp.template operator()<UV>(uvs);
	auto slgCols = initProp.template operator()<Spectrum>(cols);
	auto slgAlphas = initProp.template operator()<float>(alphas);

	GetSlgScene().DefineMeshExt(meshName, plyNbVerts, plyNbTris, (Point *)p,
			(Triangle *)vi, (Normal *)n,
			slgUVs, slgCols, slgAlphas);

	API_END();
}

void SceneImpl::SetMeshVertexAOV(const string &meshName,
		const unsigned int index, float *data, size_t size) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(meshName), index, (void *)data, size);

	GetSlgScene().SetMeshVertexAOV(meshName, index, data, size);

	API_END();
}

void SceneImpl::SetMeshTriangleAOV(const string &meshName,
		const unsigned int index, float *data, size_t size) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(meshName), index, (void *)data, size);

	GetSlgScene().SetMeshTriangleAOV(meshName, index, data, size);

	API_END();
}

void SceneImpl::SaveMesh(const string &meshName, const string &fileName) {
	API_BEGIN("{}, {}", ToArgString(meshName), ToArgString(fileName));

	auto& mesh = GetSlgScene().GetExtMeshes().GetExtMesh(meshName);
	mesh.Save(fileName);

	API_END();
}

void SceneImpl::DefineStrands(const string &shapeName, const cyHairFile &strandsFile,
		const StrandsTessellationType tesselType,
		const unsigned int adaptiveMaxDepth, const float adaptiveError,
		const unsigned int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const bool useCameraPosition) {
	API_BEGIN("{}, cyHairFile, {}, {}, {}, {}, {}, {}, {}", ToArgString(shapeName),
			ToArgString(tesselType),
			adaptiveMaxDepth, adaptiveError,
			solidSideCount, solidCapBottom, solidCapTop,
			useCameraPosition);

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().DefineStrands(shapeName, strandsFile,
			(slg::StrendsShape::TessellationType)tesselType, adaptiveMaxDepth, adaptiveError,
			solidSideCount, solidCapBottom, solidCapTop,
			useCameraPosition);
	
	API_END();
}

bool SceneImpl::IsMeshDefined(const std::string &meshName) const {
	API_BEGIN("{}", ToArgString(meshName));

	const bool result = GetSlgScene().IsMeshDefined(meshName);

	API_RETURN("{}", result);

	return result;
}

bool SceneImpl::IsTextureDefined(const std::string &texName) const {
	API_BEGIN("{}", ToArgString(texName));

	const bool result = GetSlgScene().IsTextureDefined(texName);

	API_RETURN("{}", result);

	return result;
}

bool SceneImpl::IsMaterialDefined(const std::string &matName) const {
	API_BEGIN("{}", ToArgString(matName));

	const bool result = GetSlgScene().IsMaterialDefined(matName);

	API_RETURN("{}", result);

	return result;
}

const unsigned int SceneImpl::GetLightCount() const {
	API_BEGIN_NOARGS();

	const unsigned int result = GetSlgScene().GetLightSources().GetSize();

	API_RETURN("{}", result);

	return result;
}

const unsigned int  SceneImpl::GetObjectCount() const {
	API_BEGIN_NOARGS();

	const unsigned int result = GetSlgScene().GetObjects().GetSize();

	API_RETURN("{}", result);

	return result;
}

void SceneImpl::Parse(PropertiesRPtr props) {
	API_BEGIN("{}", ToArgString(props));

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().Parse(props);

	API_END();
}

void SceneImpl::DuplicateObject(const std::string &srcObjName, const std::string &dstObjName,
		const float transMat[16], const unsigned int objectID) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(srcObjName), ToArgString(dstObjName),
			ToArgString(transMat, 16), objectID);

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	// I have to transpose the matrix
	const Matrix4x4 mat(
		transMat[0], transMat[4], transMat[8], transMat[12],
		transMat[1], transMat[5], transMat[9], transMat[13],
		transMat[2], transMat[6], transMat[10], transMat[14],
		transMat[3], transMat[7], transMat[11], transMat[15]);
	const Transform trans(mat);
	GetSlgScene().DuplicateObject(srcObjName, dstObjName, trans, objectID);

	API_END();
}

void SceneImpl::DuplicateObject(const std::string &srcObjName, const std::string &dstObjNamePrefix,
			const unsigned int count, const float *transMats, const unsigned int *objectIDs) {
	API_BEGIN("{}, {}, {}, {}, {}", ToArgString(srcObjName), ToArgString(dstObjNamePrefix),
			count, (void *)transMats, (void *)objectIDs);

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	const float *transMat = transMats;
	for (u_int i = 0; i < count; ++i) {
		// I have to transpose the matrix
		const Matrix4x4 mat(
			transMat[0], transMat[4], transMat[8], transMat[12],
			transMat[1], transMat[5], transMat[9], transMat[13],
			transMat[2], transMat[6], transMat[10], transMat[14],
			transMat[3], transMat[7], transMat[11], transMat[15]);
		const Transform trans(mat);

		const unsigned int objectID = objectIDs ? objectIDs[i] : 0xffffffff;

		const string dstObjName = dstObjNamePrefix + ToString(i);
		GetSlgScene().DuplicateObject(srcObjName, dstObjName, trans, objectID);

		// Move to the next matrix
		transMat += 16;
	}

	API_END();
}

void SceneImpl::DuplicateObject(const std::string &srcObjName, const std::string &dstObjName,
		const u_int steps, const float *times, const float *transMats, const unsigned int objectID) {
	API_BEGIN("{}, {}, {}, {}, {}, {}", ToArgString(srcObjName), ToArgString(dstObjName),
			steps, (void *)times, (void *)transMats, objectID);

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	vector<float> tms(steps);
	vector<Transform> trans(steps);
	const float *time = times;
	const float *transMat = transMats;
	for (u_int i = 0; i < steps; ++i) {
		// Copy and move the pointer to the next time
		tms[i] = *time++;

		const Matrix4x4 mat(
			transMat[0], transMat[4], transMat[8], transMat[12],
			transMat[1], transMat[5], transMat[9], transMat[13],
			transMat[2], transMat[6], transMat[10], transMat[14],
			transMat[3], transMat[7], transMat[11], transMat[15]);
		// Move the pointer to the next matrix
		transMat += 16;

		// NOTE: Transform for MotionSystem are global2local and not local2global as usual
		trans[i] = Inverse(Transform(mat));
	}

	GetSlgScene().DuplicateObject(
		srcObjName, dstObjName, MotionSystem(tms, trans), objectID
	);

	API_END();
}

void SceneImpl::DuplicateObject(const std::string &srcObjName, const std::string &dstObjNamePrefix,
		const unsigned int count, const u_int steps, const float *times, const float *transMats,
		const unsigned int *objectIDs) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}", ToArgString(srcObjName), ToArgString(dstObjNamePrefix),
			count, steps, (void *)times, (void *)transMats, (void *)objectIDs);

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	vector<float> tms(steps);
	vector<Transform> trans(steps);
	const float *time = times;
	const float *transMat = transMats;
	for (u_int j = 0; j < count; ++j) {
		for (u_int i = 0; i < steps; ++i) {
			// Copy and move the pointer to the next time
			tms[i] = *time++;

			const Matrix4x4 mat(
				transMat[0], transMat[4], transMat[8], transMat[12],
				transMat[1], transMat[5], transMat[9], transMat[13],
				transMat[2], transMat[6], transMat[10], transMat[14],
				transMat[3], transMat[7], transMat[11], transMat[15]);
			// Move the pointer to the next matrix
			transMat += 16;

			// NOTE: Transform for MotionSystem are global2local and not local2global as usual
			trans[i] = Inverse(Transform(mat));
		}

		const unsigned int objectID = objectIDs ? objectIDs[j] : 0xffffffff;

		const string dstObjName = dstObjNamePrefix + ToString(j);
		GetSlgScene().DuplicateObject(srcObjName, dstObjName, MotionSystem(tms, trans), objectID);
	}
	
	API_END();
}

void SceneImpl::UpdateObjectTransformation(const std::string &objName, const float transMat[16]) {
	API_BEGIN("{}, {}", ToArgString(objName), ToArgString(transMat, 16));

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	// I have to transpose the matrix
	const Matrix4x4 mat(
		transMat[0], transMat[4], transMat[8], transMat[12],
		transMat[1], transMat[5], transMat[9], transMat[13],
		transMat[2], transMat[6], transMat[10], transMat[14],
		transMat[3], transMat[7], transMat[11], transMat[15]);
	const Transform trans(mat);
	GetSlgScene().UpdateObjectTransformation(objName, trans);

	API_END();
}

void SceneImpl::UpdateObjectMaterial(const std::string &objName, const std::string &matName) {
	API_BEGIN("{}, {}", ToArgString(objName), ToArgString(matName));

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().UpdateObjectMaterial(objName, matName);

	API_END();
}

void SceneImpl::DeleteObject(const string &objName) {
	API_BEGIN("{}", ToArgString(objName));

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().DeleteObject(objName);

	API_END();
}

void SceneImpl::DeleteObjects(std::vector<std::string> &objNames) {
	API_BEGIN("{}", ToArgString(objNames));

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().DeleteObjects(objNames);

	API_END();
}

void SceneImpl::DeleteLight(const string &lightName) {
	API_BEGIN("{}", ToArgString(lightName));

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().DeleteLight(lightName);

	API_END();
}

void SceneImpl::DeleteLights(std::vector<std::string> &lightNames) {
	API_BEGIN("{}", ToArgString(lightNames));

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().DeleteLights(lightNames);

	API_END();
}

void SceneImpl::RemoveUnusedImageMaps() {
	API_BEGIN_NOARGS();

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().RemoveUnusedImageMaps();

	API_END();
}

void SceneImpl::RemoveUnusedTextures() {
	API_BEGIN_NOARGS();

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().RemoveUnusedTextures();
}

void SceneImpl::RemoveUnusedMaterials() {
	API_BEGIN_NOARGS();

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().RemoveUnusedMaterials();

	API_END();
}

void SceneImpl::RemoveUnusedMeshes() {
	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().RemoveUnusedMeshes();

	API_END();
}

void SceneImpl::DefineImageMapUChar(const std::string &imgMapName,
		unsigned char *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		ChannelSelectionType selectionType, WrapType wrapType) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}, {}", ToArgString(imgMapName), (void *)pixels, gamma, channels,
			width, height, ToArgString(selectionType), ToArgString(wrapType));

	GetSlgScene().DefineImageMap(imgMapName, pixels, channels, width, height,
			slg::ImageMapConfig(
				gamma,
				slg::ImageMapStorage::StorageType::BYTE,
				(slg::ImageMapStorage::WrapType)wrapType,
				(slg::ImageMapStorage::ChannelSelectionType)selectionType));

	API_END();
}

void SceneImpl::DefineImageMapHalf(const std::string &imgMapName,
		unsigned short *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		ChannelSelectionType selectionType, WrapType wrapType) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}, {}",
            ToArgString(imgMapName),
            (void *)pixels,
            gamma,
            channels,
	    width,
            height,
            ToArgString(selectionType),
            ToArgString(wrapType)
        );

	GetSlgScene().DefineImageMap(imgMapName, (half *)pixels, channels, width, height,
			slg::ImageMapConfig(
				gamma,
				slg::ImageMapStorage::StorageType::HALF,
				(slg::ImageMapStorage::WrapType)wrapType,
				(slg::ImageMapStorage::ChannelSelectionType)selectionType));

	API_END();
}

void SceneImpl::DefineImageMapFloat(const std::string &imgMapName,
		float *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		ChannelSelectionType selectionType, WrapType wrapType) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}, {}",
            ToArgString(imgMapName),
            (void *)pixels,
            gamma,
            channels,
	    width,
            height,
            ToArgString(selectionType),
            ToArgString(wrapType)
        );

	GetSlgScene().DefineImageMap(imgMapName, pixels, channels, width, height,
			slg::ImageMapConfig(
				gamma,
				slg::ImageMapStorage::StorageType::FLOAT,
				(slg::ImageMapStorage::WrapType)wrapType,
				(slg::ImageMapStorage::ChannelSelectionType)selectionType));

	API_END();
}

// Note: this method is not part of LuxCore API and it is used only internally
void SceneImpl::DefineMesh(std::unique_ptr<ExtTriangleMesh>&& mesh) {
	API_BEGIN("{}", (void *)mesh.get());

	// Invalidate the scene properties cache
	scenePropertiesCache->Clear();

	GetSlgScene().DefineMesh(std::move(mesh));

	API_END();
}

PropertiesRPtr SceneImpl::ToProperties() const {
	API_BEGIN_NOARGS();

	if (!scenePropertiesCache->GetSize())
		*scenePropertiesCache << GetSlgScene().ToProperties(true);

	//API_RETURN("{}", ToArgString(scenePropertiesCache));
	API_END();

	return scenePropertiesCache;
}

void SceneImpl::Save(const std::string &fileName) {
	API_BEGIN("{}", ToArgString(fileName));

	slg::Scene::SaveSerialized(fileName, std::move(internalScene));

	API_END();
}

Point *SceneImpl::AllocVerticesBuffer(const unsigned int meshVertCount) {
	API_BEGIN("{}", meshVertCount);

auto result = TriangleMesh::AllocVerticesBuffer(meshVertCount);

	API_RETURN("{}", (void *)result);

	return result;
}

Triangle *SceneImpl::AllocTrianglesBuffer(const unsigned int meshTriCount) {
	API_BEGIN("{}", meshTriCount);

auto result = TriangleMesh::AllocTrianglesBuffer(meshTriCount);

	API_RETURN("{}", (void *)result);
	
	return result;
}

//------------------------------------------------------------------------------
// RenderConfigImpl
//------------------------------------------------------------------------------

// Case #1: Non owing constructor: RenderConfigImpl is provided a scene (as a ref)
RenderConfigImpl::RenderConfigImpl(
	Private p,
	PropertiesRPtr props,
	SceneImpl& scn
) :
	sceneRef(scn),
	renderConfig(slg::RenderConfig::Create(std::ref(props), std::ref(scn.GetSlgScene())))
{}

// Other cases: RenderConfigImpl is requested to build the scene from the properties
// In these cases, RenderConfigImpl owns the scene
RenderConfigImpl::RenderConfigImpl(
	Private p,
	PropertiesRPtr props
) :
	renderConfig(slg::RenderConfig::Create(std::ref(props))),  // Build an internal scene
	internalScene(SceneImpl::Create<slg::SceneRef>(renderConfig->GetScene())),
	sceneRef(*internalScene)
{}

RenderConfigImpl::RenderConfigImpl(Private p, const std::string fileName) :
	renderConfig(slg::RenderConfig::LoadSerialized(fileName)),
	internalScene(SceneImpl::Create<slg::SceneRef>(renderConfig->GetScene())),
	sceneRef(*internalScene)
{}

// TODO Move to head of file?
//static slg::RenderConfigUPtr LoadRenderConfig(sif) {

	//slg::RenderConfigUPtr renderConfig;

	//SerializationInputFile sif(fileName);

	//// Read the render configuration and the scene
	//sif.GetArchive() >> renderConfig;

	//return renderConfig;
//}

template <typename T>
T RenderConfigImpl::ReadFromSIF() const {
	if (not sif) throw std::runtime_error(
		"RenderConfig: trying to read from serialization but input is not available"
	);

	T t;
	sif->GetArchive() >> t;
	return t;
}

RenderConfigImpl::RenderConfigImpl(
		Private p,
		const std::string fileName,
		std::shared_ptr<RenderStateImpl>& startState,  // Out parameter
		std::unique_ptr<FilmImpl>& startFilm  // Out parameter
) :
	sif(fileName),
	renderConfig(ReadFromSIF<slg::RenderConfigUPtr>()),
	internalScene(SceneImpl::Create(std::ref(renderConfig->GetScene()))),
	sceneRef(*internalScene)
{
	// Read the render state
	std::shared_ptr<slg::RenderState> st;
	sif->GetArchive() >> st;
	startState = std::make_shared<RenderStateImpl>(st);

	// Load the film
	std::unique_ptr<slg::Film> sf;
	sif->GetArchive() >> sf;
	startFilm = FilmImpl::Create(std::move(sf));

	if (!sif->IsGood())
		throw runtime_error(
			"Error while loading serialized render session: " + fileName
		);
}

PropertiesRPtr RenderConfigImpl::GetProperties() const {
	API_BEGIN_NOARGS();

	auto& result = renderConfig->GetConfigPtr();

	//API_RETURN("{}", ToArgString(result));
	API_END();

	return renderConfig->GetConfigPtr();
}

// We return an instance rather than a ref, to avoid issues in multithreaded
// context
Property RenderConfigImpl::GetProperty(const std::string &name) const {
	API_BEGIN("{}", ToArgString(name));

	Property result = renderConfig->GetProperty(name);

	API_RETURN("{}", ToArgString(result));

	return result;
}

PropertiesRPtr RenderConfigImpl::ToProperties() const {
	API_BEGIN_NOARGS();

	PropertiesRPtr result = renderConfig->ToProperties();

	//API_RETURN("{}", ToArgString(result));
	API_END();

	return result;
}

const Scene& RenderConfigImpl::GetScene() const {
	API_BEGIN_NOARGS();

	Scene &result = sceneRef;

	API_RETURN("{}", (void *)&result);

	return std::cref(result);
}

Scene& RenderConfigImpl::GetScene() {
	API_BEGIN_NOARGS();

	Scene &result = sceneRef;

	API_RETURN("{}", (void *)&result);

	return std::ref(result);
}

bool RenderConfigImpl::HasCachedKernels() const {
	API_BEGIN_NOARGS();

	bool result = renderConfig->HasCachedKernels();

	API_RETURN("{}", result);

	return result;
}

void RenderConfigImpl::Parse(PropertiesRPtr props) {
	API_BEGIN("{}", ToArgString(props));

	renderConfig->Parse(*props);

	API_END();
}

void RenderConfigImpl::Delete(const string &prefix) {
	API_BEGIN("{}", ToArgString(prefix));

	renderConfig->Delete(prefix);

	API_END();
}

bool RenderConfigImpl::GetFilmSize(unsigned int *filmFullWidth, unsigned int *filmFullHeight,
		unsigned int *filmSubRegion) const {
	API_BEGIN("{}, {}, {}", (void *)filmFullWidth, (void *)filmFullHeight, (void *)filmSubRegion);

	const bool result = slg::Film::GetFilmSize(
		renderConfig->GetConfig(), filmFullWidth, filmFullHeight, filmSubRegion
	);

	API_RETURN("{}", result);

	return result;
}

void RenderConfigImpl::DeleteSceneOnExit() {
	API_BEGIN_NOARGS();
	// TODO Remove (useless, now)

	API_END();
}

void RenderConfigImpl::Save(const std::string &fileName) const {
	API_BEGIN("{}", ToArgString(fileName));

	slg::RenderConfig::SaveSerialized(fileName, renderConfig);

	API_END();
}

void RenderConfigImpl::Export(const std::string &dirName) const {
	API_BEGIN("{}", ToArgString(dirName));

	slg::FileSaverRenderEngine::ExportScene(*renderConfig, dirName,
			renderConfig->GetProperty("renderengine.type").Get<string>());

	API_END();
}

void RenderConfigImpl::ExportGLTF(const std::string &fileName) const {
	API_BEGIN("{}", ToArgString(fileName));

	slg::FileSaverRenderEngine::ExportSceneGLTF(*renderConfig, fileName);

	API_END();
}

PropertiesRPtr RenderConfigImpl::GetDefaultProperties() {
	API_BEGIN_NOARGS();

	auto& result = slg::RenderConfig::GetDefaultProperties();

	API_END();

	return result;
}

//------------------------------------------------------------------------------
// RenderStateImpl
//------------------------------------------------------------------------------

RenderStateImpl::RenderStateImpl(const std::string &fileName) {
	renderState = slg::RenderState::LoadSerialized(fileName);
}

RenderStateImpl::RenderStateImpl(std::shared_ptr<slg::RenderState> state) {
	renderState = state;
}

void RenderStateImpl::Save(const std::string &fileName) const {
	API_BEGIN("{}", ToArgString(fileName));

	renderState->SaveSerialized(fileName);

	API_END();
}

//------------------------------------------------------------------------------
// RenderSessionImpl
//------------------------------------------------------------------------------

RenderSessionImpl::RenderSessionImpl(
	Private priv,
	RenderConfigImplRef config
) :
	renderConfig(config),
	stats(std::make_unique<Properties>())
{
	renderSession = std::make_unique<slg::RenderSession>(
		*config.renderConfig,
		slg::RenderStateSPtr(nullptr),
		nullptr
	);
}

RenderSessionImpl::RenderSessionImpl(
	Private priv,
	RenderConfigImplRef config,
	std::shared_ptr<RenderStateImpl>& startState,
	FilmImplStandalone& startFilm
) :
	renderConfig(config),
	stats(std::make_unique<Properties>())
{
	// Create slg session
	renderSession = std::make_unique<slg::RenderSession>(
		*config.renderConfig,
		startState->renderState,
		slg::FilmPtr(std::addressof(startFilm.GetSLGFilm()))
	);

}

RenderSessionImpl::RenderSessionImpl(
	Private priv,
	RenderConfigImplRef config,
	const std::string &startStateFileName,
	const std::string &startFilmFileName
) :
	renderConfig(config),
	stats(std::make_unique<Properties>())
{

	auto startFilm = slg::Film::LoadSerialized(startFilmFileName);
	auto startState = slg::RenderState::LoadSerialized(startStateFileName);

	slg::RenderConfigRef rcfg(*config.renderConfig);

	renderSession = std::make_unique<slg::RenderSession>(
		rcfg,
		startState,
		slg::FilmPtr(startFilm.get())
	);
}

void RenderSessionImpl::InitFilm() {
	// Only for standalone case: we need to create the session film
	film = std::make_unique<FilmImplSession>(*this);
}

RenderConfigImplConstRef RenderSessionImpl::GetRenderConfig() const {
	API_BEGIN_NOARGS();

	API_RETURN("{}", (void *)&renderConfig);

	return std::cref(renderConfig);
}

std::shared_ptr<RenderState> RenderSessionImpl::GetRenderState() {
	API_BEGIN_NOARGS();

	// Create a new RenderStateImpl
	auto result = std::make_shared<RenderStateImpl>(renderSession->GetRenderState());

	API_RETURN("{}", (void *)result.get());

	return result;
}

void RenderSessionImpl::Start() {
	API_BEGIN_NOARGS();

	renderSession->Start();

	// In order to populate the stats.* Properties
	UpdateStats();

	API_END();
}

void RenderSessionImpl::Stop() {
	API_BEGIN_NOARGS();

	renderSession->Stop();

	API_END();
}

bool RenderSessionImpl::IsStarted() const {
	API_BEGIN_NOARGS();

	const bool result = renderSession->IsStarted();

	API_RETURN("{}", result);

	return result;
}

void RenderSessionImpl::BeginSceneEdit() {
	API_BEGIN_NOARGS();

	renderSession->BeginSceneEdit();

	API_END();
}

void RenderSessionImpl::EndSceneEdit() {
	API_BEGIN_NOARGS();

	renderSession->EndSceneEdit();

	// Invalidate the scene properties cache
	const auto& sceneimpl = dynamic_cast<const SceneImpl&>(renderConfig.GetScene());
	sceneimpl.scenePropertiesCache->Clear();

	API_END();
}

bool RenderSessionImpl::IsInSceneEdit() const {
	API_BEGIN_NOARGS();

	const bool result = renderSession->IsInSceneEdit();

	API_RETURN("{}", result);

	return result;
}

void RenderSessionImpl::Pause() {
	API_BEGIN_NOARGS();

	renderSession->Pause();

	API_END();
}

void RenderSessionImpl::Resume() {
	API_BEGIN_NOARGS();

	renderSession->Resume();

	API_END();
}

bool RenderSessionImpl::IsInPause() const {
	API_BEGIN_NOARGS();

	const bool result = renderSession->IsInPause();

	API_RETURN("{}", result);

	return result;
}

bool RenderSessionImpl::HasDone() const {
	API_BEGIN_NOARGS();

	const bool result = renderSession->renderEngine->HasDone();

	API_RETURN("{}", result);

	return result;
}

void RenderSessionImpl::WaitForDone() const {
	API_BEGIN_NOARGS();

	renderSession->renderEngine->WaitForDone();

	API_END();
}

void RenderSessionImpl::WaitNewFrame() {
	API_BEGIN_NOARGS();

	renderSession->renderEngine->WaitNewFrame();

	API_END();
}

LuxFilmRef RenderSessionImpl::GetFilm() {
	API_BEGIN_NOARGS();

	API_RETURN("{}", (void *)film.get());

	return *film;
}

FilmImplRPtr RenderSessionImpl::GetFilmPtr() {
	API_BEGIN_NOARGS();

	API_RETURN("{}", (void *)film.get());

	return film;
}

static void SetTileProperties(
	Properties &props,
	const string &prefix,
	const std::deque<const slg::Tile *> &tiles
) {
	props.Set(Property(prefix + ".count")((unsigned int)tiles.size()));
	Property tileCoordProp(prefix + ".coords");
	Property tilePassProp(prefix + ".pass");
	Property tilePendingPassesProp(prefix + ".pendingpasses");
	Property tileErrorProp(prefix + ".error");

	for(auto* tile: tiles) {
		tileCoordProp.Add(tile->coord.x).Add(tile->coord.y);
		tilePassProp.Add(tile->pass);
		tilePendingPassesProp.Add(tile->pendingPasses);
		tileErrorProp.Add(tile->error);
	}

	props.Set(tileCoordProp);
	props.Set(tilePassProp);
	props.Set(tilePendingPassesProp);
	props.Set(tileErrorProp);
}

void RenderSessionImpl::UpdateStats() {
	API_BEGIN_NOARGS();

	// It is not really correct to call UpdateStats() outside a Start()/Stop()
	// however it is easy to avoid any harm if it is done.
	if (!renderSession->IsStarted()) {
		API_END();

		return;
	}

	//--------------------------------------------------------------------------
	// Stats update
	//--------------------------------------------------------------------------

	// Film update may be required by some render engine to
	// update statistics, convergence test and more
	renderSession->renderEngine->UpdateFilm();

	stats->Set(Property("stats.renderengine.total.raysec")(renderSession->renderEngine->GetTotalRaysSec()));
	stats->Set(Property("stats.renderengine.total.samplesec")(renderSession->renderEngine->GetTotalSamplesSec()));
	stats->Set(Property("stats.renderengine.total.samplesec.eye")(renderSession->renderEngine->GetTotalEyeSamplesSec()));
	stats->Set(Property("stats.renderengine.total.samplesec.light")(renderSession->renderEngine->GetTotalLightSamplesSec()));
	stats->Set(Property("stats.renderengine.total.samplecount")(renderSession->renderEngine->GetTotalSampleCount()));
	stats->Set(Property("stats.renderengine.pass")(renderSession->renderEngine->GetPass()));
	stats->Set(Property("stats.renderengine.pass.eye")(renderSession->renderEngine->GetEyePass()));
	stats->Set(Property("stats.renderengine.pass.light")(renderSession->renderEngine->GetLightPass()));
	stats->Set(Property("stats.renderengine.time")(renderSession->renderEngine->GetRenderingTime()));
	stats->Set(Property("stats.renderengine.convergence")(renderSession->film->GetConvergence()));

	// Intersection devices statistics
	const vector<IntersectionDevice *> &idevices = renderSession->renderEngine->GetIntersectionDevices();

	std::unordered_map<string, unsigned int> devCounters;
	Property devicesNames("stats.renderengine.devices");
	double totalPerf = 0.0;
	for(IntersectionDevice *dev: idevices) {
		const string &devName = dev->GetName();

		// Append a device index for the case where the same device is used
		// multiple times
		unsigned int index = devCounters[devName]++;
		const string uniqueName = devName + "-" + ToString(index);
		devicesNames.Add(uniqueName);

		const string prefix = "stats.renderengine.devices." + uniqueName;

		stats->Set(Property(prefix + ".type")(DeviceDescription::GetDeviceType(dev->GetDeviceDesc()->GetType())));

		totalPerf += dev->GetTotalPerformance();
		stats->Set(Property(prefix + ".performance.total")(dev->GetTotalPerformance()));
		stats->Set(Property(prefix + ".performance.serial")(dev->GetSerialPerformance()));
		stats->Set(Property(prefix + ".performance.dataparallel")(dev->GetDataParallelPerformance()));

		auto hardDev = dynamic_cast<const HardwareDevice *>(dev);
		if (hardDev) {
			stats->Set(Property(prefix + ".memory.total")((u_longlong)hardDev->GetDeviceDesc()->GetMaxMemory()));
			stats->Set(Property(prefix + ".memory.used")((u_longlong)hardDev->GetUsedMemory()));
		} else {
			stats->Set(Property(prefix + ".memory.total")(0ull));
			stats->Set(Property(prefix + ".memory.used")(0ull));
		}
	}
	stats->Set(devicesNames);
	stats->Set(Property("stats.renderengine.performance.total")(totalPerf));

	// The explicit cast to size_t is required by VisualC++
	stats->Set(Property("stats.dataset.trianglecount")(
		renderSession->renderConfig.GetScene().GetDataSet().GetTotalTriangleCount())
	);

	// Some engine specific statistic
	switch (renderSession->renderEngine->GetType()) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case slg::RTPATHOCL: {
		auto engine = static_cast<slg::RTPathOCLRenderEngine*>(renderSession->renderEngine.get());
			stats->Set(Property("stats.rtpathocl.frame.time")(engine->GetFrameTime()));
			break;
		}
		case slg::TILEPATHOCL: {
		auto engine = static_cast<slg::TilePathOCLRenderEngine*>(renderSession->renderEngine.get());

			stats->Set(Property("stats.tilepath.tiles.size.x")(engine->GetTileWidth()));
			stats->Set(Property("stats.tilepath.tiles.size.y")(engine->GetTileHeight()));

			// Pending tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetPendingTiles(tiles);
				SetTileProperties(*stats, "stats.tilepath.tiles.pending", tiles);
			}

			// Not converged tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetNotConvergedTiles(tiles);
				SetTileProperties(*stats, "stats.tilepath.tiles.notconverged", tiles);
			}

			// Converged tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetConvergedTiles(tiles);
				SetTileProperties(*stats, "stats.tilepath.tiles.converged", tiles);
			}
			break;
		}
#endif
		case slg::TILEPATHCPU: {
			auto engine = static_cast<slg::CPUTileRenderEngine*>(renderSession->renderEngine.get());

			stats->Set(Property("stats.tilepath.tiles.size.x")(engine->GetTileWidth()));
			stats->Set(Property("stats.tilepath.tiles.size.y")(engine->GetTileHeight()));

			// Pending tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetPendingTiles(tiles);
				SetTileProperties(*stats, "stats.tilepath.tiles.pending", tiles);
			}

			// Not converged tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetNotConvergedTiles(tiles);
				SetTileProperties(*stats, "stats.tilepath.tiles.notconverged", tiles);
			}

			// Converged tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetConvergedTiles(tiles);
				SetTileProperties(*stats, "stats.tilepath.tiles.converged", tiles);
			}
			break;
		}
		default:
			break;
	}

	//--------------------------------------------------------------------------
	// Periodic save
	//--------------------------------------------------------------------------

	renderSession->CheckPeriodicSave();

	API_END();
}

const PropertiesUPtr & RenderSessionImpl::GetStats() const {
	API_BEGIN_NOARGS();

	const PropertiesUPtr &result = stats;

	//API_RETURN("{}", ToArgString(result));
	API_END();

	return result;
}

void RenderSessionImpl::Parse(luxrays::PropertiesRPtr props) {
	API_BEGIN("{}", ToArgString(props));

	renderSession->Parse(props);

	API_END();
}
void RenderSessionImpl::SaveResumeFile(const std::string &fileName) {
	API_BEGIN("{}", ToArgString(fileName));

	renderSession->SaveResumeFile(fileName);

	API_END();
}


auto std::formatter<luxcore::Camera::CameraType>::format(
    luxcore::Camera::CameraType cam,
    std::format_context& ctx
) const -> format_context::iterator {

  string_view name = "UNKNOWN";
  switch (cam) {
    case luxcore::Camera::CameraType::PERSPECTIVE: name = "PERSPECTIVE"; break;
    case luxcore::Camera::CameraType::ORTHOGRAPHIC: name = "ORTHOGRAPHIC"; break;
    case luxcore::Camera::CameraType::STEREO: name = "STEREO"; break;
    case luxcore::Camera::CameraType::ENVIRONMENT: name = "ENVIRONMENT"; break;
  }
    return formatter<string_view>::format(name, ctx);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
