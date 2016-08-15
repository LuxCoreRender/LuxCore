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

#include <cstdlib>
#include <iostream>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

LogWindow *LuxCoreApp::currentLogWindow = NULL;

ImVec4 LuxCoreApp::colLabel = ImVec4(1.f, .5f, 0.f, 1.f);

//------------------------------------------------------------------------------
// LuxCoreApp
//------------------------------------------------------------------------------

LuxCoreApp::LuxCoreApp(luxcore::RenderConfig *renderConfig) :
		acceleratorWindow(this), epsilonWindow(this),
		filmChannelsWindow(this), filmOutputsWindow(this),
		filmRadianceGroupsWindow(this), lightStrategyWindow(this),
		oclDeviceWindow(this), pixelFilterWindow(this),
		renderEngineWindow(this), samplerWindow(this),
		statsWindow(this), logWindow(this), helpWindow(this) {
	config = renderConfig;
	session = NULL;
	window = NULL;

	selectionBuffer = NULL;
	selectionFilmWidth = 0xffffffffu;
	selectionFilmHeight = 0xffffffffu;
			
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
	delete[] selectionBuffer;

	delete session;
	delete config;
}

void LuxCoreApp::UpdateMoveStep() {
	const BBox &worldBBox = config->GetScene().GetDataSet().GetBBox();
	int maxExtent = worldBBox.MaximumExtent();

	const float worldSize = Max(worldBBox.pMax[maxExtent] - worldBBox.pMin[maxExtent], .001f);
	optMoveStep = optMoveScale * worldSize / 50.f;
}

void LuxCoreApp::IncScreenRefreshInterval() {
	const u_int screenRefreshInterval = config->ToProperties().Get("screen.refresh.interval").Get<u_int>();
	if (screenRefreshInterval >= 1000)
		config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 1000)));
	else if (screenRefreshInterval >= 100)
		config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 50)));
	else
		config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 5)));
}

void LuxCoreApp::DecScreenRefreshInterval() {
	const u_int screenRefreshInterval = config->ToProperties().Get("screen.refresh.interval").Get<u_int>();
	if (screenRefreshInterval > 1000)
		config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(1000u, screenRefreshInterval - 1000))));
	else if (screenRefreshInterval > 100)
		config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(50u, screenRefreshInterval - 50))));
	else
		config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(10u, screenRefreshInterval - 5))));
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
	statsWindow.Close();
}

void LuxCoreApp::SetRenderingEngineType(const string &engineType) {
	if (engineType != config->ToProperties().Get("renderengine.type").Get<string>()) {
		Properties props;
		if (engineType == "RTPATHCPU") {
			props <<
					Property("renderengine.type")("RTPATHCPU") <<
					Property("sampler.type")("RTPATHCPUSAMPLER");
		} else {
			if (config->ToProperties().Get("sampler.type").Get<string>() == "RTPATHCPUSAMPLER") {
				props <<
						Property("renderengine.type")(engineType) <<
						Property("sampler.type")("RANDOM");
			} else
				props << Property("renderengine.type")(engineType);
		}

		RenderConfigParse(props);
	}
}

void LuxCoreApp::RenderConfigParse(const Properties &props) {
	if (session) {
		// Delete the session
		delete session;
		session = NULL;
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

void LuxCoreApp::RenderSessionParse(const Properties &props) {
	try {
		session->Parse(props);
	} catch(exception &ex) {
		LA_LOG("RenderSession parse error: " << endl << ex.what());

		delete session;
		session = NULL;
	}
}

void LuxCoreApp::AdjustFilmResolutionToWindowSize(u_int *filmWidth, u_int *filmHeight) {
	int currentFrameBufferWidth, currentFrameBufferHeight;
	glfwGetFramebufferSize(window, &currentFrameBufferWidth, &currentFrameBufferHeight);
	const float newRatio = currentFrameBufferWidth / (float) currentFrameBufferHeight;

	if (newRatio >= 1.f)
		*filmHeight = (u_int) (*filmWidth * (1.f / newRatio));
	else
		*filmWidth = (u_int) (*filmHeight * newRatio);
	LA_LOG("Film size adjusted: " << *filmWidth << "x" << *filmHeight << " (Frame buffer size: " << currentFrameBufferWidth << "x" << currentFrameBufferHeight << ")");
}

void LuxCoreApp::SetFilmResolution(const u_int width, const u_int height) {
	// Close film related editors
	filmChannelsWindow.Close();
	filmOutputsWindow.Close();
	filmRadianceGroupsWindow.Close();

	targetFilmWidth = width;
	targetFilmHeight = height;

	StartRendering();
}

void LuxCoreApp::LoadRenderConfig(const std::string &configFileName) {
	DeleteRendering();

	// Set the current directory to place where the configuration file is
	boost::filesystem::current_path(boost::filesystem::path(configFileName).parent_path());

	try {
		if ((configFileName.length() >= 4) && (configFileName.substr(configFileName.length() - 4) == ".lxs")) {
			// It is a LuxRender SDL file
			LA_LOG("Parsing LuxRender SDL file...");
			Properties renderConfigProps, sceneProps;
			luxcore::ParseLXS(configFileName, renderConfigProps, sceneProps);

			// For debugging
			//LA_LOG("RenderConfig: \n" << renderConfigProps);
			//LA_LOG("Scene: \n" << sceneProps);

			Scene *scene = new Scene(renderConfigProps.Get(Property("images.scale")(1.f)).Get<float>());
			scene->Parse(sceneProps);
			config = new RenderConfig(renderConfigProps, scene);
			config->DeleteSceneOnExit();
		} else {
			// It is a LuxCore SDL file
			config = new RenderConfig(Properties(configFileName));
		}

		StartRendering();
	} catch(exception &ex) {
		LA_LOG("RenderConfig loading error: " << endl << ex.what());

		delete session;
		session = NULL;
		delete config;
		config = NULL;
	}
}

void LuxCoreApp::StartRendering() {
	CloseAllRenderConfigEditors();

	if (session)
		delete session;
	session = NULL;
	
	const string engineType = config->ToProperties().Get("renderengine.type").Get<string>();
	if (boost::starts_with(engineType, "RT")) {
		if (config->ToProperties().Get("screen.refresh.interval").Get<u_int>() > 25)
			config->Parse(Properties().Set(Property("screen.refresh.interval")(25)));
		optRealTimeMode = true;
		// Reset the dropped frames counter
		droppedFramesCount = 0;
		refreshDecoupling = 1;
	} else
		optRealTimeMode = false;

	currentTool = TOOL_CAMERA_EDIT;

	// Delete scene.camera.screenwindow so frame buffer resize will
	// automatically adjust the ratio
	Properties cameraProps = config->GetScene().ToProperties().GetAllProperties("scene.camera");
	cameraProps.DeleteAll(cameraProps.GetAllNames("scene.camera.screenwindow"));
	config->GetScene().Parse(cameraProps);

	u_int filmWidth = targetFilmWidth;
	u_int filmHeight = targetFilmHeight;
	if (config->ToProperties().Get("screen.adjustfilmratio.enable").Get<bool>()) {
		// Adjust the width and height to match the window width and height ratio
		AdjustFilmResolutionToWindowSize(&filmWidth, &filmHeight);
	}

	Properties cfgProps;
	cfgProps <<
			Property("film.width")(filmWidth) <<
			Property("film.height")(filmHeight);
	config->Parse(cfgProps);

	try {
		session = new RenderSession(config);

		// Re-start the rendering
		session->Start();

		UpdateMoveStep();
	} catch(exception &ex) {
		LA_LOG("RenderSession starting error: " << endl << ex.what());

		delete session;
		session = NULL;
	}
}

void LuxCoreApp::DeleteRendering() {
	CloseAllRenderConfigEditors();

	delete session;
	session = NULL;
	delete config;
	config = NULL;
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
