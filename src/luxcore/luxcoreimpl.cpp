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

#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/thread/once.hpp>
#include <boost/thread/mutex.hpp>

#include "luxcore/luxcorelogger.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/fileext.h"
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
#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;
using namespace luxcore::detail;

//------------------------------------------------------------------------------
// FilmImpl
//------------------------------------------------------------------------------

FilmImpl::FilmImpl(const std::string &fileName) : renderSession(NULL) {
	standAloneFilm = slg::Film::LoadSerialized(fileName);
}

FilmImpl::FilmImpl(const luxrays::Properties &props, const bool hasPixelNormalizedChannel,
		const bool hasScreenNormalizedChannel) : renderSession(NULL) {
	standAloneFilm = slg::Film::FromProperties(props);

	if (hasPixelNormalizedChannel)
		standAloneFilm->AddChannel(slg::Film::RADIANCE_PER_PIXEL_NORMALIZED);
	if (hasScreenNormalizedChannel)
		standAloneFilm->AddChannel(slg::Film::RADIANCE_PER_SCREEN_NORMALIZED);
	standAloneFilm->SetRadianceGroupCount(standAloneFilm->GetRadianceGroupCount());

	standAloneFilm->Init();
}

FilmImpl::FilmImpl(const RenderSessionImpl &session) : renderSession(&session),
		standAloneFilm(NULL) {
}

FilmImpl::FilmImpl(slg::Film *film) : renderSession(NULL) {
	standAloneFilm = film;
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
	API_BEGIN_NOARGS();

	const unsigned int result = GetSLGFilm()->GetWidth();

	API_RETURN("{}", result);

	return result;
}

unsigned int FilmImpl::GetHeight() const {
	API_BEGIN_NOARGS();

	const unsigned int result = GetSLGFilm()->GetHeight();

	API_RETURN("{}", result);

	return result;
}

luxrays::Properties FilmImpl::GetStats() const {
	API_BEGIN_NOARGS();

	slg::Film *film = GetSLGFilm();

	Properties stats;
	stats.Set(Property("stats.film.total.samplecount")(film->GetTotalSampleCount()));
	stats.Set(Property("stats.film.spp")(film->GetTotalSampleCount() / static_cast<float>(film->GetWidth() * film->GetHeight())));
	stats.Set(Property("stats.film.radiancegorup.count")(film->GetRadianceGroupCount()));

	API_RETURN("{}", ToArgString(stats));

	return stats;
}

float FilmImpl::GetFilmY(const u_int imagePipelineIndex) const {
	API_BEGIN_NOARGS();

	const float result = GetSLGFilm()->GetFilmY(imagePipelineIndex);
	
	API_RETURN("{}", result);

	return result;
}

void FilmImpl::Clear() {
	API_BEGIN_NOARGS();

	GetSLGFilm()->Clear();

	API_END();
}

void FilmImpl::AddFilm(const Film &film) {
	const FilmImpl *filmImpl = dynamic_cast<const FilmImpl *>(&film);
	assert (filmImpl);
	
	API_BEGIN("{}", (void *)filmImpl);

	AddFilm(film, 0, 0, filmImpl->GetWidth(), filmImpl->GetHeight(), 0, 0);

	API_END();
}

void FilmImpl::AddFilm(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY) {
	const FilmImpl *srcFilmImpl = dynamic_cast<const FilmImpl *>(&film);
	assert (srcFilmImpl);
	
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}", (void *)srcFilmImpl, srcOffsetX, srcOffsetY, srcWidth, srcHeight, dstOffsetX, dstOffsetY);
	
	const FilmImpl *dstFilmImpl = this;

	// I have to clip the parameters to avoid an out of bound memory access

	// Check the cases where I have nothing to do
	if (srcOffsetX >= srcFilmImpl->GetWidth())
		return;
	if (srcOffsetY >= srcFilmImpl->GetHeight())
		return;
	if (dstOffsetX >= dstFilmImpl->GetWidth())
		return;
	if (dstOffsetY >= dstFilmImpl->GetHeight())
		return;

	u_int clippedSrcWidth;
	// Clip with the src film
	clippedSrcWidth = Min(srcOffsetX + srcWidth, srcFilmImpl->GetWidth()) - srcOffsetX;
	// Clip with the dst film
	clippedSrcWidth = Min(dstOffsetX + clippedSrcWidth, dstFilmImpl->GetWidth()) - dstOffsetX;

	u_int clippedSrcHeight;
	// Clip with the src film
	clippedSrcHeight = Min(srcOffsetY + srcHeight, srcFilmImpl->GetHeight()) - srcOffsetY;
	// Clip with the dst film
	clippedSrcHeight = Min(dstOffsetY + clippedSrcHeight, dstFilmImpl->GetHeight()) - dstOffsetY;

	GetSLGFilm()->AddFilm(*(srcFilmImpl->GetSLGFilm()),srcOffsetX, srcOffsetY,
			clippedSrcWidth, clippedSrcHeight, dstOffsetX, dstOffsetY);

	API_END();
}

void FilmImpl::SaveOutputs() const {
	API_BEGIN_NOARGS();

	if (renderSession)
		renderSession->renderSession->SaveFilmOutputs();
	else
		throw runtime_error("Film::SaveOutputs() can not be used with a stand alone Film");

	API_END();
}

void FilmImpl::SaveOutput(const std::string &fileName, const FilmOutputType type, const Properties &props) const {
	API_BEGIN("{}, {}, {}", ToArgString(fileName),ToArgString(type), ToArgString(props));

	GetSLGFilm()->Output(fileName, (slg::FilmOutputs::FilmOutputType)type, &props);
	
	API_END();
}

void FilmImpl::SaveFilm(const string &fileName) const {
	API_BEGIN("{}", ToArgString(fileName));

	if (renderSession)
		renderSession->renderSession->SaveFilm(fileName);
	else
		slg::Film::SaveSerialized(fileName, standAloneFilm);

	API_END();
}

double FilmImpl::GetTotalSampleCount() const {
	API_BEGIN_NOARGS();

	const double result = GetSLGFilm()->GetTotalSampleCount();

	API_RETURN("{}", result);
	
	return result;
}

bool FilmImpl::HasOutput(const FilmOutputType type) const {
	API_BEGIN("{}", ToArgString(type));

	const bool result = GetSLGFilm()->HasOutput((slg::FilmOutputs::FilmOutputType)type);
	
	API_RETURN("{}", result);
	
	return result;
}

unsigned int FilmImpl::GetOutputCount(const FilmOutputType type) const {
	API_BEGIN("{}", ToArgString(type));

	const unsigned int result = GetSLGFilm()->GetOutputCount((slg::FilmOutputs::FilmOutputType)type);

	API_RETURN("{}", result);
	
	return result;
}

size_t FilmImpl::GetOutputSize(const FilmOutputType type) const {
	API_BEGIN("{}", ToArgString(type));

	const size_t result = GetSLGFilm()->GetOutputSize((slg::FilmOutputs::FilmOutputType)type);
	
	API_RETURN("{}", result);
	
	return result;
}

unsigned int FilmImpl::GetRadianceGroupCount() const {
	API_BEGIN_NOARGS();

	const unsigned int result = GetSLGFilm()->GetRadianceGroupCount();

	API_RETURN("{}", result);
	
	return result;
}

void FilmImpl::GetOutputFloat(const FilmOutputType type, float *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->GetOutput<float>((slg::FilmOutputs::FilmOutputType)type,
				buffer, index, executeImagePipeline);
	} else
		standAloneFilm->GetOutput<float>((slg::FilmOutputs::FilmOutputType)type,
				buffer, index, executeImagePipeline);

	API_END();
}

void FilmImpl::GetOutputUInt(const FilmOutputType type, unsigned int *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->GetOutput<u_int>((slg::FilmOutputs::FilmOutputType)type,
				buffer, index, executeImagePipeline);
	} else
		standAloneFilm->GetOutput<unsigned int>((slg::FilmOutputs::FilmOutputType)type,
				buffer, index, executeImagePipeline);

	API_END();
}

void FilmImpl::UpdateOutputFloat(const FilmOutputType type, const float *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	if (type != OUTPUT_USER_IMPORTANCE)
		throw runtime_error("Currently, only USER_IMPORTANCE channel can be updated with Film::UpdateOutput<float>()");

	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		slg::Film *film = renderSession->renderSession->film;
		const unsigned int pixelsCount = film->GetWidth() * film->GetHeight();

		// Only USER_IMPORTANCE can be updated
		float *destBuffer = renderSession->renderSession->film->GetChannel<float>(slg::Film::USER_IMPORTANCE,
				index, executeImagePipeline);
		copy(buffer, buffer + pixelsCount, destBuffer);
	} else {
		const unsigned int pixelsCount = standAloneFilm->GetWidth() * standAloneFilm->GetHeight();

		// Only USER_IMPORTANCE can be updated
		float *destBuffer = standAloneFilm->GetChannel<float>(slg::Film::USER_IMPORTANCE,
				index, executeImagePipeline);
		copy(buffer, buffer + pixelsCount, destBuffer);
	}

	API_END();
}

void FilmImpl::UpdateOutputUInt(const FilmOutputType type, const unsigned int *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(type), (void *)buffer, index, executeImagePipeline);

	throw runtime_error("No channel can be updated with Film::UpdateOutput<unsigned int>()");

	API_END();
}

bool FilmImpl::HasChannel(const FilmChannelType type) const {
	API_BEGIN("{}", ToArgString(type));

	const bool result = GetSLGFilm()->HasChannel((slg::Film::FilmChannelType)type);

	API_RETURN("{}", result);
	
	return result;
}

unsigned int FilmImpl::GetChannelCount(const FilmChannelType type) const {
	API_BEGIN("{}", ToArgString(type));

	const unsigned int result = GetSLGFilm()->GetChannelCount((slg::Film::FilmChannelType)type);

	API_RETURN("{}", result);
	
	return result;
}

const float *FilmImpl::GetChannelFloat(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	const float *result;
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		result = renderSession->renderSession->film->GetChannel<float>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);
	} else
		result = standAloneFilm->GetChannel<float>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);

	API_RETURN("{}", (void *)result);
	
	return result;
}

const unsigned int *FilmImpl::GetChannelUInt(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	const unsigned int *result;
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		result = renderSession->renderSession->film->GetChannel<unsigned int>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);
	} else
		result = standAloneFilm->GetChannel<unsigned int>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);

	API_RETURN("{}", (void *)result);
	
	return result;
}

float *FilmImpl::UpdateChannelFloat(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	if (type != CHANNEL_USER_IMPORTANCE)
		throw runtime_error("Only USER_IMPORTANCE channel can be updated with Film::UpdateChannel<float>()");

	float *result;
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		result = renderSession->renderSession->film->GetChannel<float>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);
	} else
		result = standAloneFilm->GetChannel<float>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);

	API_RETURN("{}", (void *)result);
	
	return result;
}

unsigned int *FilmImpl::UpdateChannelUInt(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	API_BEGIN("{}, {}, {}", ToArgString(type), index, executeImagePipeline);

	throw runtime_error("No channel can be updated with Film::UpdateChannel<unsigned int>()");

	API_END();
}

void FilmImpl::Parse(const luxrays::Properties &props) {
	API_BEGIN("{}", ToArgString(props));

	if (renderSession)
		throw runtime_error("Film::Parse() can be used only with a stand alone Film");
	else
		standAloneFilm->Parse(props);

	API_END();
}

void FilmImpl::DeleteAllImagePipelines()  {
	API_BEGIN_NOARGS();

	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->SetImagePipelines(NULL);
		renderSession->renderSession->renderConfig->DeleteAllFilmImagePipelinesProperties();
	} else
		standAloneFilm->SetImagePipelines(NULL);
	
	API_END();
}

void FilmImpl::ExecuteImagePipeline(const u_int index) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->ExecuteImagePipeline(index);
	} else
		standAloneFilm->ExecuteImagePipeline(index);
}

void FilmImpl::AsyncExecuteImagePipeline(const u_int index) {
	API_BEGIN("{}", index);

	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->AsyncExecuteImagePipeline(index);
	} else
		standAloneFilm->AsyncExecuteImagePipeline(index);
	
	API_END();
}

void FilmImpl::WaitAsyncExecuteImagePipeline() {
	API_BEGIN_NOARGS();

	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->WaitAsyncExecuteImagePipeline();
	} else
		standAloneFilm->WaitAsyncExecuteImagePipeline();

	API_END();
}

bool FilmImpl::HasDoneAsyncExecuteImagePipeline() {
	API_BEGIN_NOARGS();

	bool result;
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		result = renderSession->renderSession->film->HasDoneAsyncExecuteImagePipeline();
	} else
		result = standAloneFilm->HasDoneAsyncExecuteImagePipeline();
	
	API_RETURN("{}", result);
	
	return result;
}

//------------------------------------------------------------------------------
// CameraImpl
//------------------------------------------------------------------------------

CameraImpl::CameraImpl(const SceneImpl &scn) : scene(scn) {
}

CameraImpl::~CameraImpl() {
}

const CameraImpl::CameraType CameraImpl::GetType() const {
	API_BEGIN_NOARGS();

	const CameraImpl::CameraType type = (Camera::CameraType)scene.scene->camera->GetType();
	
	API_RETURN("{}", type);

	return type;
}

void CameraImpl::Translate(const float x, const float y, const float z) const {
	API_BEGIN("{}, {}, {}", x, y, z);

	scene.scene->camera->Translate(Vector(x, y, z));
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::TranslateLeft(const float t) const {
	API_BEGIN("{}", t);

	scene.scene->camera->TranslateLeft(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::TranslateRight(const float t) const {
	API_BEGIN("{}", t);

	scene.scene->camera->TranslateRight(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::TranslateForward(const float t) const {
	API_BEGIN("{}", t);

	scene.scene->camera->TranslateForward(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::TranslateBackward(const float t) const {
	API_BEGIN("{}", t);

	scene.scene->camera->TranslateBackward(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::Rotate(const float angle, const float x, const float y, const float z) const {
	API_BEGIN("{}, {}, {}, {}", angle, x ,y ,z);

	scene.scene->camera->Rotate(angle, Vector(x, y, z));
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::RotateLeft(const float angle) const {
	API_BEGIN("{}", angle);

	scene.scene->camera->RotateLeft(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::RotateRight(const float angle) const {
	API_BEGIN("{}", angle);

	scene.scene->camera->RotateRight(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::RotateUp(const float angle) const {
	API_BEGIN("{}", angle);

	scene.scene->camera->RotateUp(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);

	API_END();
}

void CameraImpl::RotateDown(const float angle) const {
	API_BEGIN("{}", angle);

	scene.scene->camera->RotateDown(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);

	API_END();
}

//------------------------------------------------------------------------------
// SceneImpl
//------------------------------------------------------------------------------

SceneImpl::SceneImpl(const float imageScale) {
	camera = new CameraImpl(*this);
	scene = new slg::Scene(imageScale);
	allocatedScene = true;
}

SceneImpl::SceneImpl(const luxrays::Properties &props, const float imageScale) {
	camera = new CameraImpl(*this);
	scene = new slg::Scene(props, imageScale);
	allocatedScene = true;
}

SceneImpl::SceneImpl(const string &fileName, const float imageScale) {
	camera = new CameraImpl(*this);

	const string ext = luxrays::GetFileNameExt(fileName);
	if (ext == ".bsc") {
		// The file is in a binary format
		scene = slg::Scene::LoadSerialized(fileName);
	} else if (ext == ".scn") {
		// The file is in a text format
		scene = new slg::Scene(Properties(fileName), imageScale);
	} else
		throw runtime_error("Unknown scene file extension: " + fileName);

	allocatedScene = true;
}

SceneImpl::SceneImpl(slg::Scene *scn) {
	camera = new CameraImpl(*this);
	scene = scn;
	allocatedScene = false;
}

SceneImpl::~SceneImpl() {
	if (allocatedScene)
		delete scene;
	delete camera;
}

void SceneImpl::GetBBox(float min[3], float max[3]) const {
	API_BEGIN("{}, {}", (void *)min, (void *)max);

	const BBox &worldBBox = scene->dataSet->GetBBox();

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
	API_RETURN("{}", (void *)camera);

	return *camera;
}

bool SceneImpl::IsImageMapDefined(const std::string &imgMapName) const {
	API_BEGIN("{}", ToArgString(imgMapName));

	const bool result = scene->IsImageMapDefined(imgMapName);
	
	API_RETURN("{}", result);

	return result;
}

void SceneImpl::SetDeleteMeshData(const bool v) {
	API_BEGIN("{}", v);

	scene->extMeshCache.SetDeleteMeshData(v);

	API_END();
}

void SceneImpl::SetMeshAppliedTransformation(const std::string &meshName,
			const float appliedTransMat[16]) {
	API_BEGIN("{}, {}", ToArgString(meshName), ToArgString(appliedTransMat, 16));

	ExtMesh *mesh = scene->extMeshCache.GetExtMesh(meshName);
	ExtTriangleMesh *extTriMesh = dynamic_cast<ExtTriangleMesh *>(mesh);
	if (!extTriMesh)
		throw runtime_error("Applied transformation can be set only for normal meshes: " + meshName);

	// I have to transpose the matrix
	const Matrix4x4 mat(
		appliedTransMat[0], appliedTransMat[4], appliedTransMat[8], appliedTransMat[12],
		appliedTransMat[1], appliedTransMat[5], appliedTransMat[9], appliedTransMat[13],
		appliedTransMat[2], appliedTransMat[6], appliedTransMat[10], appliedTransMat[14],
		appliedTransMat[3], appliedTransMat[7], appliedTransMat[11], appliedTransMat[15]);
	const Transform trans(mat);

	extTriMesh->SetLocal2World(trans);

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
	scenePropertiesCache.Clear();

	scene->DefineMesh(meshName, plyNbVerts, plyNbTris, (Point *)p,
			(Triangle *)vi, (Normal *)n,
			(UV *)uvs, (Spectrum *)cols, alphas);

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
	scenePropertiesCache.Clear();

	array<UV *, EXTMESH_MAX_DATA_COUNT> slgUVs;
	if (uvs) {
		for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i)
			slgUVs[i] = (UV *)((*uvs)[i]);
	} else
		fill(slgUVs.begin(), slgUVs.end(), nullptr);

	array<Spectrum *, EXTMESH_MAX_DATA_COUNT> slgCols;
	if (cols) {
		for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i)
			slgCols[i] = (Spectrum *)((*cols)[i]);
	} else
		fill(slgCols.begin(), slgCols.end(), nullptr);

	array<float *, EXTMESH_MAX_DATA_COUNT> slgAlphas;
	if (alphas) {
		for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i)
			slgAlphas[i] = (*alphas)[i];
	} else
		fill(slgAlphas.begin(), slgAlphas.end(), nullptr);

	scene->DefineMeshExt(meshName, plyNbVerts, plyNbTris, (Point *)p,
			(Triangle *)vi, (Normal *)n,
			&slgUVs, &slgCols, &slgAlphas);

	API_END();
}

void SceneImpl::SetMeshVertexAOV(const string &meshName,
		const unsigned int index, float *data) {
	API_BEGIN("{}, {}, {}", ToArgString(meshName), index, (void *)data);

	scene->SetMeshVertexAOV(meshName, index, data);

	API_END();
}

void SceneImpl::SetMeshTriangleAOV(const string &meshName,
		const unsigned int index, float *data) {
	API_BEGIN("{}, {}, {}", ToArgString(meshName), index, (void *)data);

	scene->SetMeshTriangleAOV(meshName, index, data);

	API_END();
}

void SceneImpl::SaveMesh(const string &meshName, const string &fileName) {
	API_BEGIN("{}, {}", ToArgString(meshName), ToArgString(fileName));

	const ExtMesh *mesh = scene->extMeshCache.GetExtMesh(meshName);
	mesh->Save(fileName);

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
	scenePropertiesCache.Clear();

	scene->DefineStrands(shapeName, strandsFile,
			(slg::StrendsShape::TessellationType)tesselType, adaptiveMaxDepth, adaptiveError,
			solidSideCount, solidCapBottom, solidCapTop,
			useCameraPosition);
	
	API_END();
}

bool SceneImpl::IsMeshDefined(const std::string &meshName) const {
	API_BEGIN("{}", ToArgString(meshName));

	const bool result = scene->IsMeshDefined(meshName);

	API_RETURN("{}", result);

	return result;
}

bool SceneImpl::IsTextureDefined(const std::string &texName) const {
	API_BEGIN("{}", ToArgString(texName));

	const bool result = scene->IsTextureDefined(texName);

	API_RETURN("{}", result);

	return result;
}

bool SceneImpl::IsMaterialDefined(const std::string &matName) const {
	API_BEGIN("{}", ToArgString(matName));

	const bool result = scene->IsMaterialDefined(matName);

	API_RETURN("{}", result);

	return result;
}

const unsigned int SceneImpl::GetLightCount() const {
	API_BEGIN_NOARGS();

	const unsigned int result = scene->lightDefs.GetSize();

	API_RETURN("{}", result);

	return result;
}

const unsigned int  SceneImpl::GetObjectCount() const {
	API_BEGIN_NOARGS();

	const unsigned int result = scene->objDefs.GetSize();

	API_RETURN("{}", result);

	return result;
}

void SceneImpl::Parse(const Properties &props) {
	API_BEGIN("{}", ToArgString(props));

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->Parse(props);

	API_END();
}

void SceneImpl::DuplicateObject(const std::string &srcObjName, const std::string &dstObjName,
		const float transMat[16], const unsigned int objectID) {
	API_BEGIN("{}, {}, {}, {}", ToArgString(srcObjName), ToArgString(dstObjName),
			ToArgString(transMat, 16), objectID);

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	// I have to transpose the matrix
	const Matrix4x4 mat(
		transMat[0], transMat[4], transMat[8], transMat[12],
		transMat[1], transMat[5], transMat[9], transMat[13],
		transMat[2], transMat[6], transMat[10], transMat[14],
		transMat[3], transMat[7], transMat[11], transMat[15]);
	const Transform trans(mat);
	scene->DuplicateObject(srcObjName, dstObjName, trans, objectID);

	API_END();
}

void SceneImpl::DuplicateObject(const std::string &srcObjName, const std::string &dstObjNamePrefix,
			const unsigned int count, const float *transMats, const unsigned int *objectIDs) {
	API_BEGIN("{}, {}, {}, {}, {}", ToArgString(srcObjName), ToArgString(dstObjNamePrefix),
			count, (void *)transMats, (void *)objectIDs);

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

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
		scene->DuplicateObject(srcObjName, dstObjName, trans, objectID);

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
	scenePropertiesCache.Clear();

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

	scene->DuplicateObject(srcObjName, dstObjName, MotionSystem(tms, trans), objectID);

	API_END();
}

void SceneImpl::DuplicateObject(const std::string &srcObjName, const std::string &dstObjNamePrefix,
		const unsigned int count, const u_int steps, const float *times, const float *transMats,
		const unsigned int *objectIDs) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}", ToArgString(srcObjName), ToArgString(dstObjNamePrefix),
			count, steps, (void *)times, (void *)transMats, (void *)objectIDs);

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

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
		scene->DuplicateObject(srcObjName, dstObjName, MotionSystem(tms, trans), objectID);
	}
	
	API_END();
}

void SceneImpl::UpdateObjectTransformation(const std::string &objName, const float transMat[16]) {
	API_BEGIN("{}, {}", ToArgString(objName), ToArgString(transMat, 16));

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	// I have to transpose the matrix
	const Matrix4x4 mat(
		transMat[0], transMat[4], transMat[8], transMat[12],
		transMat[1], transMat[5], transMat[9], transMat[13],
		transMat[2], transMat[6], transMat[10], transMat[14],
		transMat[3], transMat[7], transMat[11], transMat[15]);
	const Transform trans(mat);
	scene->UpdateObjectTransformation(objName, trans);

	API_END();
}

void SceneImpl::UpdateObjectMaterial(const std::string &objName, const std::string &matName) {
	API_BEGIN("{}, {}", ToArgString(objName), ToArgString(matName));

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->UpdateObjectMaterial(objName, matName);

	API_END();
}

void SceneImpl::DeleteObject(const string &objName) {
	API_BEGIN("{}", ToArgString(objName));

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DeleteObject(objName);

	API_END();
}

void SceneImpl::DeleteLight(const string &lightName) {
	API_BEGIN("{}", ToArgString(lightName));

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DeleteLight(lightName);

	API_END();
}

void SceneImpl::RemoveUnusedImageMaps() {
	API_BEGIN_NOARGS();

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedImageMaps();

	API_END();
}

void SceneImpl::RemoveUnusedTextures() {
	API_BEGIN_NOARGS();

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedTextures();
}

void SceneImpl::RemoveUnusedMaterials() {
	API_BEGIN_NOARGS();

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedMaterials();

	API_END();
}

void SceneImpl::RemoveUnusedMeshes() {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedMeshes();

	API_END();
}

void SceneImpl::DefineImageMapUChar(const std::string &imgMapName,
		unsigned char *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		ChannelSelectionType selectionType, WrapType wrapType) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}, {}", ToArgString(imgMapName), (void *)pixels, gamma, channels,
			width, height, ToArgString(selectionType), ToArgString(wrapType));

	scene->DefineImageMap<u_char>(imgMapName, pixels, gamma, channels,
			width, height, (slg::ImageMapStorage::ChannelSelectionType)selectionType,
			(slg::ImageMapStorage::WrapType)wrapType);

	API_END();
}

void SceneImpl::DefineImageMapHalf(const std::string &imgMapName,
		unsigned short *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		ChannelSelectionType selectionType, WrapType wrapType) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}, {}", ToArgString(imgMapName), (void *)pixels, gamma, channels,
			width, height, ToArgString(selectionType), ToArgString(wrapType));

	scene->DefineImageMap<half>(imgMapName, (half *)pixels, gamma, channels,
			width, height, (slg::ImageMapStorage::ChannelSelectionType)selectionType,
			(slg::ImageMapStorage::WrapType)wrapType);

	API_END();
}

void SceneImpl::DefineImageMapFloat(const std::string &imgMapName,
		float *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		ChannelSelectionType selectionType, WrapType wrapType) {
	API_BEGIN("{}, {}, {}, {}, {}, {}, {}, {}", ToArgString(imgMapName), (void *)pixels, gamma, channels,
			width, height, ToArgString(selectionType), ToArgString(wrapType));

	scene->DefineImageMap<float>(imgMapName, pixels, gamma, channels,
			width, height, (slg::ImageMapStorage::ChannelSelectionType)selectionType,
			(slg::ImageMapStorage::WrapType)wrapType);

	API_END();
}

// Note: this method is not part of LuxCore API and it is used only internally
void SceneImpl::DefineMesh(ExtTriangleMesh *mesh) {
	API_BEGIN("{}", (void *)mesh);

	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DefineMesh(mesh);

	API_END();
}

const Properties &SceneImpl::ToProperties() const {
	API_BEGIN_NOARGS();

	if (!scenePropertiesCache.GetSize())
		scenePropertiesCache << scene->ToProperties(true);

	//API_RETURN("{}", ToArgString(scenePropertiesCache));
	API_END();

	return scenePropertiesCache;
}

void SceneImpl::Save(const std::string &fileName) const {
	API_BEGIN("{}", ToArgString(fileName));

	slg::Scene::SaveSerialized(fileName, scene);

	API_END();
}

Point *SceneImpl::AllocVerticesBuffer(const unsigned int meshVertCount) {
	API_BEGIN("{}", meshVertCount);

	Point *result = TriangleMesh::AllocVerticesBuffer(meshVertCount);

	API_RETURN("{}", (void *)result);
	
	return result;
}

Triangle *SceneImpl::AllocTrianglesBuffer(const unsigned int meshTriCount) {
	API_BEGIN("{}", meshTriCount);

	Triangle *result = TriangleMesh::AllocTrianglesBuffer(meshTriCount);

	API_RETURN("{}", (void *)result);
	
	return result;
}

//------------------------------------------------------------------------------
// RenderConfigImpl
//------------------------------------------------------------------------------

RenderConfigImpl::RenderConfigImpl(const Properties &props, SceneImpl *scn) {
	if (scn) {
		scene = scn;
		allocatedScene = false;
		renderConfig = new slg::RenderConfig(props, scene->scene);
	} else {
		renderConfig = new slg::RenderConfig(props);
		scene = new SceneImpl(renderConfig->scene);
		allocatedScene = true;
	}
}

RenderConfigImpl::RenderConfigImpl(const std::string &fileName) {
	renderConfig = slg::RenderConfig::LoadSerialized(fileName);
	scene = new SceneImpl(renderConfig->scene);
	allocatedScene = true;
}

RenderConfigImpl::RenderConfigImpl(const std::string &fileName, RenderStateImpl **startState,
			FilmImpl **startFilm) {
	SerializationInputFile sif(fileName);

	// Read the render configuration and the scene
	sif.GetArchive() >> renderConfig;
	scene = new SceneImpl(renderConfig->scene);
	allocatedScene = true;

	// Read the render state
	slg::RenderState *st;
	sif.GetArchive() >> st;
	*startState = new RenderStateImpl(st);

	// Save the film
	slg::Film *sf;
	sif.GetArchive() >> sf;
	*startFilm = new FilmImpl(sf);

	if (!sif.IsGood())
		throw runtime_error("Error while loading serialized render session: " + fileName);
}

RenderConfigImpl::~RenderConfigImpl() {
	delete renderConfig;
	if (allocatedScene)
		delete scene;
}

const Properties &RenderConfigImpl::GetProperties() const {
	API_BEGIN_NOARGS();

	const Properties &result = renderConfig->cfg;

	//API_RETURN("{}", ToArgString(result));
	API_END();

	return result;
}

const Property RenderConfigImpl::GetProperty(const std::string &name) const {
	API_BEGIN("{}", ToArgString(name));

	const Property result = renderConfig->GetProperty(name);

	API_RETURN("{}", ToArgString(result));

	return result;
}

const Properties &RenderConfigImpl::ToProperties() const {
	API_BEGIN_NOARGS();

	const Properties &result = renderConfig->ToProperties();

	//API_RETURN("{}", ToArgString(result));
	API_END();

	return result;
}

Scene &RenderConfigImpl::GetScene() const {
	API_BEGIN_NOARGS();
	
	Scene &result = *scene;

	API_RETURN("{}", (void *)&result);

	return result;
}

bool RenderConfigImpl::HasCachedKernels() const {
	API_BEGIN_NOARGS();

	bool result = renderConfig->HasCachedKernels();

	API_RETURN("{}", result);

	return result;
}

void RenderConfigImpl::Parse(const Properties &props) {
	API_BEGIN("{}", ToArgString(props));

	renderConfig->Parse(props);

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

	const bool result = slg::Film::GetFilmSize(renderConfig->cfg, filmFullWidth, filmFullHeight, filmSubRegion);
	
	API_RETURN("{}", result);

	return result;
}

void RenderConfigImpl::DeleteSceneOnExit() {
	API_BEGIN_NOARGS();

	allocatedScene = true;

	API_END();
}

void RenderConfigImpl::Save(const std::string &fileName) const {
	API_BEGIN("{}", ToArgString(fileName));

	slg::RenderConfig::SaveSerialized(fileName, renderConfig);

	API_END();
}

void RenderConfigImpl::Export(const std::string &dirName) const {
	API_BEGIN("{}", ToArgString(dirName));

	slg::FileSaverRenderEngine::ExportScene(renderConfig, dirName,
			renderConfig->GetProperty("renderengine.type").Get<string>());

	API_END();
}

void RenderConfigImpl::ExportGLTF(const std::string &fileName) const {
	API_BEGIN("{}", ToArgString(fileName));

	slg::FileSaverRenderEngine::ExportSceneGLTF(renderConfig, fileName);

	API_END();
}

const Properties &RenderConfigImpl::GetDefaultProperties() {
	API_BEGIN_NOARGS();

	const Properties &result = slg::RenderConfig::GetDefaultProperties();

	API_END();

	return result;
}

//------------------------------------------------------------------------------
// RenderStateImpl
//------------------------------------------------------------------------------

RenderStateImpl::RenderStateImpl(const std::string &fileName) {
	renderState = slg::RenderState::LoadSerialized(fileName);
}

RenderStateImpl::RenderStateImpl(slg::RenderState *state) {
	renderState = state;
}

RenderStateImpl::~RenderStateImpl() {
	delete renderState;
}

void RenderStateImpl::Save(const std::string &fileName) const {
	API_BEGIN("{}", ToArgString(fileName));

	renderState->SaveSerialized(fileName);

	API_END();
}

//------------------------------------------------------------------------------
// RenderSessionImpl
//------------------------------------------------------------------------------

RenderSessionImpl::RenderSessionImpl(const RenderConfigImpl *config, RenderStateImpl *startState, FilmImpl *startFilm) :
		renderConfig(config) {
	film = new FilmImpl(*this);

	renderSession = new slg::RenderSession(config->renderConfig,
			startState ? startState->renderState : NULL,
			startFilm ? startFilm->standAloneFilm : NULL);

	if (startState) {
		// slg::RenderSession will take care of deleting startState->renderState
		startState->renderState = NULL;
		// startState is not more a valid/usable object after this point, it can
		// only be deleted
	}

	if (startFilm) {
		// slg::RenderSession will take care of deleting startFilm->standAloneFilm too
		startFilm->standAloneFilm = NULL;
		// startFilm is not more a valid/usable object after this point, it can
		// only be deleted
	}
}

RenderSessionImpl::RenderSessionImpl(const RenderConfigImpl *config, const std::string &startStateFileName,
		const std::string &startFilmFileName) :
		renderConfig(config) {
	film = new FilmImpl(*this);

	unique_ptr<slg::Film> startFilm(slg::Film::LoadSerialized(startFilmFileName));
	unique_ptr<slg::RenderState> startState(slg::RenderState::LoadSerialized(startStateFileName));

	renderSession = new slg::RenderSession(config->renderConfig,
			startState.release(), startFilm.release());
}

RenderSessionImpl::~RenderSessionImpl() {
	delete film;
	delete renderSession;
}
const RenderConfig &RenderSessionImpl::GetRenderConfig() const {
	API_BEGIN_NOARGS();

	const RenderConfig &result = *renderConfig;

	API_RETURN("{}", (void *)&result);

	return result;
}

RenderState *RenderSessionImpl::GetRenderState() {
	API_BEGIN_NOARGS();

	RenderState *result = new RenderStateImpl(renderSession->GetRenderState());
	
	API_RETURN("{}", (void *)result);

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
	renderConfig->scene->scenePropertiesCache.Clear();

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

Film &RenderSessionImpl::GetFilm() {
	API_BEGIN_NOARGS();

	Film &result = *film;
	
	API_RETURN("{}", (void *)&result);

	return result;
}

static void SetTileProperties(Properties &props, const string &prefix,
		const deque<const slg::Tile *> &tiles) {
	props.Set(Property(prefix + ".count")((unsigned int)tiles.size()));
	Property tileCoordProp(prefix + ".coords");
	Property tilePassProp(prefix + ".pass");
	Property tilePendingPassesProp(prefix + ".pendingpasses");
	Property tileErrorProp(prefix + ".error");

	BOOST_FOREACH(const slg::Tile *tile, tiles) {
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

	stats.Set(Property("stats.renderengine.total.raysec")(renderSession->renderEngine->GetTotalRaysSec()));
	stats.Set(Property("stats.renderengine.total.samplesec")(renderSession->renderEngine->GetTotalSamplesSec()));
	stats.Set(Property("stats.renderengine.total.samplesec.eye")(renderSession->renderEngine->GetTotalEyeSamplesSec()));
	stats.Set(Property("stats.renderengine.total.samplesec.light")(renderSession->renderEngine->GetTotalLightSamplesSec()));
	stats.Set(Property("stats.renderengine.total.samplecount")(renderSession->renderEngine->GetTotalSampleCount()));
	stats.Set(Property("stats.renderengine.pass")(renderSession->renderEngine->GetPass()));
	stats.Set(Property("stats.renderengine.pass.eye")(renderSession->renderEngine->GetEyePass()));
	stats.Set(Property("stats.renderengine.pass.light")(renderSession->renderEngine->GetLightPass()));
	stats.Set(Property("stats.renderengine.time")(renderSession->renderEngine->GetRenderingTime()));
	stats.Set(Property("stats.renderengine.convergence")(renderSession->film->GetConvergence()));

	// Intersection devices statistics
	const vector<IntersectionDevice *> &idevices = renderSession->renderEngine->GetIntersectionDevices();

	boost::unordered_map<string, unsigned int> devCounters;
	Property devicesNames("stats.renderengine.devices");
	double totalPerf = 0.0;
	BOOST_FOREACH(IntersectionDevice *dev, idevices) {
		const string &devName = dev->GetName();

		// Append a device index for the case where the same device is used multiple times
		unsigned int index = devCounters[devName]++;
		const string uniqueName = devName + "-" + ToString(index);
		devicesNames.Add(uniqueName);

		const string prefix = "stats.renderengine.devices." + uniqueName;
		totalPerf += dev->GetTotalPerformance();
		stats.Set(Property(prefix + ".performance.total")(dev->GetTotalPerformance()));
		stats.Set(Property(prefix + ".performance.serial")(dev->GetSerialPerformance()));
		stats.Set(Property(prefix + ".performance.dataparallel")(dev->GetDataParallelPerformance()));

		const HardwareDevice *hardDev = dynamic_cast<const HardwareDevice *>(dev);
		if (hardDev) {
			stats.Set(Property(prefix + ".memory.total")((u_longlong)hardDev->GetDeviceDesc()->GetMaxMemory()));
			stats.Set(Property(prefix + ".memory.used")((u_longlong)hardDev->GetUsedMemory()));
		} else {
			stats.Set(Property(prefix + ".memory.total")(0ull));
			stats.Set(Property(prefix + ".memory.used")(0ull));			
		}
	}
	stats.Set(devicesNames);
	stats.Set(Property("stats.renderengine.performance.total")(totalPerf));

	// The explicit cast to size_t is required by VisualC++
	stats.Set(Property("stats.dataset.trianglecount")(renderSession->renderConfig->scene->dataSet->GetTotalTriangleCount()));

	// Some engine specific statistic
	switch (renderSession->renderEngine->GetType()) {
#if !defined(LUXRAYS_DISABLE_OPENCL)
		case slg::RTPATHOCL: {
			slg::RTPathOCLRenderEngine *engine = (slg::RTPathOCLRenderEngine *)renderSession->renderEngine;
			stats.Set(Property("stats.rtpathocl.frame.time")(engine->GetFrameTime()));
			break;
		}
		case slg::TILEPATHOCL: {
			slg::TilePathOCLRenderEngine *engine = (slg::TilePathOCLRenderEngine *)renderSession->renderEngine;

			stats.Set(Property("stats.tilepath.tiles.size.x")(engine->GetTileWidth()));
			stats.Set(Property("stats.tilepath.tiles.size.y")(engine->GetTileHeight()));

			// Pending tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetPendingTiles(tiles);
				SetTileProperties(stats, "stats.tilepath.tiles.pending", tiles);
			}

			// Not converged tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetNotConvergedTiles(tiles);
				SetTileProperties(stats, "stats.tilepath.tiles.notconverged", tiles);
			}

			// Converged tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetConvergedTiles(tiles);
				SetTileProperties(stats, "stats.tilepath.tiles.converged", tiles);
			}
			break;
		}
#endif
		case slg::TILEPATHCPU: {
			slg::CPUTileRenderEngine *engine = (slg::CPUTileRenderEngine *)renderSession->renderEngine;

			stats.Set(Property("stats.tilepath.tiles.size.x")(engine->GetTileWidth()));
			stats.Set(Property("stats.tilepath.tiles.size.y")(engine->GetTileHeight()));

			// Pending tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetPendingTiles(tiles);
				SetTileProperties(stats, "stats.tilepath.tiles.pending", tiles);
			}

			// Not converged tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetNotConvergedTiles(tiles);
				SetTileProperties(stats, "stats.tilepath.tiles.notconverged", tiles);
			}

			// Converged tiles
			{
				deque<const slg::Tile *> tiles;
				engine->GetConvergedTiles(tiles);
				SetTileProperties(stats, "stats.tilepath.tiles.converged", tiles);
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

const Properties &RenderSessionImpl::GetStats() const {
	API_BEGIN_NOARGS();
	
	const Properties &result = stats;

	//API_RETURN("{}", ToArgString(result));
	API_END();

	return result;
}

void RenderSessionImpl::Parse(const Properties &props) {
	API_BEGIN("{}", ToArgString(props));

	renderSession->Parse(props);

	API_END();
}
void RenderSessionImpl::SaveResumeFile(const std::string &fileName) {
	API_BEGIN("{}", ToArgString(fileName));

	renderSession->SaveResumeFile(fileName);

	API_END();
}
