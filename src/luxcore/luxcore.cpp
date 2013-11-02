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

size_t Film::GetOutputSize(const FilmOutputType type) const {
	const u_int pixelCount = renderSession.renderSession->film->GetWidth() * renderSession.renderSession->film->GetHeight();
	switch (type) {
		case RGB:
			return 3 * pixelCount;
		case RGBA:
			return 3 * pixelCount;
		case RGB_TONEMAPPED:
			return 3 * pixelCount;
		case RGBA_TONEMAPPED:
			return 4 * pixelCount;
		case ALPHA:
			return pixelCount;
		case DEPTH:
			return pixelCount;
		case POSITION:
			return 3 * pixelCount;
		case GEOMETRY_NORMAL:
			return 3 * pixelCount;
		case SHADING_NORMAL:
			return 3 * pixelCount;
		case DIRECT_DIFFUSE:
			return 3 * pixelCount;
		case DIRECT_GLOSSY:
			return 3 * pixelCount;
		case EMISSION:
			return 3 * pixelCount;
		case INDIRECT_DIFFUSE:
			return 3 * pixelCount;
		case INDIRECT_GLOSSY:
			return 3 * pixelCount;
		case INDIRECT_SPECULAR:
			return 3 * pixelCount;
		case MATERIAL_ID_MASK:
			return pixelCount;
		case DIRECT_SHADOW_MASK:
			return 3 * pixelCount;
		case INDIRECT_SHADOW_MASK:
			return 3 * pixelCount;
		case RADIANCE_GROUP:
			return 3 * pixelCount;
		case UV:
			return 2 * pixelCount;
		case RAYCOUNT:
			return pixelCount;
		default:
			throw runtime_error("Unknown FilmOutputType in Film::GetOutputSize()" + ToString(type));
	}
}

template<> void Film::GetOutput<float>(const FilmOutputType type, float *buffer, const u_int index) const {
	switch (type) {
		case RGB:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::RGB, buffer, index);
			break;
		case RGBA:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::RGBA, buffer, index);
			break;
		case RGB_TONEMAPPED:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::RGB_TONEMAPPED, buffer, index);
			break;
		case RGBA_TONEMAPPED:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::RGBA_TONEMAPPED, buffer, index);
			break;
		case ALPHA:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::ALPHA, buffer, index);
			break;
		case DEPTH:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::DEPTH, buffer, index);
			break;
		case POSITION:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::POSITION, buffer, index);
			break;
		case GEOMETRY_NORMAL:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::GEOMETRY_NORMAL, buffer, index);
			break;
		case SHADING_NORMAL:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::SHADING_NORMAL, buffer, index);
			break;
		case DIRECT_DIFFUSE:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::DIRECT_DIFFUSE, buffer, index);
			break;
		case DIRECT_GLOSSY:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::DIRECT_GLOSSY, buffer, index);
			break;
		case EMISSION:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::EMISSION, buffer, index);
			break;
		case INDIRECT_DIFFUSE:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::INDIRECT_DIFFUSE, buffer, index);
			break;
		case INDIRECT_GLOSSY:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::INDIRECT_GLOSSY, buffer, index);
			break;
		case INDIRECT_SPECULAR:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::INDIRECT_SPECULAR, buffer, index);
			break;
		case MATERIAL_ID_MASK:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::MATERIAL_ID_MASK, buffer, index);
			break;
		case DIRECT_SHADOW_MASK:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::DIRECT_SHADOW_MASK, buffer, index);
			break;
		case INDIRECT_SHADOW_MASK:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::INDIRECT_SHADOW_MASK, buffer, index);
			break;
		case RADIANCE_GROUP:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::RADIANCE_GROUP, buffer, index);
			break;
		case UV:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::UV, buffer, index);
			break;
		case RAYCOUNT:
			renderSession.renderSession->film->GetOutput<float>(slg::FilmOutputs::RAYCOUNT, buffer, index);
			break;
		default:
			throw runtime_error("Unknown FilmOutputType in Film::GetOutput<float>()" + ToString(type));
	}
}

template<> void Film::GetOutput<u_int>(const FilmOutputType type, u_int *buffer, const u_int index) const {
	switch (type) {
		case MATERIAL_ID:
			renderSession.renderSession->film->GetOutput<u_int>(slg::FilmOutputs::MATERIAL_ID, buffer, index);
			break;
		default:
			throw runtime_error("Unknown FilmOutputType in Film::GetOutput<u_int>()" + ToString(type));
	}
}

const float *Film::GetRGBToneMappedOutput() const {
	renderSession.renderSession->film->UpdateChannel_RGB_TONEMAPPED();

	return renderSession.renderSession->film->channel_RGB_TONEMAPPED->GetPixels();
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

//------------------------------------------------------------------------------
// Utility
//------------------------------------------------------------------------------

namespace luxcore {

RenderEngineType String2RenderEngineType(const string &type) {
	return slg::RenderEngine::String2RenderEngineType(type);
}

const string RenderEngineType2String(const RenderEngineType type) {
	return slg::RenderEngine::RenderEngineType2String(type);
}

SamplerType String2SamplerType(const string &type) {
	return slg::Sampler::String2SamplerType(type);
}

const string SamplerType2String(const SamplerType type) {
	return slg::Sampler::SamplerType2String(type);
}

ToneMapType String2ToneMapType(const string &type) {
	return slg::String2ToneMapType(type);
}

const string ToneMapType2String(const ToneMapType type) {
	return slg::ToneMapType2String(type);
}

}
