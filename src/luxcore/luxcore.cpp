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

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/core/virtualdevice.h"
#include "slg/slg.h"
#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

static void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
	printf("\n*** ");
	if(fif != FIF_UNKNOWN)
		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));

	printf("%s", message);
	printf(" ***\n");
}

static void LuxRaysDebugHandler(const char *msg) {
	cerr << "[LuxRays] " << msg << endl;
}

static void SDLDebugHandler(const char *msg) {
	cerr << "[SDL] " << msg << endl;
}

static void SLGDebugHandler(const char *msg) {
	cerr << "[LuxCore] " << msg << endl;
}

void luxcore::Init() {
	slg::LuxRays_DebugHandler = ::LuxRaysDebugHandler;
	slg::SLG_DebugHandler = ::SLGDebugHandler;
	slg::SLG_SDLDebugHandler = ::SDLDebugHandler;

	// Initialize FreeImage Library
	FreeImage_Initialise(TRUE);
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

template<> void Film::GetOutput<float>(const FilmOutputType type, float *buffer, const u_int index) const {
//	switch (type) {
//		case RGB:
//			break;

//		RGBA, RGB_TONEMAPPED, RGBA_TONEMAPPED, ALPHA, DEPTH, POSITION,
//		GEOMETRY_NORMAL, SHADING_NORMAL, MATERIAL_ID, DIRECT_DIFFUSE,
//		DIRECT_GLOSSY, EMISSION, INDIRECT_DIFFUSE, INDIRECT_GLOSSY,
//		INDIRECT_SPECULAR, MATERIAL_ID_MASK, DIRECT_SHADOW_MASK, INDIRECT_SHADOW_MASK,
//		RADIANCE_GROUP, UV, RAYCOUNT		
//	}
}

template<> void Film::GetOutput<u_int>(const FilmOutputType type, u_int *buffer, const u_int index) const {
	
}

const float *Film::GetScreenBuffer() {
	slg::Film *film = renderSession.renderSession->film;

	film->UpdateScreenBuffer();
	return film->GetScreenBuffer();
}

//------------------------------------------------------------------------------
// Scene
//------------------------------------------------------------------------------

Scene::Scene(const float imageScale) {
	scene = new slg::Scene(imageScale);
	allocatedScene = true;
}

Scene::Scene(const string &fileName, const float imageScale) {
	scene = new slg::Scene(fileName, imageScale);
	allocatedScene = true;
}

Scene::Scene(slg::Scene *scn) {
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

void Scene::SetDeleteMeshData(const bool v) {
	scene->extMeshCache.SetDeleteMeshData(v);
}

void Scene::DefineMesh(const std::string &meshName, luxrays::ExtTriangleMesh *mesh) {
	scene->DefineMesh(meshName, mesh);
}

void Scene::DefineMesh(const std::string &meshName,
	const long plyNbVerts, const long plyNbTris,
	luxrays::Point *p, luxrays::Triangle *vi, luxrays::Normal *n, luxrays::UV *uv,
	luxrays::Spectrum *cols, float *alphas) {
	scene->DefineMesh(meshName, plyNbVerts, plyNbTris, p, vi, n, uv, cols, alphas, true);
}

void Scene::Parse(const luxrays::Properties &props) {
	scene->Parse(props);
}

void Scene::DeleteObject(const std::string &objName) {
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

RenderConfig::RenderConfig(const luxrays::Properties &props, Scene *scn) {
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

const luxrays::Properties &RenderConfig::GetProperties() const {
	return renderConfig->cfg;
}

Scene &RenderConfig::GetScene() {
	return *scene;
}

void RenderConfig::Parse(const luxrays::Properties &props) {
	renderConfig->Parse(props);
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
	BOOST_FOREACH(IntersectionDevice *dev, realDevices) {
		const string &devName = dev->GetName();

		// Append a device index for the case where the same device is used multiple times
		u_int index = devCounters[devName]++;
		const string uniqueName = devName + "-" + ToString(index);
		devicesNames.Add(uniqueName);

		const string prefix = "stats.renderengine.devices." + uniqueName;
		stats.Set(Property(prefix + ".performance.total")(dev->GetTotalPerformance()));
		stats.Set(Property(prefix + ".performance.serial")(dev->GetSerialPerformance()));
		stats.Set(Property(prefix + ".performance.dataparallel")(dev->GetDataParallelPerformance()));
		stats.Set(Property(prefix + ".memory.total")(dev->GetMaxMemory()));
		stats.Set(Property(prefix + ".memory.used")(dev->GetUsedMemory()));
	}
	stats.Set(devicesNames);

	stats.Set(Property("stats.dataset.trianglecount")(renderSession->renderConfig->scene->dataSet->GetTotalTriangleCount()));
}

const Properties &RenderSession::GetStats() const {
	return stats;
}
