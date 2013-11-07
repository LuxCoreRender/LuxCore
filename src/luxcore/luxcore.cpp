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

#include <boost/thread/once.hpp>
#include <boost/thread/mutex.hpp>

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/virtualdevice.h"
#include "slg/slg.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/engines/biaspathocl/biaspathocl.h"
#include "luxcore/luxcore.h"
#include "slg/engines/rtbiaspathocl/rtbiaspathocl.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// Initialization and logging
//------------------------------------------------------------------------------

void (*luxcore::LuxCore_LogHandler)(const char *msg) = NULL;

static void DefaultDebugHandler(const char *msg) {
	cerr << msg << endl;
}

static void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	if (LuxCore_LogHandler) {
		stringstream ss;
		ss << "[FreeImage] ";
		if (fif != FIF_UNKNOWN)
			ss << FreeImage_GetFormatFromFIF(fif) << " Format: ";
		ss << message;
		LuxCore_LogHandler(ss.str().c_str());
	}
}

static void LuxRaysDebugHandler(const char *msg) {
	if (LuxCore_LogHandler) {
		stringstream ss;
		ss << "[LuxRays] " << msg;
		LuxCore_LogHandler(ss.str().c_str());
	}
}

static void SDLDebugHandler(const char *msg) {
	if (LuxCore_LogHandler) {
		stringstream ss;
		ss << "[SDL] " << msg;
		LuxCore_LogHandler(ss.str().c_str());
	}
}

static void SLGDebugHandler(const char *msg) {
	if (LuxCore_LogHandler) {
		stringstream ss;
		ss << "[LuxCore] " << msg;
		LuxCore_LogHandler(ss.str().c_str());
	}
}

static void InitFreeImage() {
	// Initialize FreeImage Library
	FreeImage_Initialise(TRUE);
}

void luxcore::Init(void (*LogHandler)(const char *)) {
	static boost::once_flag initFreeImageOnce = BOOST_ONCE_INIT;

	// Run the FreeImage initialization only once
	boost::call_once(initFreeImageOnce, &InitFreeImage);

	// To be thread safe
	static boost::mutex initMutex;
	boost::unique_lock<boost::mutex> lock(initMutex);
	
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
	FreeImage_SetOutputMessage(FreeImageErrorHandler);
}

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

extern FILE *yyin;
extern int yyparse(void);
extern void yyrestart( FILE *new_file );
extern void IncludeClear();
extern string currentFile;
extern u_int lineNum;

namespace luxcore { namespace parselxs {
Properties *renderConfigProps = NULL;
Properties *sceneProps = NULL;
} }

void luxcore::ParseLXS(const string &fileName, Properties &renderConfigProps, Properties &sceneProps) {
	// Otherwise the code is not thread-safe
	static boost::mutex parseLXSMutex;
	boost::unique_lock<boost::mutex> lock(parseLXSMutex);

	luxcore::parselxs::renderConfigProps = &renderConfigProps;
	luxcore::parselxs::sceneProps = &sceneProps;

	bool parseSuccess = false;

	if (fileName == "-")
		yyin = stdin;
	else
		yyin = fopen(fileName.c_str(), "r");

	if (yyin != NULL) {
		currentFile = fileName;
		if (yyin == stdin)
			currentFile = "<standard input>";
		lineNum = 1;
		// Make sure to flush any buffers before parsing
		IncludeClear();
		yyrestart(yyin);
		try {
			parseSuccess = (yyparse() == 0);
		} catch (std::runtime_error& e) {
			throw runtime_error("Exception during parsing (file '" + currentFile + "', line: " + ToString(lineNum) + "): " + e.what());
		}
		
		if (yyin != stdin)
			fclose(yyin);
	} else
		throw runtime_error("Unable to read scene file: " + fileName);

	currentFile = "";
	lineNum = 0;

	if ((yyin == NULL) || !parseSuccess)
		throw runtime_error("Parsing failed: " + fileName);
}

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

Film::Film(const RenderSession &session) : renderSession(session) {
}

Film::~Film() {
}

u_int Film::GetWidth() const {
	return renderSession.renderSession->film->GetWidth();
}

u_int Film::GetHeight() const {
	return renderSession.renderSession->film->GetHeight();
}

void Film::Save() const {
	renderSession.renderSession->SaveFilm();
}

double Film::GetTotalSampleCount() const {
	return renderSession.renderSession->film->GetTotalSampleCount(); 
}

bool Film::HasOutput(const FilmOutputType type) const {
	return renderSession.renderSession->film->HasOutput((slg::FilmOutputs::FilmOutputType)type);
}

size_t Film::GetOutputSize(const FilmOutputType type) const {
	const u_int pixelCount = renderSession.renderSession->film->GetWidth() * renderSession.renderSession->film->GetHeight();
	switch (type) {
		case OUTPUT_RGB:
			return 3 * pixelCount;
		case OUTPUT_RGBA:
			return 3 * pixelCount;
		case OUTPUT_RGB_TONEMAPPED:
			return 3 * pixelCount;
		case OUTPUT_RGBA_TONEMAPPED:
			return 4 * pixelCount;
		case OUTPUT_ALPHA:
			return pixelCount;
		case OUTPUT_DEPTH:
			return pixelCount;
		case OUTPUT_POSITION:
			return 3 * pixelCount;
		case OUTPUT_GEOMETRY_NORMAL:
			return 3 * pixelCount;
		case OUTPUT_SHADING_NORMAL:
			return 3 * pixelCount;
		case OUTPUT_MATERIAL_ID:
			return pixelCount;
		case OUTPUT_DIRECT_DIFFUSE:
			return 3 * pixelCount;
		case OUTPUT_DIRECT_GLOSSY:
			return 3 * pixelCount;
		case OUTPUT_EMISSION:
			return 3 * pixelCount;
		case OUTPUT_INDIRECT_DIFFUSE:
			return 3 * pixelCount;
		case OUTPUT_INDIRECT_GLOSSY:
			return 3 * pixelCount;
		case OUTPUT_INDIRECT_SPECULAR:
			return 3 * pixelCount;
		case OUTPUT_MATERIAL_ID_MASK:
			return pixelCount;
		case OUTPUT_DIRECT_SHADOW_MASK:
			return 3 * pixelCount;
		case OUTPUT_INDIRECT_SHADOW_MASK:
			return 3 * pixelCount;
		case OUTPUT_RADIANCE_GROUP:
			return 3 * pixelCount;
		case OUTPUT_UV:
			return 2 * pixelCount;
		case OUTPUT_RAYCOUNT:
			return pixelCount;
		default:
			throw runtime_error("Unknown FilmOutputType in Film::GetOutputSize(): " + ToString(type));
	}
}

u_int Film::GetRadianceGroupCount() const {
	return renderSession.renderSession->film->GetRadianceGroupCount();
}

template<> void Film::GetOutput<float>(const FilmOutputType type, float *buffer, const u_int index) const {
	renderSession.renderSession->film->GetOutput<float>((slg::FilmOutputs::FilmOutputType)type, buffer, index);
}

template<> void Film::GetOutput<u_int>(const FilmOutputType type, u_int *buffer, const u_int index) const {
	renderSession.renderSession->film->GetOutput<u_int>((slg::FilmOutputs::FilmOutputType)type, buffer, index);
}

u_int Film::GetChannelCount(const FilmChannelType type) const {
	switch (type) {
		case CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED:
			return renderSession.renderSession->film->channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size();
		case CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED:
			return renderSession.renderSession->film->channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size();
		case CHANNEL_ALPHA:
			return renderSession.renderSession->film->channel_ALPHA ? 1 : 0;
		case CHANNEL_RGB_TONEMAPPED:
			return renderSession.renderSession->film->channel_RGB_TONEMAPPED ? 1 : 0;
		case CHANNEL_DEPTH:
			return renderSession.renderSession->film->channel_DEPTH ? 1 : 0;
		case CHANNEL_POSITION:
			return renderSession.renderSession->film->channel_POSITION ? 1 : 0;
		case CHANNEL_GEOMETRY_NORMAL:
			return renderSession.renderSession->film->channel_GEOMETRY_NORMAL ? 1 : 0;
		case CHANNEL_SHADING_NORMAL:
			return renderSession.renderSession->film->channel_SHADING_NORMAL ? 1 : 0;
		case CHANNEL_MATERIAL_ID:
			return renderSession.renderSession->film->channel_MATERIAL_ID ? 1 : 0;
		case CHANNEL_DIRECT_DIFFUSE:
			return renderSession.renderSession->film->channel_DIRECT_DIFFUSE ? 1 : 0;
		case CHANNEL_DIRECT_GLOSSY:
			return renderSession.renderSession->film->channel_DIRECT_GLOSSY ? 1 : 0;
		case CHANNEL_EMISSION:
			return renderSession.renderSession->film->channel_EMISSION ? 1 : 0;
		case CHANNEL_INDIRECT_DIFFUSE:
			return renderSession.renderSession->film->channel_INDIRECT_DIFFUSE ? 1 : 0;
		case CHANNEL_INDIRECT_GLOSSY:
			return renderSession.renderSession->film->channel_INDIRECT_GLOSSY ? 1 : 0;
		case CHANNEL_INDIRECT_SPECULAR:
			return renderSession.renderSession->film->channel_INDIRECT_SPECULAR ? 1 : 0;
		case CHANNEL_MATERIAL_ID_MASK:
			return renderSession.renderSession->film->channel_MATERIAL_ID_MASKs.size();
		case CHANNEL_DIRECT_SHADOW_MASK:
			return renderSession.renderSession->film->channel_DIRECT_SHADOW_MASK ? 1 : 0;
		case CHANNEL_INDIRECT_SHADOW_MASK:
			return renderSession.renderSession->film->channel_INDIRECT_SHADOW_MASK ? 1 : 0;
		case CHANNEL_UV:
			return renderSession.renderSession->film->channel_UV ? 1 : 0;
		case CHANNEL_RAYCOUNT:
			return renderSession.renderSession->film->channel_RAYCOUNT ? 1 : 0;
		default:
			throw runtime_error("Unknown FilmOutputType in Film::GetChannelCount>(): " + ToString(type));
	}
}

template<> const float *Film::GetChannel<float>(const FilmChannelType type, const u_int index) const {
	switch (type) {
		case CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED:
			return renderSession.renderSession->film->channel_RADIANCE_PER_PIXEL_NORMALIZEDs[index]->GetPixels();
		case CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED:
			return renderSession.renderSession->film->channel_RADIANCE_PER_SCREEN_NORMALIZEDs[index]->GetPixels();
		case CHANNEL_ALPHA:
			return renderSession.renderSession->film->channel_ALPHA->GetPixels();
		case CHANNEL_RGB_TONEMAPPED:
			renderSession.renderSession->film->UpdateChannel_RGB_TONEMAPPED();
			return renderSession.renderSession->film->channel_RGB_TONEMAPPED->GetPixels();
		case CHANNEL_DEPTH:
			return renderSession.renderSession->film->channel_DEPTH->GetPixels();
		case CHANNEL_POSITION:
			return renderSession.renderSession->film->channel_POSITION->GetPixels();
		case CHANNEL_GEOMETRY_NORMAL:
			return renderSession.renderSession->film->channel_GEOMETRY_NORMAL->GetPixels();
		case CHANNEL_SHADING_NORMAL:
			return renderSession.renderSession->film->channel_SHADING_NORMAL->GetPixels();
		case CHANNEL_DIRECT_DIFFUSE:
			return renderSession.renderSession->film->channel_DIRECT_DIFFUSE->GetPixels();
		case CHANNEL_DIRECT_GLOSSY:
			return renderSession.renderSession->film->channel_DIRECT_GLOSSY->GetPixels();
		case CHANNEL_EMISSION:
			return renderSession.renderSession->film->channel_EMISSION->GetPixels();
		case CHANNEL_INDIRECT_DIFFUSE:
			return renderSession.renderSession->film->channel_INDIRECT_DIFFUSE->GetPixels();
		case CHANNEL_INDIRECT_GLOSSY:
			return renderSession.renderSession->film->channel_INDIRECT_GLOSSY->GetPixels();
		case CHANNEL_INDIRECT_SPECULAR:
			return renderSession.renderSession->film->channel_INDIRECT_SPECULAR->GetPixels();
		case CHANNEL_MATERIAL_ID_MASK:
			return renderSession.renderSession->film->channel_MATERIAL_ID_MASKs[index]->GetPixels();
		case CHANNEL_DIRECT_SHADOW_MASK:
			return renderSession.renderSession->film->channel_DIRECT_SHADOW_MASK->GetPixels();
		case CHANNEL_INDIRECT_SHADOW_MASK:
			return renderSession.renderSession->film->channel_INDIRECT_SHADOW_MASK->GetPixels();
		case CHANNEL_UV:
			return renderSession.renderSession->film->channel_UV->GetPixels();
		case CHANNEL_RAYCOUNT:
			return renderSession.renderSession->film->channel_RAYCOUNT->GetPixels();
		default:
			throw runtime_error("Unknown FilmOutputType in Film::GetChannel<float>(): " + ToString(type));
	}
}

template<> const u_int *Film::GetChannel<u_int>(const FilmChannelType type, const u_int index) const {
	switch (type) {
		case CHANNEL_MATERIAL_ID:
			return renderSession.renderSession->film->channel_MATERIAL_ID->GetPixels();
		default:
			throw runtime_error("Unknown FilmOutputType in Film::GetChannel<u_int>(): " + ToString(type));
	}
}

//------------------------------------------------------------------------------
// Camera
//------------------------------------------------------------------------------

Camera::Camera(const Scene &scn) : scene(scn) {
}

Camera::~Camera() {
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

const Properties &Scene::GetProperties() const {
	return scene->GetProperties();
}

const DataSet &Scene::GetDataSet() const {
	return *(scene->dataSet);
}

const Camera &Scene::GetCamera() const {
	return camera;
}

void Scene::DefineImageMap(const string &imgMapName, float *cols, const float gamma,
		const u_int channels, const u_int width, const u_int height) {
	scene->DefineImageMap(imgMapName, cols, gamma, channels, width, height);
}

bool Scene::IsImageMapDefined(const std::string &imgMapName) const {
	return scene->IsImageMapDefined(imgMapName);
}

void Scene::SetDeleteMeshData(const bool v) {
	scene->extMeshCache.SetDeleteMeshData(v);
}

void Scene::DefineMesh(const string &meshName, ExtTriangleMesh *mesh) {
	scene->DefineMesh(meshName, mesh);
}

void Scene::DefineMesh(const string &meshName,
	const long plyNbVerts, const long plyNbTris,
	Point *p, Triangle *vi, Normal *n, UV *uv,
	Spectrum *cols, float *alphas) {
	scene->DefineMesh(meshName, plyNbVerts, plyNbTris, p, vi, n, uv, cols, alphas);
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
	return scene->GetLightCount();
}

const u_int  Scene::GetObjectCount() const {
	return scene->GetObjectCount();
}

void Scene::Parse(const Properties &props) {
	scene->Parse(props);
}

void Scene::DeleteObject(const string &objName) {
	scene->DeleteObject(objName);
}

void Scene::RemoveUnusedImageMaps() {
	scene->RemoveUnusedImageMaps();
}

void Scene::RemoveUnusedTextures() {
	scene->RemoveUnusedTextures();
}

void Scene::RemoveUnusedMaterials() {
	scene->RemoveUnusedMaterials();
}

void Scene::RemoveUnusedMeshes() {
	scene->RemoveUnusedMeshes();
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

Scene &RenderConfig::GetScene() {
	return *scene;
}

void RenderConfig::Parse(const Properties &props) {
	renderConfig->Parse(props);
}

void RenderConfig::Delete(const string prefix) {
	renderConfig->Delete(prefix);
}

bool RenderConfig::GetFilmSize(u_int *filmFullWidth, u_int *filmFullHeight,
		u_int *filmSubRegion) const {
	return renderConfig->GetFilmSize(filmFullWidth, filmFullHeight, filmSubRegion);
}

const Properties &RenderConfig::GetDefaultProperties() {
	return slg::RenderConfig::GetDefaultProperties();
}

//------------------------------------------------------------------------------
// RenderSession
//------------------------------------------------------------------------------

RenderSession::RenderSession(const RenderConfig *config) : renderConfig(config), film(*this) {
	renderSession = new slg::RenderSession(config->renderConfig);
}

RenderSession::~RenderSession() {
	delete renderSession;
}

const RenderConfig &RenderSession::GetRenderConfig() const {
	return *renderConfig;
}

void RenderSession::Start() {
	renderSession->Start();
}

void RenderSession::Stop() {
	renderSession->Stop();
}

void RenderSession::BeginSceneEdit() {
	renderSession->BeginSceneEdit();
}

void RenderSession::EndSceneEdit() {
	renderSession->EndSceneEdit();
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
		stats.Set(Property(prefix + ".memory.total")(dev->GetMaxMemory()));
		stats.Set(Property(prefix + ".memory.used")(dev->GetUsedMemory()));
	}
	stats.Set(devicesNames);
	stats.Set(Property("stats.renderengine.performance.total")(totalPerf));

	// The explicit cast to size_t is required by VisualC++
	stats.Set(Property("stats.dataset.trianglecount")((size_t)renderSession->renderConfig->scene->dataSet->GetTotalTriangleCount()));

	// Some engine specific statistic
	switch (renderSession->renderEngine->GetEngineType()) {
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
			
			stats.Set(Property("stats.biaspath.tiles.size")(engine->GetTileSize()));
			vector<slg::TileRepository::Tile> tiles;
			engine->GetPendingTiles(tiles);
			stats.Set(Property("stats.biaspath.tiles.pending.count")(tiles.size()));
			Property tileProp("stats.biaspath.tiles.pending.coords");
			BOOST_FOREACH(const slg::TileRepository::Tile &tile, tiles)
				tileProp.Add(tile.xStart).Add(tile.yStart);
			stats.Set(tileProp);
			break;
		}
#endif
		case slg::BIASPATHCPU: {
			slg::CPUTileRenderEngine *engine = (slg::CPUTileRenderEngine *)renderSession->renderEngine;

			stats.Set(Property("stats.biaspath.tiles.size")(engine->GetTileSize()));
			vector<slg::TileRepository::Tile> tiles;
			engine->GetPendingTiles(tiles);
			stats.Set(Property("stats.biaspath.tiles.pending.count")(tiles.size()));
			Property tileProp("stats.biaspath.tiles.pending.coords");
			BOOST_FOREACH(const slg::TileRepository::Tile &tile, tiles)
				tileProp.Add(tile.xStart).Add(tile.yStart);
			stats.Set(tileProp);
			break;
		}
		default:
			break;
	}
}

const Properties &RenderSession::GetStats() const {
	return stats;
}
