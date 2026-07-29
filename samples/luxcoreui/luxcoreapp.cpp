/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include <cstdlib>
#include <iostream>
#include <boost/algorithm/string/predicate.hpp>
#include <filesystem>

#include "luxrays/utils/oclerror.h"

#include "luxcoreapp.h"
#include "fileext.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

LogWindow *LuxCoreApp::currentLogWindow = NULL;

ImVec4 LuxCoreApp::colLabel = ImVec4(1.f, .5f, 0.f, 1.f);

//------------------------------------------------------------------------------
// LuxCoreApp
//------------------------------------------------------------------------------

LuxCoreApp::LuxCoreApp(RenderConfigRPtr & renderConfig) :
		// Note: isOpenCLAvailable and isCUDAAvailable have to be initialized before
		// ObjectEditorWindow constructors call (it is used by RenderEngineWindow).
		isOpenCLAvailable(GetPlatformDesc()->Get(Property("compile.LUXRAYS_ENABLE_OPENCL")(false)).Get<bool>()),
		isCUDAAvailable(GetPlatformDesc()->Get(Property("compile.LUXRAYS_ENABLE_CUDA")(false)).Get<bool>()),
		acceleratorWindow(this), epsilonWindow(this),
		filmChannelsWindow(this), filmOutputsWindow(this),
		filmRadianceGroupsWindow(this), lightStrategyWindow(this),
		oclDeviceWindow(this),
		pixelFilterWindow(this), renderEngineWindow(this),
		samplerWindow(this), haltConditionsWindow(this),
		statsWindow(this), logWindow(this), helpWindow(this),
		userImportancePaintWindow(this),
		config(renderConfig)
		{

	session = NULL;
	window = NULL;

	renderImageBuffer = NULL;
	renderImageWidth = 0xffffffffu;
	renderImageHeight = 0xffffffffu;

	currentTool = TOOL_CAMERA_EDIT;

	menuBarHeight = 0;
	captionHeight = 0;
	mouseHoverRenderingWindow = false;

	optRealTimeMode = false;
	droppedFramesCount = 0;
	refreshDecoupling = 1;

	optMouseGrabMode = false;
	optMoveScale = 1.f;
	optMoveStep = .5f;
	optRotateStep = 4.f;

	mouseButton0 = false;
	mouseButton2 = false;
	mouseGrabLastX = 0.0;
	mouseGrabLastY = 0.0;
	lastMouseUpdate = 0.0;
	
	optFullScreen = false;
	
	currentLogWindow = &logWindow;

	renderFrameBufferTexMinFilter = GL_LINEAR;
	renderFrameBufferTexMagFilter = GL_LINEAR;
	
	imagePipelineIndex = 0;

	guiLoopTimeShortAvg = 0.0;
	guiLoopTimeLongAvg = 0.0;
	guiSleepTime = 0.0;
	guiFilmUpdateTime = 0.0;
}

LuxCoreApp::~LuxCoreApp() {
	currentLogWindow = NULL;
	delete[] renderImageBuffer;
}

static int MaximumExtent(const float pMin[3], const float pMax[3]) {
	const float diagX = pMax[0] - pMin[0];
	const float diagY = pMax[1] - pMin[1];
	const float diagZ = pMax[2] - pMin[2];
	if (diagX > diagY && diagX > diagZ)
		return 0;
	else if (diagY > diagZ)
		return 1;
	else
		return 2;
}
void LuxCoreApp::UpdateMoveStep() {
	float pMin[3], pMax[3];
	config->GetScene().GetBBox(pMin, pMax);
	const int maxExtent = MaximumExtent(pMin, pMax);

	const float worldSize = Max(pMax[maxExtent] - pMin[maxExtent], .001f);
	optMoveStep = optMoveScale * worldSize / 50.f;
}

void LuxCoreApp::SetRefreshInterval(int interval) {
    auto props = std::make_unique<Properties>();
    props->Set(Property("screen.refresh.interval")(interval));
    config->Parse(props);
}

void LuxCoreApp::IncScreenRefreshInterval() {


	const unsigned int screenRefreshInterval = config->ToProperties()->Get("screen.refresh.interval").Get<unsigned int>();

	if (screenRefreshInterval >= 1000) SetRefreshInterval(screenRefreshInterval + 1000);
	else if (screenRefreshInterval >= 100)
		SetRefreshInterval(screenRefreshInterval +50);
	else
		SetRefreshInterval(screenRefreshInterval + 5);
}

void LuxCoreApp::DecScreenRefreshInterval() {
	const unsigned int screenRefreshInterval = config->ToProperties()->Get("screen.refresh.interval").Get<unsigned int>();
	if (screenRefreshInterval > 1000)
		SetRefreshInterval(Max(1000u, screenRefreshInterval - 1000));
	else if (screenRefreshInterval > 100)
		SetRefreshInterval(Max(50u, screenRefreshInterval - 50));
	else
		SetRefreshInterval(Max(10u, screenRefreshInterval - 5));
}

void LuxCoreApp::CloseAllRenderConfigEditors() {
	acceleratorWindow.Close();
	epsilonWindow.Close();
	filmChannelsWindow.Close();
	filmOutputsWindow.Close();
	filmRadianceGroupsWindow.Close();
	lightStrategyWindow.Close();
	oclDeviceWindow.Close();
	pixelFilterWindow.Close();
	renderEngineWindow.Close();
	samplerWindow.Close();
	haltConditionsWindow.Close();
	statsWindow.Close();
}

void LuxCoreApp::SetRenderingEngineType(const string &engineType) {
	if (engineType != config->ToProperties()->Get("renderengine.type").Get<string>()) {
		Properties props;
		if (engineType == "RTPATHCPU") {
			props <<
					Property("renderengine.type")("RTPATHCPU") <<
					Property("sampler.type")("RTPATHCPUSAMPLER");
		} else if (engineType == "RTPATHOCL") {
			props <<
					Property("renderengine.type")("RTPATHOCL") <<
					Property("sampler.type")("TILEPATHSAMPLER");
		} else if (engineType == "TILEPATHCPU") {
			props <<
					Property("renderengine.type")("TILEPATHCPU") <<
					Property("sampler.type")("TILEPATHSAMPLER");
		} else if (engineType == "TILEPATHOCL") {
			props <<
					Property("renderengine.type")("TILEPATHOCL") <<
					Property("sampler.type")("TILEPATHSAMPLER");
		} else {
			props <<
					Property("renderengine.type")(engineType) <<
					Property("sampler.type")("SOBOL");
		}

		auto pprops = std::make_unique<Properties>(std::move(props));
		RenderConfigParse(pprops);
	}
}

void LuxCoreApp::RenderConfigParse(PropertiesRPtr props) {
	if (session) {
		// Reset the session
		session.reset();
	}

	// Change the configuration
	try {
		config->Parse(props);
	} catch(exception &ex) {
		LA_LOG("RenderConfig fatal parse error: " << endl << ex.what());
		// I can not recover from a RenderConfig parse error: I would have to create
		// a new RenderConfig
		exit(EXIT_FAILURE);
	}

	StartRendering();
}

void LuxCoreApp::RenderSessionParse(const std::unique_ptr<Properties> & props) {
	try {
		session->Parse(props);
	} catch(exception &ex) {
		LA_LOG("RenderSession parse error: " << endl << ex.what());

		session.reset();
		session = nullptr;
	}
}

void LuxCoreApp::AdjustFilmResolutionToWindowSize(unsigned int *filmWidth, unsigned int *filmHeight) {
	int currentFrameBufferWidth, currentFrameBufferHeight;
	glfwGetFramebufferSize(window, &currentFrameBufferWidth, &currentFrameBufferHeight);
	const float newRatio = currentFrameBufferWidth / (float) currentFrameBufferHeight;

	if (newRatio >= 1.f)
		*filmHeight = (unsigned int) (*filmWidth * (1.f / newRatio));
	else
		*filmWidth = (unsigned int) (*filmHeight * newRatio);
	LA_LOG("Film size adjusted: " << *filmWidth << "x" << *filmHeight << " (Frame buffer size: " << currentFrameBufferWidth << "x" << currentFrameBufferHeight << ")");
}

void LuxCoreApp::SetFilmResolution(const unsigned int width, const unsigned int height) {
	// Close film related editors
	filmChannelsWindow.Close();
	filmOutputsWindow.Close();
	filmRadianceGroupsWindow.Close();

	targetFilmWidth = width;
	targetFilmHeight = height;

	StartRendering();
}

void LuxCoreApp::LoadRenderConfig(const std::string &fileName, const std::string &filePath) {
	DeleteRendering();

	// Set the current directory to place where the configuration file is
	std::filesystem::current_path(std::filesystem::path(filePath));

	// Clear the file name resolver list
	luxcore::ClearFileNameResolverPaths();
	// Add the current directory to the list of place where to look for files
	luxcore::AddFileNameResolverPath(".");
	// Add the .cfg directory to the list of place where to look for files
	std::filesystem::path path(fileName);
	luxcore::AddFileNameResolverPath(path.parent_path().generic_string());

	try {
		const string ext = GetFileNameExt(fileName);
		if (ext == ".lxs") {
			// It is a LuxRender SDL file
			LA_LOG("Parsing LuxRender SDL file...");
			auto renderConfigProps = std::make_unique<Properties>();
			auto sceneProps = std::make_unique<Properties>();
			luxcore::ParseLXS(fileName, renderConfigProps, sceneProps);

			// For debugging
			//LA_LOG("RenderConfig: \n" << renderConfigProps);
			//LA_LOG("Scene: \n" << sceneProps);

			auto scene = ScenePtr(Scene::Create());
			scene->Parse(sceneProps);
			PropertiesRPtr ptr = renderConfigProps;
			config = RenderConfigRPtr(RenderConfig::Create(std::move(renderConfigProps), scene));
			config->DeleteSceneOnExit();

			StartRendering();
		} else if (ext == ".cfg") {
			// It is a LuxCore SDL file
			config = RenderConfigRPtr(RenderConfig::Create(std::make_unique<Properties>(fileName)));

			StartRendering();
		} else if (ext == ".bcf") {
			// It is a LuxCore RenderConfig binary archive
			config = RenderConfigRPtr(RenderConfig::Create(fileName));

			StartRendering();
		} else if (ext == ".rsm") {
			// It is a LuxCore resume file
			std::shared_ptr<RenderState> startState;
			std::unique_ptr<Film> startFilm;
			config = RenderConfigRPtr(RenderConfig::Create(fileName, startState, startFilm));

			StartRendering(startState, startFilm);
		} else
			throw runtime_error("Unknown file extension: " + fileName);
	} catch(exception &ex) {
		LA_LOG("RenderConfig loading error: " << endl << ex.what());

		session.reset();
		session = nullptr;
		config.reset();
		config = std::unique_ptr<RenderConfig>();
	}

	popupMenuBar = true;
}

void LuxCoreApp::StartRendering(
    std::shared_ptr<RenderState> startState, const std::unique_ptr<Film> & startFilm
) {
	CloseAllRenderConfigEditors();

	if (session) {
		session.reset();
	}

	const string engineType = config->ToProperties()->Get("renderengine.type").Get<string>();
	if (engineType.starts_with("RT")) {
		if (config->ToProperties()->Get("screen.refresh.interval").Get<unsigned int>() > 25)
			SetRefreshInterval(25);
		optRealTimeMode = true;
		// Reset the dropped frames counter
		droppedFramesCount = 0;
		refreshDecoupling = 1;
	} else
		optRealTimeMode = false;

	const string toolTypeStr = config->ToProperties()->Get("screen.tool.type").Get<string>();
	if (toolTypeStr == "OBJECT_SELECTION")
		currentTool = TOOL_OBJECT_SELECTION;
	else if (toolTypeStr == "IMAGE_VIEW")
		currentTool = TOOL_IMAGE_VIEW;
	else if (toolTypeStr == "USER_IMPORTANCE_PAINT")
		currentTool = TOOL_USER_IMPORTANCE_PAINT;
	else
		currentTool = TOOL_CAMERA_EDIT;

	// Read the image pipeline index from user-set properties (GetProperties includes -D overrides)
	imagePipelineIndex = config->GetProperties()->Get(Property("screen.imagepipeline.index")(0u)).Get<unsigned int>();

	unsigned int filmWidth = targetFilmWidth;
	unsigned int filmHeight = targetFilmHeight;
	if (currentTool != TOOL_IMAGE_VIEW) {
		// Delete scene.camera.screenwindow so frame buffer resize will
		// automatically adjust the ratio
		Properties cameraProps = std::move(Properties() << *config->GetScene().ToProperties()->GetAllProperties("scene.camera"));
		cameraProps.DeleteAll(cameraProps.GetAllNames("scene.camera.screenwindow"));
		auto pprops = std::make_unique<Properties>(std::move(cameraProps));
		config->GetScene().Parse(pprops);

		// Adjust the width and height to match the window width and height ratio
		AdjustFilmResolutionToWindowSize(&filmWidth, &filmHeight);
	}

	Properties cfgProps;
	cfgProps <<
			Property("film.width")(filmWidth) <<
			Property("film.height")(filmHeight);
	auto pcfgProps = std::make_unique<Properties>(std::move(cfgProps));
	config->Parse(pcfgProps);

	LA_LOG("RenderConfig has cached kernels: " << (config->HasCachedKernels() ? "True" : "False"));

	// TODO
	//try {
		session = (startState and startFilm) ?
			RenderSession::Create(config, startState, *startFilm) :
			RenderSession::Create(config);

		// Re-start the rendering
		session->Start();

		UpdateMoveStep();

		if (currentTool == TOOL_USER_IMPORTANCE_PAINT)
			userImportancePaintWindow.Init();
	//} catch(exception &ex) {
		//LA_LOG("RenderSession starting error: " << endl << ex.what());

		//session.reset();
		//session = NULL;
	//}
}

void LuxCoreApp::DeleteRendering() {
	CloseAllRenderConfigEditors();

	session.reset();
	config.reset();
}

//------------------------------------------------------------------------------
// Console log
//------------------------------------------------------------------------------

void LuxCoreApp::LogHandler(const char *msg) {
	cout << msg << endl;

	if (currentLogWindow)
		currentLogWindow->AddMsg(msg);
}

//------------------------------------------------------------------------------
// GUI related methods
//------------------------------------------------------------------------------

void LuxCoreApp::ColoredLabelText(const ImVec4 &col, const char *label, const char *fmt, ...) {
	ImGui::TextColored(col, "%s", label);
	ImGui::SameLine();

	va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

void LuxCoreApp::ColoredLabelText(const char *label, const char *fmt, ...) {
	ImGui::TextColored(colLabel, "%s", label);
	ImGui::SameLine();

	va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

void LuxCoreApp::HelpMarker(const char *desc) {
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", desc);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
