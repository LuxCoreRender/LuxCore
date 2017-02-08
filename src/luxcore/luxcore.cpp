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

#include <boost/thread/once.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/format.hpp>

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/virtualdevice.h"
#include "slg/slg.h"
#include "slg/engines/tilerepository.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/engines/oclrenderengine.h"
#include "slg/engines/biaspathocl/biaspathocl.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/engines/rtbiaspathocl/rtbiaspathocl.h"
#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// Initialization and logging
//------------------------------------------------------------------------------

void (*luxcore::LuxCore_LogHandler)(const char *msg) = NULL;

static double lcInitTime;

static void DefaultDebugHandler(const char *msg) {
	cerr << msg << endl;
}

static void LuxRaysDebugHandler(const char *msg) {
	if (LuxCore_LogHandler)
		LuxCore_LogHandler((boost::format("[LuxRays][%.3f] %s") % (WallClockTime() - lcInitTime) % msg).str().c_str());
}

static void SDLDebugHandler(const char *msg) {
	if (LuxCore_LogHandler)
		LuxCore_LogHandler((boost::format("[SDL][%.3f] %s") % (WallClockTime() - lcInitTime) % msg).str().c_str());
}

static void SLGDebugHandler(const char *msg) {
	if (LuxCore_LogHandler)
		LuxCore_LogHandler((boost::format("[LuxCore][%.3f] %s") % (WallClockTime() - lcInitTime) % msg).str().c_str());
}

void luxcore::Init(void (*LogHandler)(const char *)) {
	// To be thread safe
	static boost::mutex initMutex;
	boost::unique_lock<boost::mutex> lock(initMutex);
	
	lcInitTime = WallClockTime();
	
	// Set all debug handlers
	if (LogHandler) {
		// User provided handler
		LuxCore_LogHandler = LogHandler;
	} else {
		// Default handler
		LuxCore_LogHandler = DefaultDebugHandler;
	}

	slg::LuxRays_DebugHandler = ::LuxRaysDebugHandler;
	slg::SLG_DebugHandler = ::SLGDebugHandler;
	slg::SLG_SDLDebugHandler = ::SDLDebugHandler;	
}

//------------------------------------------------------------------------------
// ParseLXS
//------------------------------------------------------------------------------

extern FILE *luxcore_parserlxs_yyin;
extern int luxcore_parserlxs_yyparse(void);
extern void luxcore_parserlxs_yyrestart(FILE *new_file);

namespace luxcore { namespace parselxs {
extern void IncludeClear();
extern void ResetParser();

extern string currentFile;
extern u_int lineNum;

extern Properties overwriteProps;
extern Properties *renderConfigProps;
extern Properties *sceneProps;

} }

void luxcore::ParseLXS(const string &fileName, Properties &renderConfigProps, Properties &sceneProps) {
	// Otherwise the code is not thread-safe
	static boost::mutex parseLXSMutex;
	boost::unique_lock<boost::mutex> lock(parseLXSMutex);

	luxcore::parselxs::renderConfigProps = &renderConfigProps;
	luxcore::parselxs::sceneProps = &sceneProps;
	luxcore::parselxs::ResetParser();

	bool parseSuccess = false;

	if (fileName == "-")
		luxcore_parserlxs_yyin = stdin;
	else
		luxcore_parserlxs_yyin = fopen(fileName.c_str(), "r");

	if (luxcore_parserlxs_yyin != NULL) {
		luxcore::parselxs::currentFile = fileName;
		if (luxcore_parserlxs_yyin == stdin)
			luxcore::parselxs::currentFile = "<standard input>";
		luxcore::parselxs::lineNum = 1;
		// Make sure to flush any buffers before parsing
		luxcore::parselxs::IncludeClear();
		luxcore_parserlxs_yyrestart(luxcore_parserlxs_yyin);
		try {
			parseSuccess = (luxcore_parserlxs_yyparse() == 0);

			// Overwrite properties with Renderer command one
			luxcore::parselxs::renderConfigProps->Set(luxcore::parselxs::overwriteProps);
		} catch (std::runtime_error &e) {
			throw runtime_error("Exception during parsing (file '" + luxcore::parselxs::currentFile +
					"', line: " + ToString(luxcore::parselxs::lineNum) + "): " + e.what());
		}
		
		if (luxcore_parserlxs_yyin != stdin)
			fclose(luxcore_parserlxs_yyin);
	} else
		throw runtime_error("Unable to read scene file: " + fileName);

	luxcore::parselxs::currentFile = "";
	luxcore::parselxs::lineNum = 0;

	if ((luxcore_parserlxs_yyin == NULL) || !parseSuccess)
		throw runtime_error("Parsing failed: " + fileName);
}

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

Film::Film(const std::string &fileName) : renderSession(NULL) {
	standAloneFilm = slg::Film::LoadSerialized(fileName);
}

Film::Film(const RenderSession &session) : renderSession(&session),
		standAloneFilm(NULL) {
}

Film::~Film() {
	delete standAloneFilm;
}

slg::Film *Film::GetSLGFilm() const {
	if (renderSession)
		return renderSession->renderSession->film;
	else
		return standAloneFilm;
}

u_int Film::GetWidth() const {
	return GetSLGFilm()->GetWidth();
}

u_int Film::GetHeight() const {
	return GetSLGFilm()->GetHeight();
}

void Film::SaveOutputs() const {
	if (renderSession)
		renderSession->renderSession->SaveFilmOutputs();
	else
		throw runtime_error("Film::SaveOutputs() can not be used with a stand alone Film");
}

void Film::SaveOutput(const std::string &fileName, const FilmOutputType type, const Properties &props) const {
	GetSLGFilm()->Output(fileName, (slg::FilmOutputs::FilmOutputType)type, &props);
}

void Film::SaveFilm(const string &fileName) const {
	if (renderSession)
		renderSession->renderSession->SaveFilm(fileName);
	else
		slg::Film::SaveSerialized(fileName, standAloneFilm);
}

double Film::GetTotalSampleCount() const {
	return GetSLGFilm()->GetTotalSampleCount(); 
}

bool Film::HasOutput(const FilmOutputType type) const {
	return GetSLGFilm()->HasOutput((slg::FilmOutputs::FilmOutputType)type);
}

size_t Film::GetOutputSize(const FilmOutputType type) const {
	return GetSLGFilm()->GetOutputSize((slg::FilmOutputs::FilmOutputType)type);
}

u_int Film::GetRadianceGroupCount() const {
	return GetSLGFilm()->GetRadianceGroupCount();
}

template<> void Film::GetOutput<float>(const FilmOutputType type, float *buffer, const u_int index) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->GetOutput<float>((slg::FilmOutputs::FilmOutputType)type, buffer, index);
	} else
		standAloneFilm->GetOutput<float>((slg::FilmOutputs::FilmOutputType)type, buffer, index);
}

template<> void Film::GetOutput<u_int>(const FilmOutputType type, u_int *buffer, const u_int index) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		renderSession->renderSession->film->GetOutput<u_int>((slg::FilmOutputs::FilmOutputType)type, buffer, index);
	} else
		standAloneFilm->GetOutput<u_int>((slg::FilmOutputs::FilmOutputType)type, buffer, index);
}

u_int Film::GetChannelCount(const FilmChannelType type) const {
	return GetSLGFilm()->GetChannelCount((slg::Film::FilmChannelType)type);
}

template<> const float *Film::GetChannel<float>(const FilmChannelType type, const u_int index) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		return renderSession->renderSession->film->GetChannel<float>((slg::Film::FilmChannelType)type, index);
	} else
		return standAloneFilm->GetChannel<float>((slg::Film::FilmChannelType)type, index);
}

template<> const u_int *Film::GetChannel<u_int>(const FilmChannelType type, const u_int index) {
	if (renderSession) {
		boost::unique_lock<boost::mutex> lock(renderSession->renderSession->filmMutex);

		return renderSession->renderSession->film->GetChannel<u_int>((slg::Film::FilmChannelType)type, index);
	} else
		return standAloneFilm->GetChannel<u_int>((slg::Film::FilmChannelType)type, index);
}

void Film::Parse(const luxrays::Properties &props) {
	if (renderSession)
		throw runtime_error("Film::Parse() can be used only with a stand alone Film");
	else
		standAloneFilm->Parse(props);
}

//------------------------------------------------------------------------------
// Camera
//------------------------------------------------------------------------------

Camera::Camera(const Scene &scn) : scene(scn) {
}

Camera::~Camera() {
}

const Camera::CameraType Camera::GetType() const {
	return (Camera::CameraType)scene.scene->camera->GetType();
}

void Camera::Translate(const Vector &t) const {
	scene.scene->camera->Translate(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void Camera::TranslateLeft(const float t) const {
	scene.scene->camera->TranslateLeft(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void Camera::TranslateRight(const float t) const {
	scene.scene->camera->TranslateRight(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void Camera::TranslateForward(const float t) const {
	scene.scene->camera->TranslateForward(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void Camera::TranslateBackward(const float t) const {
	scene.scene->camera->TranslateBackward(t);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void Camera::Rotate(const float angle, const Vector &axis) const {
	scene.scene->camera->Rotate(angle, axis);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void Camera::RotateLeft(const float angle) const {
	scene.scene->camera->RotateLeft(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void Camera::RotateRight(const float angle) const {
	scene.scene->camera->RotateRight(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void Camera::RotateUp(const float angle) const {
	scene.scene->camera->RotateUp(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

void Camera::RotateDown(const float angle) const {
	scene.scene->camera->RotateDown(angle);
	scene.scene->editActions.AddAction(slg::CAMERA_EDIT);
}

//------------------------------------------------------------------------------
// Scene
//------------------------------------------------------------------------------

Scene::Scene(const float imageScale) : camera(*this) {
	scene = new slg::Scene(imageScale);
	allocatedScene = true;
}

Scene::Scene(const string &fileName, const float imageScale) : camera(*this) {
	scene = new slg::Scene(fileName, imageScale);
	allocatedScene = true;
}

Scene::Scene(slg::Scene *scn) : camera(*this) {
	scene = scn;
	allocatedScene = false;
}

Scene::~Scene() {
	if (allocatedScene)
		delete scene;
}

const DataSet &Scene::GetDataSet() const {
	return *(scene->dataSet);
}

const Camera &Scene::GetCamera() const {
	return camera;
}

bool Scene::IsImageMapDefined(const std::string &imgMapName) const {
	return scene->IsImageMapDefined(imgMapName);
}

void Scene::SetDeleteMeshData(const bool v) {
	scene->extMeshCache.SetDeleteMeshData(v);
}

void Scene::DefineMesh(const string &meshName, ExtTriangleMesh *mesh) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DefineMesh(meshName, mesh);
}

void Scene::DefineMesh(const string &meshName,
	const long plyNbVerts, const long plyNbTris,
	Point *p, Triangle *vi, Normal *n, UV *uv,
	Spectrum *cols, float *alphas) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DefineMesh(meshName, plyNbVerts, plyNbTris, p, vi, n, uv, cols, alphas);
}

void Scene::SaveMesh(const string &meshName, const string &fileName) {
	const ExtMesh *mesh = scene->extMeshCache.GetExtMesh(meshName);
	mesh->WritePly(fileName);
}

void Scene::DefineStrands(const string &shapeName, const luxrays::cyHairFile &strandsFile,
		const StrandsTessellationType tesselType,
		const u_int adaptiveMaxDepth, const float adaptiveError,
		const u_int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const bool useCameraPosition) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DefineStrands(shapeName, strandsFile,
			(slg::StrendsShape::TessellationType)tesselType, adaptiveMaxDepth, adaptiveError,
			solidSideCount, solidCapBottom, solidCapTop,
			useCameraPosition);
}

bool Scene::IsMeshDefined(const std::string &meshName) const {
	return scene->IsMeshDefined(meshName);
}

bool Scene::IsTextureDefined(const std::string &texName) const {
	return scene->IsTextureDefined(texName);
}

bool Scene::IsMaterialDefined(const std::string &matName) const {
	return scene->IsMaterialDefined(matName);
}

const u_int Scene::GetLightCount() const {
	return scene->lightDefs.GetSize();
}

const u_int  Scene::GetObjectCount() const {
	return scene->objDefs.GetSize();
}

void Scene::Parse(const Properties &props) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->Parse(props);
}

void Scene::UpdateObjectTransformation(const std::string &objName, const luxrays::Transform &trans) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->UpdateObjectTransformation(objName, trans);
}

void Scene::UpdateObjectMaterial(const std::string &objName, const std::string &matName) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->UpdateObjectMaterial(objName, matName);
}

void Scene::DeleteObject(const string &objName) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DeleteObject(objName);
}

void Scene::DeleteLight(const string &lightName) {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->DeleteLight(lightName);
}

void Scene::RemoveUnusedImageMaps() {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedImageMaps();
}

void Scene::RemoveUnusedTextures() {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedTextures();
}

void Scene::RemoveUnusedMaterials() {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedMaterials();
}

void Scene::RemoveUnusedMeshes() {
	// Invalidate the scene properties cache
	scenePropertiesCache.Clear();

	scene->RemoveUnusedMeshes();
}

const Properties &Scene::ToProperties() const {
	if (!scenePropertiesCache.GetSize())
		scenePropertiesCache << scene->ToProperties();

	return scenePropertiesCache;
}

Point *Scene::AllocVerticesBuffer(const u_int meshVertCount) {
	return TriangleMesh::AllocVerticesBuffer(meshVertCount);
}

Triangle *Scene::AllocTrianglesBuffer(const u_int meshTriCount) {
	return TriangleMesh::AllocTrianglesBuffer(meshTriCount);
}

//------------------------------------------------------------------------------
// RenderConfig
//------------------------------------------------------------------------------

RenderConfig::RenderConfig(const Properties &props, Scene *scn) {
	if (scn) {
		scene = scn;
		allocatedScene = false;
		renderConfig = new slg::RenderConfig(props, scn->scene);
	} else {
		renderConfig = new slg::RenderConfig(props);
		scene = new Scene(renderConfig->scene);
		allocatedScene = true;
	}
}

RenderConfig::~RenderConfig() {
	delete renderConfig;
	if (allocatedScene)
		delete scene;
}

const Properties &RenderConfig::GetProperties() const {
	return renderConfig->cfg;
}

const Property RenderConfig::GetProperty(const std::string &name) const {
	return renderConfig->GetProperty(name);
}

const Properties &RenderConfig::ToProperties() const {
	return renderConfig->ToProperties();
}

Scene &RenderConfig::GetScene() const {
	return *scene;
}

void RenderConfig::Parse(const Properties &props) {
	renderConfig->Parse(props);
}

void RenderConfig::Delete(const string &prefix) {
	renderConfig->Delete(prefix);
}

bool RenderConfig::GetFilmSize(u_int *filmFullWidth, u_int *filmFullHeight,
		u_int *filmSubRegion) const {
	return renderConfig->GetFilmSize(filmFullWidth, filmFullHeight, filmSubRegion);
}

void RenderConfig::DeleteSceneOnExit() {
	allocatedScene = true;
}

const Properties &RenderConfig::GetDefaultProperties() {
	return slg::RenderConfig::GetDefaultProperties();
}

//------------------------------------------------------------------------------
// RenderState
//------------------------------------------------------------------------------

RenderState::RenderState(const std::string &fileName) {
	renderState = slg::RenderState::LoadSerialized(fileName);
}

RenderState::RenderState(slg::RenderState *state) {
	renderState = state;
}

RenderState::~RenderState() {
	delete renderState;
}

void RenderState::Save(const std::string &fileName) const {
	renderState->SaveSerialized(fileName);
}

//------------------------------------------------------------------------------
// RenderSession
//------------------------------------------------------------------------------

RenderSession::RenderSession(const RenderConfig *config, RenderState *startState, Film *startFilm) :
		renderConfig(config), film(*this) {
	renderSession = new slg::RenderSession(config->renderConfig,
			startState ? startState->renderState : NULL,
			startFilm ? startFilm->standAloneFilm : NULL);

	if (startState) {
		// slg::RenderSession will take care of deleting startState->renderState
		startState->renderState = NULL;
		delete startState;
	}

	if (startFilm) {
		// slg::RenderSession will take care of deleting startFilm->standAloneFilm too
		startFilm->standAloneFilm = NULL;
		delete startFilm;
	}
}

RenderSession::RenderSession(const RenderConfig *config, const std::string &startStateFileName,
		const std::string &startFilmFileName) :
		renderConfig(config), film(*this) {
	auto_ptr<slg::Film> startFilm(slg::Film::LoadSerialized(startFilmFileName));
	auto_ptr<slg::RenderState> startState(slg::RenderState::LoadSerialized(startStateFileName));

	renderSession = new slg::RenderSession(config->renderConfig,
			startState.release(), startFilm.release());
}

RenderSession::~RenderSession() {
	delete renderSession;
}
const RenderConfig &RenderSession::GetRenderConfig() const {
	return *renderConfig;
}

RenderState *RenderSession::GetRenderState() {
	return new RenderState(renderSession->GetRenderState());
}

void RenderSession::Start() {
	renderSession->Start();

	// In order to populate the stats.* Properties
	UpdateStats();
}

void RenderSession::Stop() {
	renderSession->Stop();
}

bool RenderSession::IsStarted() const {
	return renderSession->IsStarted();
}

void RenderSession::BeginSceneEdit() {
	renderSession->BeginSceneEdit();
}

void RenderSession::EndSceneEdit() {
	renderSession->EndSceneEdit();
}

bool RenderSession::IsInSceneEdit() const {
	return renderSession->IsInSceneEdit();
}

void RenderSession::Pause() {
	renderSession->Pause();
}

void RenderSession::Resume() {
	renderSession->Resume();
}

bool RenderSession::IsInPause() const {
	return renderSession->IsInPause();
}

bool RenderSession::HasDone() const {
	return renderSession->renderEngine->HasDone();
}

void RenderSession::WaitForDone() const {
	renderSession->renderEngine->WaitForDone();
}

void RenderSession::WaitNewFrame() {
	renderSession->renderEngine->WaitNewFrame();
}

bool RenderSession::NeedPeriodicFilmSave() {
	return renderSession->NeedPeriodicFilmSave();
}

Film &RenderSession::GetFilm() {
	return film;
}

static void SetTileProperties(Properties &props, const string &prefix,
		const deque<const slg::TileRepository::Tile *> &tiles) {
	props.Set(Property(prefix + ".count")((u_int)tiles.size()));
	Property tileCoordProp(prefix + ".coords");
	Property tilePassProp(prefix + ".pass");
	Property tileErrorProp(prefix + ".error");

	BOOST_FOREACH(const slg::TileRepository::Tile *tile, tiles) {
		tileCoordProp.Add(tile->xStart).Add(tile->yStart);
		tilePassProp.Add(tile->pass);
		tileErrorProp.Add(tile->error);
	}

	props.Set(tileCoordProp);
	props.Set(tilePassProp);
	props.Set(tileErrorProp);
}

void RenderSession::UpdateStats() {
	// Film update may be required by some render engine to
	// update statistics, convergence test and more
	renderSession->renderEngine->UpdateFilm();

	stats.Set(Property("stats.renderengine.total.raysec")(renderSession->renderEngine->GetTotalRaysSec()));
	stats.Set(Property("stats.renderengine.total.samplesec")(renderSession->renderEngine->GetTotalSamplesSec()));
	stats.Set(Property("stats.renderengine.total.samplecount")(renderSession->renderEngine->GetTotalSampleCount()));
	stats.Set(Property("stats.renderengine.pass")(renderSession->renderEngine->GetPass()));
	stats.Set(Property("stats.renderengine.time")(renderSession->renderEngine->GetRenderingTime()));
	stats.Set(Property("stats.renderengine.convergence")(renderSession->renderEngine->GetConvergence()));
	
	// Intersection devices statistics
	const vector<IntersectionDevice *> &idevices = renderSession->renderEngine->GetIntersectionDevices();

	// Replace all virtual devices with real
	vector<IntersectionDevice *> realDevices;
	for (size_t i = 0; i < idevices.size(); ++i) {
		VirtualIntersectionDevice *vdev = dynamic_cast<VirtualIntersectionDevice *>(idevices[i]);
		if (vdev) {
			const vector<IntersectionDevice *> &realDevs = vdev->GetRealDevices();
			realDevices.insert(realDevices.end(), realDevs.begin(), realDevs.end());
		} else
			realDevices.push_back(idevices[i]);
	}

	boost::unordered_map<string, u_int> devCounters;
	Property devicesNames("stats.renderengine.devices");
	double totalPerf = 0.0;
	BOOST_FOREACH(IntersectionDevice *dev, realDevices) {
		const string &devName = dev->GetName();

		// Append a device index for the case where the same device is used multiple times
		u_int index = devCounters[devName]++;
		const string uniqueName = devName + "-" + ToString(index);
		devicesNames.Add(uniqueName);

		const string prefix = "stats.renderengine.devices." + uniqueName;
		totalPerf += dev->GetTotalPerformance();
		stats.Set(Property(prefix + ".performance.total")(dev->GetTotalPerformance()));
		stats.Set(Property(prefix + ".performance.serial")(dev->GetSerialPerformance()));
		stats.Set(Property(prefix + ".performance.dataparallel")(dev->GetDataParallelPerformance()));
		stats.Set(Property(prefix + ".memory.total")((u_longlong)dev->GetMaxMemory()));
		stats.Set(Property(prefix + ".memory.used")((u_longlong)dev->GetUsedMemory()));
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
		case slg::RTBIASPATHOCL: {
			slg::RTBiasPathOCLRenderEngine *engine = (slg::RTBiasPathOCLRenderEngine *)renderSession->renderEngine;
			stats.Set(Property("stats.rtbiaspathocl.frame.time")(engine->GetFrameTime()));
			break;
		}
		case slg::BIASPATHOCL: {
			slg::BiasPathOCLRenderEngine *engine = (slg::BiasPathOCLRenderEngine *)renderSession->renderEngine;
			
			stats.Set(Property("stats.biaspath.tiles.size.x")(engine->GetTileWidth()));
			stats.Set(Property("stats.biaspath.tiles.size.y")(engine->GetTileHeight()));

			// Pending tiles
			{
				deque<const slg::TileRepository::Tile *> tiles;
				engine->GetPendingTiles(tiles);
				SetTileProperties(stats, "stats.biaspath.tiles.pending", tiles);
			}

			// Not converged tiles
			{
				deque<const slg::TileRepository::Tile *> tiles;
				engine->GetNotConvergedTiles(tiles);
				SetTileProperties(stats, "stats.biaspath.tiles.notconverged", tiles);
			}

			// Converged tiles
			{
				deque<const slg::TileRepository::Tile *> tiles;
				engine->GetConvergedTiles(tiles);
				SetTileProperties(stats, "stats.biaspath.tiles.converged", tiles);
			}
			break;
		}
#endif
		case slg::BIASPATHCPU: {
			slg::CPUTileRenderEngine *engine = (slg::CPUTileRenderEngine *)renderSession->renderEngine;

			stats.Set(Property("stats.biaspath.tiles.size.x")(engine->GetTileWidth()));
			stats.Set(Property("stats.biaspath.tiles.size.y")(engine->GetTileHeight()));

			// Pending tiles
			{
				deque<const slg::TileRepository::Tile *> tiles;
				engine->GetPendingTiles(tiles);
				SetTileProperties(stats, "stats.biaspath.tiles.pending", tiles);
			}

			// Not converged tiles
			{
				deque<const slg::TileRepository::Tile *> tiles;
				engine->GetNotConvergedTiles(tiles);
				SetTileProperties(stats, "stats.biaspath.tiles.notconverged", tiles);
			}

			// Converged tiles
			{
				deque<const slg::TileRepository::Tile *> tiles;
				engine->GetConvergedTiles(tiles);
				SetTileProperties(stats, "stats.biaspath.tiles.converged", tiles);
			}
			break;
		}
		default:
			break;
	}
}

const Properties &RenderSession::GetStats() const {
	return stats;
}

void RenderSession::Parse(const Properties &props) {
	renderSession->Parse(props);
}
