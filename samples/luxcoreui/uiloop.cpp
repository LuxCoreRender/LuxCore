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
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp> 

#include <GLFW/glfw3.h>
#include <imgui.h>
#include "imgui_impl_glfw.h"

#include "luxrays/utils/ocl.h"
#include "luxcore/luxcore.h"
#include "include/GLFW/glfw3.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

const string windowTitle = "LuxCore UI v" LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR " (http://www.luxrender.net)";

static RenderSession *session;
static RenderConfig *config;

static GLuint renderFrameBufferTexID;
static GLFWwindow *window;

static bool optRealTimeMode = false;
// Mouse "grab" mode. This is the natural way cameras are usually manipulated
// The flag is off by default but can be turned on by using the -m switch
bool optMouseGrabMode = false;
static float optMoveScale = 1.f;
static float optMoveStep = .5f;
static float optRotateStep = 4.f;

static void GLFWErrorCallback(int error, const char *description) {
	cout <<
			"GLFW Error: " << error << "\n"
			"Description: " << description << "\n";
}

static void UpdateMoveStep() {
	const BBox &worldBBox = config->GetScene().GetDataSet().GetBBox();
	int maxExtent = worldBBox.MaximumExtent();

	const float worldSize = Max(worldBBox.pMax[maxExtent] - worldBBox.pMin[maxExtent], .001f);
	optMoveStep = optMoveScale * worldSize / 50.f;
}

static void RefreshRenderingTexture() {
	const u_int filmWidth = session->GetFilm().GetWidth();
	const u_int filmHeight = session->GetFilm().GetHeight();
	const float *pixels = session->GetFilm().GetChannel<float>(Film::CHANNEL_RGB_TONEMAPPED);

	glBindTexture(GL_TEXTURE_2D, renderFrameBufferTexID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, filmWidth, filmHeight, 0, GL_RGB, GL_FLOAT, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	session->UpdateStats();
}

static void DrawRendering() {
	const u_int filmWidth = session->GetFilm().GetWidth();
	const u_int filmHeight = session->GetFilm().GetHeight();

	glColor3f(1.f, 1.f, 1.f);
	glBindTexture(GL_TEXTURE_2D, renderFrameBufferTexID);

	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);

	glTexCoord2f(0.f, 0.f);
	glVertex2i(0, 0);

	glTexCoord2f(0.f, 1.f);
	glVertex2i(0, (GLint)filmHeight);

	glTexCoord2f(1.f, 1.f);
	glVertex2i((GLint)filmWidth, (GLint)filmHeight);

	glTexCoord2f(1.f, 0.f);
	glVertex2i((GLint)filmWidth, 0);

	glEnd();
	glDisable(GL_TEXTURE_2D);
}

static void DrawTiles(const Property &propCoords, const Property &propPasses,  const Property &propErrors,
		const u_int tileCount, const u_int tileWidth, const u_int tileHeight) {
	const bool showPassCount = config->GetProperties().Get(Property("screen.tiles.passcount.show")(false)).Get<bool>();
	const bool showError = config->GetProperties().Get(Property("screen.tiles.error.show")(false)).Get<bool>();

	int frameBufferWidth, frameBufferHeight;
	glfwGetFramebufferSize(window, &frameBufferWidth, &frameBufferHeight);

	const u_int filmWidth = session->GetFilm().GetWidth();
	const u_int filmHeight = session->GetFilm().GetHeight();
	const ImVec2 imGuiScale(frameBufferWidth / (float)filmWidth, frameBufferHeight / (float)filmHeight);

	for (u_int i = 0; i < tileCount; ++i) {
		const u_int xStart = propCoords.Get<u_int>(i * 2);
		const u_int yStart = propCoords.Get<u_int>(i * 2 + 1);
		const u_int width = Min(tileWidth, filmWidth - xStart - 1);
		const u_int height = Min(tileHeight, filmHeight - yStart - 1);

		glBegin(GL_LINE_LOOP);
		glVertex2i(xStart, yStart);
		glVertex2i(xStart + width, yStart);
		glVertex2i(xStart + width, yStart + height);
		glVertex2i(xStart, yStart + height);
		glEnd();

		if (showPassCount || showError) {
			float xs = xStart + 3.f;
			float ys = (filmHeight - yStart - 1.f) - ImGui::GetTextLineHeight() - 3.f;

			if (showError) {
				const float error = propErrors.Get<float>(i) * 256.f;
				const string errorStr = boost::str(boost::format("[%.2f]") % error);

				ImGui::SetCursorPos(ImVec2(xs * imGuiScale.x, ys * imGuiScale.y));
				ImGui::TextUnformatted(errorStr.c_str());
				
				ys -= ImGui::GetTextLineHeight();
			}

			if (showPassCount) {
				const u_int pass = propPasses.Get<u_int>(i);
				const string passStr = boost::lexical_cast<string>(pass);
				
				ImGui::SetCursorPos(ImVec2(xs * imGuiScale.x, ys * imGuiScale.y));
				ImGui::TextUnformatted(passStr.c_str());
			}
		}
	}
}

static void DrawTiles() {
	// Draw the pending, converged and not converged tiles for BIASPATHCPU or BIASPATHOCL
	const Properties &stats = session->GetStats();

	const string engineType = config->GetProperty("renderengine.type").Get<string>();
	if ((engineType == "BIASPATHCPU") || (engineType == "BIASPATHOCL")) {
		int frameBufferWidth, frameBufferHeight;
		glfwGetFramebufferSize(window, &frameBufferWidth, &frameBufferHeight);
		ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));
		ImGui::SetNextWindowSize(ImVec2((float)frameBufferWidth, (float)frameBufferHeight), ImGuiSetCond_Always);

		bool opened = true;
		if (ImGui::Begin("BIASPATH tiles", &opened, ImVec2(0.f, 0.f), 0.0f,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
			const u_int tileWidth = stats.Get("stats.biaspath.tiles.size.x").Get<u_int>();
			const u_int tileHeight = stats.Get("stats.biaspath.tiles.size.y").Get<u_int>();

			if (config->GetProperties().Get(Property("screen.tiles.converged.show")(false)).Get<bool>()) {
				// Draw converged tiles borders
				glColor3f(0.f, 1.f, 0.f);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 0.f, 1.f));

				DrawTiles(stats.Get("stats.biaspath.tiles.converged.coords"),
						stats.Get("stats.biaspath.tiles.converged.pass"),
						stats.Get("stats.biaspath.tiles.converged.error"),
						stats.Get("stats.biaspath.tiles.converged.count").Get<u_int>(),
						tileWidth, tileHeight);

				ImGui::PopStyleColor();
			}

			if (config->GetProperties().Get(Property("screen.tiles.notconverged.show")(false)).Get<bool>()) {
				// Draw converged tiles borders
				glColor3f(1.f, 0.f, 0.f);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));

				DrawTiles(stats.Get("stats.biaspath.tiles.notconverged.coords"),
						stats.Get("stats.biaspath.tiles.notconverged.pass"),
						stats.Get("stats.biaspath.tiles.notconverged.error"),
						stats.Get("stats.biaspath.tiles.notconverged.count").Get<u_int>(),
						tileWidth, tileHeight);

				ImGui::PopStyleColor();
			}

			// Draw pending tiles borders
			glColor3f(1.f, 1.f, 0.f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 0.f, 1.f));

			DrawTiles(stats.Get("stats.biaspath.tiles.pending.coords"),
					stats.Get("stats.biaspath.tiles.pending.pass"),
					stats.Get("stats.biaspath.tiles.pending.error"),
					stats.Get("stats.biaspath.tiles.pending.count").Get<u_int>(),
					tileWidth, tileHeight);

			ImGui::PopStyleColor();
		}

		ImGui::End();
	}
}

static void PrintCaptions() {
	const Properties &stats = session->GetStats();
	int frameBufferWidth, frameBufferHeight;
	glfwGetFramebufferSize(window, &frameBufferWidth, &frameBufferHeight);

	// Top screen label (to use only in full-screen mode)
	/*ImGui::SetNextWindowPos(ImVec2(0.f, -8.f));
	ImGui::SetNextWindowSize(ImVec2(frameBufferWidth, 0.f), ImGuiSetCond_Always);

	bool topOpened = true;
	if (ImGui::Begin("Top screen label", &topOpened,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
		ImGui::TextUnformatted(windowTitle.c_str());
	}
	ImGui::End();*/

	// Bottom screen label
	ImGui::SetNextWindowPos(ImVec2(0.f, (frameBufferHeight - 25.f)));
	ImGui::SetNextWindowSize(ImVec2(frameBufferWidth, 0.f), ImGuiSetCond_Always);

	bool bottomOpened = true;
	if (ImGui::Begin("Bottom screen label", &bottomOpened,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
		const string buffer = boost::str(boost::format("[Pass %3d][Avg. samples/sec % 3.2fM][Avg. rays/sample %.2f on %.1fK tris]") %
			stats.Get("stats.renderengine.pass").Get<int>() %
			(stats.Get("stats.renderengine.total.samplesec").Get<double>() / 1000000.0) %
			(stats.Get("stats.renderengine.performance.total").Get<double>() /
				stats.Get("stats.renderengine.total.samplesec").Get<double>()) %
			(stats.Get("stats.dataset.trianglecount").Get<double>() / 1000.0));

		ImGui::TextUnformatted(buffer.c_str());
	}
	ImGui::End();
}

static void CenterWindow(GLFWwindow *window) {
	GLFWmonitor *monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	int windowWidth, windowHeight;
	glfwGetWindowSize(window, &windowWidth, &windowHeight);

	glfwSetWindowPos(window, (mode->width - windowWidth) / 2, (mode->height - windowHeight) / 2);
}

//------------------------------------------------------------------------------
// Key bindings
//------------------------------------------------------------------------------

static void GLFW_KeyCallback(GLFWwindow *window, int key, int scanCode, int action, int mods) {
	ImGui_ImplGlFw_KeyCallback(window, key, scanCode, action, mods);

	if (action == GLFW_PRESS) {
		ImGuiIO &io = ImGui::GetIO();

		switch (key) {
			case GLFW_KEY_ESCAPE:
				glfwSetWindowShouldClose(window, GL_TRUE);
				break;
			case GLFW_KEY_P: {
				if (io.KeyShift)
					session->GetFilm().SaveFilm("film.flm");
				else
					session->GetFilm().SaveOutputs();
				break;
			}
			case GLFW_KEY_SPACE: {
				// Restart rendering
				session->Stop();
				session->Start();
			}
			case  GLFW_KEY_A: {
				session->BeginSceneEdit();
				config->GetScene().GetCamera().TranslateLeft(optMoveStep);
				session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_D: {
				session->BeginSceneEdit();
				config->GetScene().GetCamera().TranslateRight(optMoveStep);
				session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_W: {
				session->BeginSceneEdit();
				config->GetScene().GetCamera().TranslateForward(optMoveStep);
				session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_S: {
				session->BeginSceneEdit();
				config->GetScene().GetCamera().TranslateBackward(optMoveStep);
				session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_R: {
				session->BeginSceneEdit();
				config->GetScene().GetCamera().Translate(Vector(0.f, 0.f, optMoveStep));
				session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_F: {
				session->BeginSceneEdit();
				config->GetScene().GetCamera().Translate(Vector(0.f, 0.f, -optMoveStep));
				session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_UP: {
				session->BeginSceneEdit();
				config->GetScene().GetCamera().RotateUp(optRotateStep);
				session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_DOWN: {
				session->BeginSceneEdit();
				config->GetScene().GetCamera().RotateDown(optRotateStep);
				session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_LEFT: {
				session->BeginSceneEdit();
				config->GetScene().GetCamera().RotateLeft(optRotateStep);
				session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_RIGHT: {
				session->BeginSceneEdit();
				config->GetScene().GetCamera().RotateRight(optRotateStep);
				session->EndSceneEdit();
				break;
			}
			default:
				break;
		}
	}
}

//------------------------------------------------------------------------------
// Camera handling with mouse
//------------------------------------------------------------------------------

static bool mouseButton0 = false;
static bool mouseButton2 = false;
static double mouseGrabLastX = 0;
static double mouseGrabLastY = 0;
static double lastMouseUpdate = 0.0;

static void GLFW_MousePositionCallback(GLFWwindow *window, double x, double y) {
	const double minInterval = 0.2;

	if (mouseButton0) {
		// Check elapsed time since last update
		if (optRealTimeMode || (WallClockTime() - lastMouseUpdate > minInterval)) {
			const int distX = x - mouseGrabLastX;
			const int distY = y - mouseGrabLastY;

			session->BeginSceneEdit();

			if (optMouseGrabMode) {
				config->GetScene().GetCamera().RotateUp(.04f * distY * optRotateStep);
				config->GetScene().GetCamera().RotateLeft(.04f * distX * optRotateStep);
			}
			else {
				config->GetScene().GetCamera().RotateDown(.04f * distY * optRotateStep);
				config->GetScene().GetCamera().RotateRight(.04f * distX * optRotateStep);
			};

			session->EndSceneEdit();

			mouseGrabLastX = x;
			mouseGrabLastY = y;

			lastMouseUpdate = WallClockTime();
		}
	} else if (mouseButton2) {
		// Check elapsed time since last update
		if (optRealTimeMode || (WallClockTime() - lastMouseUpdate > minInterval)) {
			const int distX = x - mouseGrabLastX;
			const int distY = y - mouseGrabLastY;

			session->BeginSceneEdit();

			if (optMouseGrabMode) {
				config->GetScene().GetCamera().TranslateLeft(.04f * distX * optMoveStep);
				config->GetScene().GetCamera().TranslateForward(.04f * distY * optMoveStep);
			}
			else {
				config->GetScene().GetCamera().TranslateRight(.04f * distX * optMoveStep);
				config->GetScene().GetCamera().TranslateBackward(.04f * distY * optMoveStep);
			}

			session->EndSceneEdit();

			mouseGrabLastX = x;
			mouseGrabLastY = y;

			lastMouseUpdate = WallClockTime();
		}
	}
}

void GLFW_MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			// Record start position
			glfwGetCursorPos(window, &mouseGrabLastX, &mouseGrabLastY);
			mouseButton0 = true;
		} else if (action == GLFW_RELEASE) {
			mouseButton0 = false;
		}
	} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS) {
			// Record start position
			glfwGetCursorPos(window, &mouseGrabLastX, &mouseGrabLastY);
			mouseButton2 = true;
		} else if (action == GLFW_RELEASE) {
			mouseButton2 = false;
		}
	}
}

//------------------------------------------------------------------------------
// UI loop
//------------------------------------------------------------------------------

void UILoop(RenderConfig *renderConfig) {
	config = renderConfig;

	//--------------------------------------------------------------------------
	// Initialize GLFW
	//--------------------------------------------------------------------------

	glfwSetErrorCallback(GLFWErrorCallback);
	if (!glfwInit())
		exit(EXIT_FAILURE);

	// It is important to initialize OpenGL before OpenCL
	// (required in case of OpenGL/OpenCL inter-operability)
	u_int filmWidth, filmHeight;
	config->GetFilmSize(&filmWidth, &filmHeight, NULL);

	glfwWindowHint(GLFW_DEPTH_BITS, 0);
	glfwWindowHint(GLFW_ALPHA_BITS, 0);

	//--------------------------------------------------------------------------
	// Create the window
	//--------------------------------------------------------------------------

	window = glfwCreateWindow(filmWidth, filmHeight, windowTitle.c_str(), NULL, NULL);
	if (!window) {
		glfwTerminate();
		throw runtime_error("Error while opening GLFW window");
	}

	glfwMakeContextCurrent(window);

	glfwSetMouseButtonCallback(window, GLFW_MouseButtonCallback);
	glfwSetScrollCallback(window, ImGui_ImplGlfw_ScrollCallback);
	glfwSetKeyCallback(window, GLFW_KeyCallback);
	glfwSetCharCallback(window, ImGui_ImplGlfw_CharCallback);
	glfwSetCursorPosCallback(window, GLFW_MousePositionCallback);

	CenterWindow(window);

	// Setup ImGui binding
    ImGui_ImplGlfw_Init(window, false);
	ImGui::GetIO().IniFilename = NULL;

	int lastFrameBufferWidth, lastFrameBufferHeight;
	glfwGetFramebufferSize(window, &lastFrameBufferWidth, &lastFrameBufferHeight);

	//--------------------------------------------------------------------------
	// Start the rendering
	//--------------------------------------------------------------------------

	session = new RenderSession(config);
	session->Start();
	session->UpdateStats();

	UpdateMoveStep();

	filmWidth = session->GetFilm().GetWidth();
	filmHeight = session->GetFilm().GetHeight();

	//--------------------------------------------------------------------------
	// Initialize OpenGL
	//--------------------------------------------------------------------------

    glGenTextures(1, &renderFrameBufferTexID);

	glViewport(0, 0, lastFrameBufferWidth, lastFrameBufferHeight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.f, filmWidth,
			0.f, filmHeight,
			-1.f, 1.f);

	//--------------------------------------------------------------------------
	// Refresh loop
	//--------------------------------------------------------------------------

	double currentTime;
	double lastLoop = WallClockTime();
	double lastScreenRefresh = WallClockTime();
	double lastFrameBufferSizeRefresh = WallClockTime();
	while (!glfwWindowShouldClose(window)) {
		//----------------------------------------------------------------------
		// Refresh the screen
		//----------------------------------------------------------------------

		int currentFrameBufferWidth, currentFrameBufferHeight;
		glfwGetFramebufferSize(window, &currentFrameBufferWidth, &currentFrameBufferHeight);
		glViewport(0, 0, currentFrameBufferWidth, currentFrameBufferHeight);

		// Refresh the frame buffer size at 1HZ
		if (WallClockTime() - lastFrameBufferSizeRefresh > 1.0) {
			// Check if the frame buffer has been resized
			if ((currentFrameBufferWidth != lastFrameBufferWidth) ||
					(currentFrameBufferHeight != lastFrameBufferHeight)) {
				const float newRatio = currentFrameBufferWidth / (float)currentFrameBufferHeight;

				if (newRatio >= 1.f)
					filmWidth = (u_int)(filmHeight * newRatio);
				else
					filmHeight = (u_int)(filmWidth * (1.f / newRatio));
				LC_LOG("Film resize: " << filmWidth << "x" << filmHeight <<
						" (Frame buffer size: " << currentFrameBufferWidth << "x" << currentFrameBufferHeight << ")");

				glLoadIdentity();
				glOrtho(0.f, filmWidth,
						0.f, filmHeight,
						-1.f, 1.f);

				lastFrameBufferWidth = currentFrameBufferWidth;
				lastFrameBufferHeight = currentFrameBufferHeight;

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
				Properties cameraProps = config->GetScene().GetProperties().GetAllProperties("scene.camera");
				cameraProps.DeleteAll(cameraProps.GetAllNames("scene.camera.screenwindow"));
				config->GetScene().Parse(cameraProps);

				session = new RenderSession(config);

				// Re-start the rendering
				session->Start();
			}
			
			lastFrameBufferSizeRefresh = WallClockTime();
		}

		// Check if it is time to update the frame buffer texture
		const double screenRefreshTime = config->GetProperty("screen.refresh.interval").Get<u_int>() / 1000.0;
		currentTime = WallClockTime();
		if (currentTime - lastScreenRefresh >= screenRefreshTime) {
			RefreshRenderingTexture();
			lastScreenRefresh = currentTime;
		}

		DrawRendering();

		//----------------------------------------------------------------------
		// Draw the UI
		//----------------------------------------------------------------------

		ImGui_ImplGlfw_NewFrame();

		DrawTiles();
		PrintCaptions();

		ImGui::Render();

		glfwSwapBuffers(window);
		glfwPollEvents();

		//----------------------------------------------------------------------
		// Check if periodic save is enabled
		//----------------------------------------------------------------------

		if (session->NeedPeriodicFilmSave()) {
			// Time to save the image and film
			session->GetFilm().SaveOutputs();
		}

		//----------------------------------------------------------------------
		// Check for how long to sleep
		//----------------------------------------------------------------------

		currentTime = WallClockTime();
		const double loopTime = currentTime - lastLoop;
		//LC_LOG("Loop time: " << loopTime * 1000.0 << "ms");
		lastLoop = currentTime;

		// The UI loop runs at 100HZ
		if (loopTime < 0.01) {
			const double sleepTime = (0.01 - loopTime) * 0.99;
			const u_int msSleepTime = (u_int)(sleepTime * 1000.0);
			//LC_LOG("Sleep time: " << msSleepTime<< "ms");

			if (msSleepTime > 0)
				boost::this_thread::sleep_for(boost::chrono::milliseconds(msSleepTime));
		}
	}


	//--------------------------------------------------------------------------
	// Stop the rendering
	//--------------------------------------------------------------------------

	delete session;

	//--------------------------------------------------------------------------
	// Exit
	//--------------------------------------------------------------------------

	glfwDestroyWindow(window);
	ImGui_ImplGlfw_Shutdown();
	glfwTerminate();
}
