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
extern unsigned int lineNum;

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

Film *Film::Create(const std::string &fileName) {
	return new FilmImpl(fileName);
}

Film *Film::Create(const RenderSession &session) {
	return new FilmImpl(session);
}

Film::~Film() {
}

template<> void Film::GetOutput<float>(const FilmOutputType type, float *buffer, const unsigned int index) {
	GetOutputFloat(type, buffer, index);
}

template<> void Film::GetOutput<unsigned int>(const FilmOutputType type, unsigned int *buffer, const unsigned int index) {
	GetOutputUInt(type, buffer, index);
}

template<> const float *Film::GetChannel<float>(const FilmChannelType type, const unsigned int index) {
	return GetChannelFloat(type, index);
}

template<> const unsigned int *Film::GetChannel(const FilmChannelType type, const unsigned int index) {
	return GetChannelUInt(type, index);
}

//------------------------------------------------------------------------------
// Camera
//------------------------------------------------------------------------------

Camera::~Camera() {
}

//------------------------------------------------------------------------------
// Scene
//------------------------------------------------------------------------------

Scene *Scene::Create(const float imageScale) {
	return new SceneImpl(imageScale);
}

Scene *Scene::Create(const string &fileName, const float imageScale) {
	return new SceneImpl(fileName, imageScale);
}

Scene::~Scene() {
}

template<> void Scene::DefineImageMap<unsigned char>(const std::string &imgMapName,
		unsigned char *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		Scene::ChannelSelectionType selectionType) {
	DefineImageMapUChar(imgMapName, pixels, gamma, channels, width, height, selectionType);
}

template<> void Scene::DefineImageMap<unsigned short>(const std::string &imgMapName,
		unsigned short *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		Scene::ChannelSelectionType selectionType) {
	DefineImageMapHalf(imgMapName, pixels, gamma, channels, width, height, selectionType);
}

template<> void Scene::DefineImageMap<float>(const std::string &imgMapName,
		float *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		Scene::ChannelSelectionType selectionType) {
	DefineImageMapFloat(imgMapName, pixels, gamma, channels, width, height, selectionType);
}

float *Scene::AllocVerticesBuffer(const unsigned int meshVertCount) {
	return (float *)SceneImpl::AllocVerticesBuffer(meshVertCount);
}

unsigned int *Scene::AllocTrianglesBuffer(const unsigned int meshTriCount) {
	return (unsigned int *)SceneImpl::AllocTrianglesBuffer(meshTriCount);
}

//------------------------------------------------------------------------------
// RenderConfig
//------------------------------------------------------------------------------

RenderConfig *RenderConfig::Create(const Properties &props, Scene *scn) {
	return new RenderConfigImpl(props, dynamic_cast<SceneImpl *>(scn));
}

RenderConfig::~RenderConfig() {
}

const Properties &RenderConfig::GetDefaultProperties() {
	return RenderConfigImpl::GetDefaultProperties();
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
		renderConfig(config) {
	film = dynamic_cast<FilmImpl *>(Film::Create(*this));

	FilmImpl *startFilmImpl = dynamic_cast<FilmImpl *>(startFilm);
	const RenderConfigImpl *configImpl = dynamic_cast<const RenderConfigImpl *>(config);
	renderSession = new slg::RenderSession(configImpl->renderConfig,
			startState ? startState->renderState : NULL,
			startFilm ? startFilmImpl->standAloneFilm : NULL);

	if (startState) {
		// slg::RenderSession will take care of deleting startState->renderState
		startState->renderState = NULL;
		delete startState;
	}

	if (startFilm) {
		// slg::RenderSession will take care of deleting startFilm->standAloneFilm too
		startFilmImpl->standAloneFilm = NULL;
		delete startFilmImpl;
	}
}

RenderSession::RenderSession(const RenderConfig *config, const std::string &startStateFileName,
		const std::string &startFilmFileName) :
		renderConfig(config) {
	film = dynamic_cast<FilmImpl *>(Film::Create(*this));

	auto_ptr<slg::Film> startFilm(slg::Film::LoadSerialized(startFilmFileName));
	auto_ptr<slg::RenderState> startState(slg::RenderState::LoadSerialized(startStateFileName));

	const RenderConfigImpl *configImpl = dynamic_cast<const RenderConfigImpl *>(config);
	renderSession = new slg::RenderSession(configImpl->renderConfig,
			startState.release(), startFilm.release());
}

RenderSession::~RenderSession() {
	delete film;
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
	return *film;
}

static void SetTileProperties(Properties &props, const string &prefix,
		const deque<const slg::TileRepository::Tile *> &tiles) {
	props.Set(Property(prefix + ".count")((unsigned int)tiles.size()));
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

	boost::unordered_map<string, unsigned int> devCounters;
	Property devicesNames("stats.renderengine.devices");
	double totalPerf = 0.0;
	BOOST_FOREACH(IntersectionDevice *dev, realDevices) {
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
		case slg::TILEPATHOCL: {
			slg::TilePathOCLRenderEngine *engine = (slg::TilePathOCLRenderEngine *)renderSession->renderEngine;
			
			stats.Set(Property("stats.tilepath.tiles.size.x")(engine->GetTileWidth()));
			stats.Set(Property("stats.tilepath.tiles.size.y")(engine->GetTileHeight()));

			// Pending tiles
			{
				deque<const slg::TileRepository::Tile *> tiles;
				engine->GetPendingTiles(tiles);
				SetTileProperties(stats, "stats.tilepath.tiles.pending", tiles);
			}

			// Not converged tiles
			{
				deque<const slg::TileRepository::Tile *> tiles;
				engine->GetNotConvergedTiles(tiles);
				SetTileProperties(stats, "stats.tilepath.tiles.notconverged", tiles);
			}

			// Converged tiles
			{
				deque<const slg::TileRepository::Tile *> tiles;
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
				deque<const slg::TileRepository::Tile *> tiles;
				engine->GetPendingTiles(tiles);
				SetTileProperties(stats, "stats.tilepath.tiles.pending", tiles);
			}

			// Not converged tiles
			{
				deque<const slg::TileRepository::Tile *> tiles;
				engine->GetNotConvergedTiles(tiles);
				SetTileProperties(stats, "stats.tilepath.tiles.notconverged", tiles);
			}

			// Converged tiles
			{
				deque<const slg::TileRepository::Tile *> tiles;
				engine->GetConvergedTiles(tiles);
				SetTileProperties(stats, "stats.tilepath.tiles.converged", tiles);
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
