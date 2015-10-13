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

#include <iostream>
#include <boost/algorithm/string/predicate.hpp>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

LogWindow *LuxCoreApp::currentLogWindow = NULL;

ImVec4 LuxCoreApp::colLabel = ImVec4(1.f, .5f, 0.f, 1.f);

//------------------------------------------------------------------------------
// LuxCoreApp
//------------------------------------------------------------------------------

LuxCoreApp::LuxCoreApp(luxcore::RenderConfig *renderConfig) : samplerWindow(this),
		statsWindow(this) {
	config = renderConfig;
	session = NULL;
	window = NULL;

	optRealTimeMode = false;
	optMouseGrabMode = false;
	optMoveScale = 1.f;
	optMoveStep = .5f;
	optRotateStep = 4.f;

	mouseButton0 = false;
	mouseButton2 = false;
	mouseGrabLastX = 0.0;
	mouseGrabLastY = 0.0;
	lastMouseUpdate = 0.0;
	
	currentLogWindow = &logWindow;
}

LuxCoreApp::~LuxCoreApp() {
}

void LuxCoreApp::IncScreenRefreshInterval() {
	const u_int screenRefreshInterval = config->GetProperty("screen.refresh.interval").Get<u_int>();
	if (screenRefreshInterval >= 1000)
		config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 1000)));
	else if (screenRefreshInterval >= 100)
		config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 50)));
	else
		config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 5)));
}

void LuxCoreApp::DecScreenRefreshInterval() {
	const u_int screenRefreshInterval = config->GetProperty("screen.refresh.interval").Get<u_int>();
	if (screenRefreshInterval > 1000)
		config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(1000u, screenRefreshInterval - 1000))));
	else if (screenRefreshInterval > 100)
		config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(50u, screenRefreshInterval - 50))));
	else
		config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(10u, screenRefreshInterval - 5))));
}

void LuxCoreApp::SetRenderingEngineType(const string &engineType) {
	if (engineType != config->GetProperty("renderengine.type").Get<string>()) {
		// Stop the session
		session->Stop();

		// Delete the session
		delete session;
		session = NULL;

		if (boost::starts_with(engineType, "RT")) {
			if (config->GetProperty("screen.refresh.interval").Get<u_int>() > 25)
				config->Parse(Properties().Set(Property("screen.refresh.interval")(25)));
			optRealTimeMode = true;
		} else
			optRealTimeMode = false;
		
		// Change the render engine
		config->Parse(
				Properties() <<
				Property("renderengine.type")(engineType));
		session = new RenderSession(config);

		// Re-start the rendering
		session->Start();
	}
}

void LuxCoreApp::EditRenderConfig(const Properties &samplerProps) {
	// Stop the session
	session->Stop();

	// Delete the session
	delete session;
	session = NULL;

	// Change the sampler
	config->Parse(samplerProps);
	session = new RenderSession(config);

	// Re-start the rendering
	session->Start();
}

void LuxCoreApp::SetFilmResolution(const u_int width, const u_int height) {
	u_int filmWidth = width;
	u_int filmHeight = height;

	int currentFrameBufferWidth, currentFrameBufferHeight;
	glfwGetFramebufferSize(window, &currentFrameBufferWidth, &currentFrameBufferHeight);
	const float newRatio = currentFrameBufferWidth / (float)currentFrameBufferHeight;

	if (newRatio >= 1.f)
		filmWidth = (u_int)(filmHeight * newRatio);
	else
		filmHeight = (u_int)(filmWidth * (1.f / newRatio));
	LA_LOG("Film resize: " << filmWidth << "x" << filmHeight <<
			" (Frame buffer size: " << currentFrameBufferWidth << "x" << currentFrameBufferHeight << ")");

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.f, filmWidth,
			0.f, filmHeight,
			-1.f, 1.f);

	// Stop the session
	session->Stop();

	// Delete the session
	delete session;
	session = NULL;

	// Change the film size
	config->Parse(
			Property("film.width")(filmWidth) <<
			Property("film.height")(filmHeight));

	// Delete scene.camera.screenwindow so frame buffer resize will
	// automatically adjust the ratio
	Properties cameraProps = config->GetScene().ToProperties().GetAllProperties("scene.camera");
	cameraProps.DeleteAll(cameraProps.GetAllNames("scene.camera.screenwindow"));
	config->GetScene().Parse(cameraProps);

	session = new RenderSession(config);

	// Re-start the rendering
	session->Start();

	newFilmSize[0] = filmWidth;
	newFilmSize[1] = filmHeight;
}

void LuxCoreApp::LogHandler(const char *msg) {
	cout << msg << endl;

	if (currentLogWindow)
		currentLogWindow->AddMsg(msg);
}

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
