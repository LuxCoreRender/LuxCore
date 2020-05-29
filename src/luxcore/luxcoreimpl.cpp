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
	return GetSLGFilm()->GetWidth();
}

unsigned int FilmImpl::GetHeight() const {
	return GetSLGFilm()->GetHeight();
}

luxrays::Properties FilmImpl::GetStats() const {
	slg::Film *film = GetSLGFilm();

	Properties stats;
	stats.Set(Property("stats.film.total.samplecount")(film->GetTotalSampleCount()));
	stats.Set(Property("stats.film.spp")(film->GetTotalSampleCount() / static_cast<float>(film->GetWidth() * film->GetHeight())));
	stats.Set(Property("stats.film.radiancegorup.count")(film->GetRadianceGroupCount()));

	return stats;
}

float FilmImpl::GetFilmY(const u_int imagePipelineIndex) const {
	return GetSLGFilm()->GetFilmY(imagePipelineIndex);
}

void FilmImpl::Clear() {
	return GetSLGFilm()->Clear();
}

void FilmImpl::AddFilm(const Film &film) {
	const FilmImpl *filmImpl = dynamic_cast<const FilmImpl *>(&film);
	assert (filmImpl);

	AddFilm(film, 0, 0, filmImpl->GetWidth(), filmImpl->GetHeight(), 0, 0);
}

void FilmImpl::AddFilm(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY) {
	const FilmImpl *srcFilmImpl = dynamic_cast<const FilmImpl *>(&film);
	assert (srcFilmImpl);
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

unsigned int FilmImpl::GetOutputCount(const FilmOutputType type) const {
	return GetSLGFilm()->GetOutputCount((slg::FilmOutputs::FilmOutputType)type);
}

size_t FilmImpl::GetOutputSize(const FilmOutputType type) const {
	return GetSLGFilm()->GetOutputSize((slg::FilmOutputs::FilmOutputType)type);
}

unsigned int FilmImpl::GetRadianceGroupCount() const {
	return GetSLGFilm()->GetRadianceGroupCount();
}

void FilmImpl::GetOutputFloat(const FilmOutputType type, float *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->GetOutput<float>((slg::FilmOutputs::FilmOutputType)type,
				buffer, index, executeImagePipeline);
	} else
		standAloneFilm->GetOutput<float>((slg::FilmOutputs::FilmOutputType)type,
				buffer, index, executeImagePipeline);
}

void FilmImpl::GetOutputUInt(const FilmOutputType type, unsigned int *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->GetOutput<u_int>((slg::FilmOutputs::FilmOutputType)type,
				buffer, index, executeImagePipeline);
	} else
		standAloneFilm->GetOutput<unsigned int>((slg::FilmOutputs::FilmOutputType)type,
				buffer, index, executeImagePipeline);
}

void FilmImpl::UpdateOutputFloat(const FilmOutputType type, const float *buffer,
		const unsigned int index, const bool executeImagePipeline) {
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
}

void FilmImpl::UpdateOutputUInt(const FilmOutputType type, const unsigned int *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	throw runtime_error("No channel can be updated with Film::UpdateOutput<unsigned int>()");
}

bool FilmImpl::HasChannel(const FilmChannelType type) const {
	return GetSLGFilm()->HasChannel((slg::Film::FilmChannelType)type);
}

unsigned int FilmImpl::GetChannelCount(const FilmChannelType type) const {
	return GetSLGFilm()->GetChannelCount((slg::Film::FilmChannelType)type);
}

const float *FilmImpl::GetChannelFloat(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		return renderSession->renderSession->film->GetChannel<float>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);
	} else
		return standAloneFilm->GetChannel<float>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);
}

const unsigned int *FilmImpl::GetChannelUInt(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		return renderSession->renderSession->film->GetChannel<unsigned int>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);
	} else
		return standAloneFilm->GetChannel<unsigned int>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);
}

float *FilmImpl::UpdateChannelFloat(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	if (type != CHANNEL_USER_IMPORTANCE)
		throw runtime_error("Only USER_IMPORTANCE channel can be updated with Film::UpdateChannel<float>()");

	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		return renderSession->renderSession->film->GetChannel<float>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);
	} else
		return standAloneFilm->GetChannel<float>((slg::Film::FilmChannelType)type,
				index, executeImagePipeline);
}

unsigned int *FilmImpl::UpdateChannelUInt(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	throw runtime_error("No channel can be updated with Film::UpdateChannel<unsigned int>()");
}

void FilmImpl::Parse(const luxrays::Properties &props) {
	if (renderSession)
		throw runtime_error("Film::Parse() can be used only with a stand alone Film");
	else
		standAloneFilm->Parse(props);
}

void FilmImpl::DeleteAllImagePipelines()  {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->SetImagePipelines(NULL);
		renderSession->renderSession->renderConfig->DeleteAllFilmImagePipelinesProperties();
	} else
		standAloneFilm->SetImagePipelines(NULL);
}

void FilmImpl::ExecuteImagePipeline(const u_int index) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->ExecuteImagePipeline(index);
	} else
		standAloneFilm->ExecuteImagePipeline(index);
}

void FilmImpl::AsyncExecuteImagePipeline(const u_int index) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->AsyncExecuteImagePipeline(index);
	} else
		standAloneFilm->AsyncExecuteImagePipeline(index);
}

void FilmImpl::WaitAsyncExecuteImagePipeline() {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->WaitAsyncExecuteImagePipeline();
	} else
		standAloneFilm->WaitAsyncExecuteImagePipeline();
}

bool FilmImpl::HasDoneAsyncExecuteImagePipeline() {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		return renderSession->renderSession->film->HasDoneAsyncExecuteImagePipeline();
	} else
		return standAloneFilm->HasDoneAsyncExecuteImagePipeline();
}

//------------------------------------------------------------------------------
// CameraImpl
//------------------------------------------------------------------------------

CameraImpl::CameraImpl(const SceneImpl &scn) : scene(scn) {
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

	const string ext = slg::GetFileNameExt(fileName);
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
	const BBox &worldBBox = scene->dataSet->GetBBox();

	min[0] = worldBBox.pMin.x;
	min[1] = worldBBox.pMin.y;
	min[2] = worldBBox.pMin.z;

	max[0] = worldBBox.pMax.x;
	max[1] = worldBBox.pMax.y;
	max[2] = worldBBox.pMax.z;
}

const Camera &SceneImpl::GetCamera() const {
	return *camera;
}

bool SceneImpl::IsImageMapDefined(const std::string &imgMapName) const {
	return scene->IsImageMapDefined(imgMapName);
}

void SceneImpl::SetDeleteMeshData(const bool v) {
	scene->extMeshCache.SetDeleteMeshData(v);
}

void SceneImpl::SetMeshAppliedTransformation(const std::string &meshName,
			const float appliedTransMat[16]) {
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
}

void SceneImpl::DefineMesh(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		float *p, unsigned int *vi, float *n,
		float *uvs, float *cols, float *alphas) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DefineMesh(meshName, plyNbVerts, plyNbTris, (Point *)p,
			(Triangle *)vi, (Normal *)n,
			(UV *)uvs, (Spectrum *)cols, alphas);
}

void SceneImpl::DefineMeshExt(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		float *p, unsigned int *vi, float *n,
		array<float *, LC_MESH_MAX_DATA_COUNT> *uvs,
		array<float *, LC_MESH_MAX_DATA_COUNT> *cols,
		array<float *, LC_MESH_MAX_DATA_COUNT> *alphas) {
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
}

void SceneImpl::SetMeshVertexAOV(const string &meshName,
		const unsigned int index, float *data) {
	scene->SetMeshVertexAOV(meshName, index, data);
}

void SceneImpl::SetMeshTriangleAOV(const string &meshName,
		const unsigned int index, float *data) {
	scene->SetMeshTriangleAOV(meshName, index, data);
}

void SceneImpl::SaveMesh(const string &meshName, const string &fileName) {
	const ExtMesh *mesh = scene->extMeshCache.GetExtMesh(meshName);
	mesh->Save(fileName);
}

void SceneImpl::DefineStrands(const string &shapeName, const cyHairFile &strandsFile,
		const StrandsTessellationType tesselType,
		const unsigned int adaptiveMaxDepth, const float adaptiveError,
		const unsigned int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const bool useCameraPosition) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DefineStrands(shapeName, strandsFile,
			(slg::StrendsShape::TessellationType)tesselType, adaptiveMaxDepth, adaptiveError,
			solidSideCount, solidCapBottom, solidCapTop,
			useCameraPosition);
}

bool SceneImpl::IsMeshDefined(const std::string &meshName) const {
	return scene->IsMeshDefined(meshName);
}

bool SceneImpl::IsTextureDefined(const std::string &texName) const {
	return scene->IsTextureDefined(texName);
}

bool SceneImpl::IsMaterialDefined(const std::string &matName) const {
	return scene->IsMaterialDefined(matName);
}

const unsigned int SceneImpl::GetLightCount() const {
	return scene->lightDefs.GetSize();
}

const unsigned int  SceneImpl::GetObjectCount() const {
	return scene->objDefs.GetSize();
}

void SceneImpl::Parse(const Properties &props) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->Parse(props);
}

void SceneImpl::DuplicateObject(const std::string &srcObjName, const std::string &dstObjName,
		const float transMat[16], const unsigned int objectID) {
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
}

void SceneImpl::DuplicateObject(const std::string &srcObjName, const std::string &dstObjNamePrefix,
			const unsigned int count, const float *transMats, const unsigned int *objectIDs) {
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
}

void SceneImpl::DuplicateObject(const std::string &srcObjName, const std::string &dstObjName,
		const u_int steps, const float *times, const float *transMats, const unsigned int objectID) {
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
}

void SceneImpl::DuplicateObject(const std::string &srcObjName, const std::string &dstObjNamePrefix,
		const unsigned int count, const u_int steps, const float *times, const float *transMats,
		const unsigned int *objectIDs) {
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
}

void SceneImpl::UpdateObjectTransformation(const std::string &objName, const float transMat[16]) {
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
}

void SceneImpl::UpdateObjectMaterial(const std::string &objName, const std::string &matName) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->UpdateObjectMaterial(objName, matName);
}

void SceneImpl::DeleteObject(const string &objName) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DeleteObject(objName);
}

void SceneImpl::DeleteLight(const string &lightName) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DeleteLight(lightName);
}

void SceneImpl::RemoveUnusedImageMaps() {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedImageMaps();
}

void SceneImpl::RemoveUnusedTextures() {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedTextures();
}

void SceneImpl::RemoveUnusedMaterials() {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedMaterials();
}

void SceneImpl::RemoveUnusedMeshes() {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedMeshes();
}

void SceneImpl::DefineImageMapUChar(const std::string &imgMapName,
		unsigned char *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		ChannelSelectionType selectionType, WrapType wrapType) {
	scene->DefineImageMap<u_char>(imgMapName, pixels, gamma, channels,
			width, height, (slg::ImageMapStorage::ChannelSelectionType)selectionType,
			(slg::ImageMapStorage::WrapType)wrapType);
}

void SceneImpl::DefineImageMapHalf(const std::string &imgMapName,
		unsigned short *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		ChannelSelectionType selectionType, WrapType wrapType) {
	scene->DefineImageMap<half>(imgMapName, (half *)pixels, gamma, channels,
			width, height, (slg::ImageMapStorage::ChannelSelectionType)selectionType,
			(slg::ImageMapStorage::WrapType)wrapType);
}

void SceneImpl::DefineImageMapFloat(const std::string &imgMapName,
		float *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		ChannelSelectionType selectionType, WrapType wrapType) {
	scene->DefineImageMap<float>(imgMapName, pixels, gamma, channels,
			width, height, (slg::ImageMapStorage::ChannelSelectionType)selectionType,
			(slg::ImageMapStorage::WrapType)wrapType);
}

// Note: this method is not part of LuxCore API and it is used only internally
void SceneImpl::DefineMesh(ExtTriangleMesh *mesh) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DefineMesh(mesh);
}

const Properties &SceneImpl::ToProperties() const {
	if (!scenePropertiesCache.GetSize())
		scenePropertiesCache << scene->ToProperties(true);

	return scenePropertiesCache;
}

void SceneImpl::Save(const std::string &fileName) const {
	slg::Scene::SaveSerialized(fileName, scene);
}

Point *SceneImpl::AllocVerticesBuffer(const unsigned int meshVertCount) {
	return TriangleMesh::AllocVerticesBuffer(meshVertCount);
}

Triangle *SceneImpl::AllocTrianglesBuffer(const unsigned int meshTriCount) {
	return TriangleMesh::AllocTrianglesBuffer(meshTriCount);
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
	return renderConfig->cfg;
}

const Property RenderConfigImpl::GetProperty(const std::string &name) const {
	return renderConfig->GetProperty(name);
}

const Properties &RenderConfigImpl::ToProperties() const {
	return renderConfig->ToProperties();
}

Scene &RenderConfigImpl::GetScene() const {
	return *scene;
}

bool RenderConfigImpl::HasCachedKernels() const {
	return renderConfig->HasCachedKernels();
}

void RenderConfigImpl::Parse(const Properties &props) {
	renderConfig->Parse(props);
}

void RenderConfigImpl::Delete(const string &prefix) {
	renderConfig->Delete(prefix);
}

bool RenderConfigImpl::GetFilmSize(unsigned int *filmFullWidth, unsigned int *filmFullHeight,
		unsigned int *filmSubRegion) const {
	return slg::Film::GetFilmSize(renderConfig->cfg, filmFullWidth, filmFullHeight, filmSubRegion);
}

void RenderConfigImpl::DeleteSceneOnExit() {
	allocatedScene = true;
}

void RenderConfigImpl::Save(const std::string &fileName) const {
	slg::RenderConfig::SaveSerialized(fileName, renderConfig);
}

void RenderConfigImpl::Export(const std::string &dirName) const {
	slg::FileSaverRenderEngine::ExportScene(renderConfig, dirName,
			renderConfig->GetProperty("renderengine.type").Get<string>());
}

void RenderConfigImpl::ExportGLTF(const std::string &fileName) const {
	slg::FileSaverRenderEngine::ExportSceneGLTF(renderConfig, fileName);
}

const Properties &RenderConfigImpl::GetDefaultProperties() {
	return slg::RenderConfig::GetDefaultProperties();
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
	renderState->SaveSerialized(fileName);
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
	return *renderConfig;
}

RenderState *RenderSessionImpl::GetRenderState() {
	return new RenderStateImpl(renderSession->GetRenderState());
}

void RenderSessionImpl::Start() {
	renderSession->Start();

	// In order to populate the stats.* Properties
	UpdateStats();
}

void RenderSessionImpl::Stop() {
	renderSession->Stop();
}

bool RenderSessionImpl::IsStarted() const {
	return renderSession->IsStarted();
}

void RenderSessionImpl::BeginSceneEdit() {
	renderSession->BeginSceneEdit();
}

void RenderSessionImpl::EndSceneEdit() {
	renderSession->EndSceneEdit();

	// Invalidate the scene properties cache
	renderConfig->scene->scenePropertiesCache.Clear();
}

bool RenderSessionImpl::IsInSceneEdit() const {
	return renderSession->IsInSceneEdit();
}

void RenderSessionImpl::Pause() {
	renderSession->Pause();
}

void RenderSessionImpl::Resume() {
	renderSession->Resume();
}

bool RenderSessionImpl::IsInPause() const {
	return renderSession->IsInPause();
}

bool RenderSessionImpl::HasDone() const {
	return renderSession->renderEngine->HasDone();
}

void RenderSessionImpl::WaitForDone() const {
	renderSession->renderEngine->WaitForDone();
}

void RenderSessionImpl::WaitNewFrame() {
	renderSession->renderEngine->WaitNewFrame();
}

Film &RenderSessionImpl::GetFilm() {
	return *film;
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
	// It is not really correct to call UpdateStats() outside a Start()/Stop()
	// however it is easy to avoid any harm if it is done.
	if (!renderSession->IsStarted())
		return;

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
}

const Properties &RenderSessionImpl::GetStats() const {
	return stats;
}

void RenderSessionImpl::Parse(const Properties &props) {
	renderSession->Parse(props);
}
void RenderSessionImpl::SaveResumeFile(const std::string &fileName) {
	renderSession->SaveResumeFile(fileName);
}
