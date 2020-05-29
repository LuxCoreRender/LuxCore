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
#include "luxrays/utils/utils.h"
#include "slg/slg.h"
#include "slg/engines/tilerepository.h"
#include "slg/engines/cpurenderengine.h"
#include "slg/engines/oclrenderengine.h"
#include "slg/engines/tilepathocl/tilepathocl.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/utils/filenameresolver.h"
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


static void NopDebugHandler(const char *msg) {
}

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
	slg::Init();

	// To be thread safe
	static boost::mutex initMutex;
	boost::unique_lock<boost::mutex> lock(initMutex);
	
	lcInitTime = WallClockTime();
	
	slg::LuxRays_DebugHandler = ::LuxRaysDebugHandler;
	slg::SLG_DebugHandler = ::SLGDebugHandler;
	slg::SLG_SDLDebugHandler = ::SDLDebugHandler;	

	if (LogHandler)
		SetLogHandler(LogHandler);
	else
		SetLogHandler(DefaultDebugHandler);
}

void luxcore::SetLogHandler(void (*LogHandler)(const char *)) {
	// Set all debug handlers
	if (LogHandler) {
		// User provided handler
		LuxCore_LogHandler = LogHandler;
	} else {
		// Default handler
		LuxCore_LogHandler = NopDebugHandler;
	}
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

	// For some debugging
	/*cout << "================ ParseLXS RenderConfig Properties ================\n";
	cout << renderConfigProps;
	cout << "================ ParseLXS RenderConfig Properties ================\n";
	cout << sceneProps;
	cout << "==================================================================\n";*/
}

//------------------------------------------------------------------------------
// GetPlatformDesc
//------------------------------------------------------------------------------

Properties luxcore::GetPlatformDesc() {
	Properties props;

	static const string luxCoreVersion(LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR);
	props << Property("version.number")(luxCoreVersion);

#if !defined(LUXRAYS_DISABLE_OPENCL)
	props << Property("compile.LUXRAYS_DISABLE_OPENCL")(false);
	props << Property("compile.LUXRAYS_ENABLE_OPENCL")(true);
#else
	props << Property("compile.LUXRAYS_DISABLE_OPENCL")(true);
	props << Property("compile.LUXRAYS_ENABLE_OPENCL")(false);
#endif

#if !defined(LUXRAYS_DISABLE_CUDA)
	props << Property("compile.LUXRAYS_DISABLE_CUDA")(!luxrays::isCudaAvilable);
	props << Property("compile.LUXRAYS_ENABLE_CUDA")(luxrays::isCudaAvilable);
#else
	props << Property("compile.LUXRAYS_DISABLE_CUDA")(true);
	props << Property("compile.LUXRAYS_ENABLE_CUDA")(false);
#endif

	props << Property("compile.LUXCORE_DISABLE_EMBREE_BVH_BUILDER")(false);
	props << Property("compile.LC_MESH_MAX_DATA_COUNT")(LC_MESH_MAX_DATA_COUNT);

	return props;
}

//------------------------------------------------------------------------------
// GetOpenCLDeviceDescs
//------------------------------------------------------------------------------

Properties luxcore::GetOpenCLDeviceDescs() {
#if defined(LUXRAYS_DISABLE_OPENCL)
	return Properties();
#else
	Properties props;

	Context ctx;
	vector<DeviceDescription *> deviceDescriptions = ctx.GetAvailableDeviceDescriptions();

	// Select only OpenCL devices
	DeviceDescription::Filter((DeviceType)(DEVICE_TYPE_OPENCL_ALL | DEVICE_TYPE_CUDA_ALL), deviceDescriptions);

	// Add all device information to the list
	for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
		DeviceDescription *desc = deviceDescriptions[i];

		string platformName = "UNKNOWN";
		string platformVersion = "UNKNOWN";
		int deviceClock = 0;
		unsigned long long deviceLocalMem = 0;
		unsigned long long deviceConstMem = 0;
		if (desc->GetType() & DEVICE_TYPE_OPENCL_ALL) {
			OpenCLDeviceDescription *oclDesc = (OpenCLDeviceDescription *)deviceDescriptions[i];

			platformName = oclDesc->GetOpenCLPlatform();
			platformVersion = oclDesc->GetOpenCLVersion();
			deviceClock = oclDesc->GetClock();
			deviceLocalMem = oclDesc->GetLocalMem();
			deviceConstMem = oclDesc->GetConstMem();
		} else if (desc->GetType() & DEVICE_TYPE_CUDA_ALL) {
			platformName = "NVIDIA";
		}

		const string prefix = "opencl.device." + ToString(i);
		props <<
				Property(prefix + ".platform.name")(platformName) <<
				Property(prefix + ".platform.version")(platformVersion) <<
				Property(prefix + ".name")(desc->GetName()) <<
				Property(prefix + ".type")(DeviceDescription::GetDeviceType(desc->GetType())) <<
				Property(prefix + ".units")(desc->GetComputeUnits()) <<
				Property(prefix + ".clock")(deviceClock) <<
				Property(prefix + ".nativevectorwidthfloat")(desc->GetNativeVectorWidthFloat()) <<
				Property(prefix + ".maxmemory")((unsigned long long)desc->GetMaxMemory()) <<
				Property(prefix + ".maxmemoryallocsize")((unsigned long long)desc->GetMaxMemoryAllocSize()) <<
				Property(prefix + ".localmemory")((unsigned long long)deviceLocalMem) <<
				Property(prefix + ".constmemory")((unsigned long long)deviceConstMem);
	}

	return props;
#endif
}

//------------------------------------------------------------------------------
// FileNameResolver
//------------------------------------------------------------------------------

void luxcore::ClearFileNameResolverPaths() {
	slg::SLG_FileNameResolver.Clear();
}

void luxcore::AddFileNameResolverPath(const std::string &path) {
	slg::SLG_FileNameResolver.AddPath(path);
}

vector<string> luxcore::GetFileNameResolverPaths() {
	return slg::SLG_FileNameResolver.GetPaths();
}

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

Film *Film::Create(const std::string &fileName) {
	return new luxcore::detail::FilmImpl(fileName);
}

Film *Film::Create(const luxrays::Properties &props,
		const bool hasPixelNormalizedChannel,
		const bool hasScreenNormalizedChannel) {
	return new luxcore::detail::FilmImpl(props, hasPixelNormalizedChannel, hasScreenNormalizedChannel);
}

Film::~Film() {
}

template<> void Film::GetOutput<float>(const FilmOutputType type, float *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	GetOutputFloat(type, buffer, index, executeImagePipeline);
}

template<> void Film::GetOutput<unsigned int>(const FilmOutputType type, unsigned int *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	GetOutputUInt(type, buffer, index, executeImagePipeline);
}

template<> void Film::UpdateOutput<float>(const FilmOutputType type, const float *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	UpdateOutputFloat(type, buffer, index, executeImagePipeline);
}

template<> void Film::UpdateOutput<unsigned int>(const FilmOutputType type, const unsigned int *buffer,
		const unsigned int index, const bool executeImagePipeline) {
	UpdateOutputUInt(type, buffer, index, executeImagePipeline);
}

template<> const float *Film::GetChannel<float>(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	return GetChannelFloat(type, index, executeImagePipeline);
}

template<> const unsigned int *Film::GetChannel(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) {
	return GetChannelUInt(type, index, executeImagePipeline);
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
	return new luxcore::detail::SceneImpl(imageScale);
}

Scene *Scene::Create(const luxrays::Properties &props, const float imageScale) {
	return new luxcore::detail::SceneImpl(props, imageScale);
}

Scene *Scene::Create(const string &fileName, const float imageScale) {
	return new luxcore::detail::SceneImpl(fileName, imageScale);
}

Scene::~Scene() {
}

template<> void Scene::DefineImageMap<unsigned char>(const std::string &imgMapName,
		unsigned char *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		Scene::ChannelSelectionType selectionType, Scene::WrapType wrapType) {
	DefineImageMapUChar(imgMapName, pixels, gamma, channels, width, height, selectionType, wrapType);
}

template<> void Scene::DefineImageMap<unsigned short>(const std::string &imgMapName,
		unsigned short *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		Scene::ChannelSelectionType selectionType, Scene::WrapType wrapType) {
	DefineImageMapHalf(imgMapName, pixels, gamma, channels, width, height, selectionType, wrapType);
}

template<> void Scene::DefineImageMap<float>(const std::string &imgMapName,
		float *pixels, const float gamma, const unsigned int channels,
		const unsigned int width, const unsigned int height,
		Scene::ChannelSelectionType selectionType, Scene::WrapType wrapType) {
	DefineImageMapFloat(imgMapName, pixels, gamma, channels, width, height, selectionType, wrapType);
}

float *Scene::AllocVerticesBuffer(const unsigned int meshVertCount) {
	return (float *)luxcore::detail::SceneImpl::AllocVerticesBuffer(meshVertCount);
}

unsigned int *Scene::AllocTrianglesBuffer(const unsigned int meshTriCount) {
	return (unsigned int *)luxcore::detail::SceneImpl::AllocTrianglesBuffer(meshTriCount);
}

//------------------------------------------------------------------------------
// RenderConfig
//------------------------------------------------------------------------------

RenderConfig *RenderConfig::Create(const Properties &props, Scene *scn) {
	luxcore::detail::SceneImpl *scnImpl = dynamic_cast<luxcore::detail::SceneImpl *>(scn);

	return new luxcore::detail::RenderConfigImpl(props, scnImpl);
}

RenderConfig *RenderConfig::Create(const std::string &fileName) {
	return new luxcore::detail::RenderConfigImpl(fileName);
}

RenderConfig *RenderConfig::Create(const std::string &fileName, RenderState **startState,
		Film **startFilm) {
	luxcore::detail::RenderStateImpl *ss;
	luxcore::detail::FilmImpl *sf;
	RenderConfig *rcfg = new luxcore::detail::RenderConfigImpl(fileName, &ss, &sf);
	
	*startState = ss;
	*startFilm = sf;
	return rcfg;
}

RenderConfig::~RenderConfig() {
}

const Properties &RenderConfig::GetDefaultProperties() {
	return luxcore::detail::RenderConfigImpl::GetDefaultProperties();
}

//------------------------------------------------------------------------------
// RenderState
//------------------------------------------------------------------------------

RenderState *RenderState::Create(const std::string &fileName) {
	return new luxcore::detail::RenderStateImpl(fileName);
}

RenderState::~RenderState() {
}

//------------------------------------------------------------------------------
// RenderSession
//------------------------------------------------------------------------------

RenderSession *RenderSession::Create(const RenderConfig *config, RenderState *startState, Film *startFilm) {
	const luxcore::detail::RenderConfigImpl *configImpl = dynamic_cast<const luxcore::detail::RenderConfigImpl *>(config);
	luxcore::detail::RenderStateImpl *startStateImpl = dynamic_cast<luxcore::detail::RenderStateImpl *>(startState);
	luxcore::detail::FilmImpl *startFilmImpl = dynamic_cast<luxcore::detail::FilmImpl *>(startFilm);

	return new luxcore::detail::RenderSessionImpl(configImpl, startStateImpl, startFilmImpl);
}

RenderSession *RenderSession::Create(const RenderConfig *config, const std::string &startStateFileName,
		const std::string &startFilmFileName) {
	const luxcore::detail::RenderConfigImpl *configImpl = dynamic_cast<const luxcore::detail::RenderConfigImpl *>(config);

	return new luxcore::detail::RenderSessionImpl(configImpl, startStateFileName, startFilmFileName);
}

RenderSession::~RenderSession() {
}
