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

void luxcore::Init() {
	// Initialize FreeImage Library
	FreeImage_Initialise(TRUE);
	FreeImage_SetOutputMessage(FreeImageErrorHandler);
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

void Scene::Parse(const luxrays::Properties &props) {
	scene->Parse(props);
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

void RenderConfig::Parse(const luxrays::Properties &props) {
	renderConfig->Parse(props);
}

Scene &RenderConfig::GetScene() {
	return *scene;
}

//------------------------------------------------------------------------------
// RenderSession
//------------------------------------------------------------------------------

RenderSession::RenderSession(const RenderConfig *config) {
	renderSession = new slg::RenderSession(config->renderConfig);
}

RenderSession::~RenderSession() {
	delete renderSession;
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

void RenderSession::SaveFilm() {
	renderSession->SaveFilm();
}

vector<unsigned char> RenderSession::GetScreenBuffer() {
	slg::Film *film = renderSession->film;

	film->UpdateScreenBuffer();
	const float *src = film->GetScreenBuffer();

	const u_int count = film->GetWidth() * film->GetHeight() * 3;
	vector<unsigned char> dst(count);
	for (u_int i = 0; i < count; ++i)
		dst[i] = (unsigned char)floor((src[i] * 255.f + .5f));

	return dst;
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
