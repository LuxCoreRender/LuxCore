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

using namespace std;
using namespace luxrays;
using namespace luxcore;

static RenderSession *renderSession;
static GLuint renderFrameBufferTexID;

static void GLFWErrorCallback(int error, const char *description) {
	cout <<
			"GLFW Error: " << error << "\n"
			"Description: " << description << "\n";
}

static void RefreshRenderingTexture() {
	const u_int filmWidth = renderSession->GetFilm().GetWidth();
	const u_int filmHeight = renderSession->GetFilm().GetHeight();
	const float *pixels = renderSession->GetFilm().GetChannel<float>(Film::CHANNEL_RGB_TONEMAPPED);

	glBindTexture(GL_TEXTURE_2D, renderFrameBufferTexID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, filmWidth, filmHeight, 0, GL_RGB, GL_FLOAT, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	renderSession->UpdateStats();
}

static void DrawRendering() {
	const u_int filmWidth = renderSession->GetFilm().GetWidth();
	const u_int filmHeight = renderSession->GetFilm().GetHeight();

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

static void CenterWindow(GLFWwindow *window) {
	GLFWmonitor *monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	int windowWidth, windowHeight;
	glfwGetWindowSize(window, &windowWidth, &windowHeight);

	glfwSetWindowPos(window, (mode->width - windowWidth) / 2, (mode->height - windowHeight) / 2);
}

void UILoop(RenderConfig *renderConfig) {
	glfwSetErrorCallback(GLFWErrorCallback);
	if (!glfwInit())
		exit(EXIT_FAILURE);

	// It is important to initialize OpenGL before OpenCL
	// (required in case of OpenGL/OpenCL inter-operability)
	u_int filmWidth, filmHeight;
	renderConfig->GetFilmSize(&filmWidth, &filmHeight, NULL);

	glfwWindowHint(GLFW_DEPTH_BITS, 0);
	glfwWindowHint(GLFW_ALPHA_BITS, 0);

	const string windowTitle = "LuxCore UI v" LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR " (http://www.luxrender.net)";
	GLFWwindow *window = glfwCreateWindow(filmWidth, filmHeight, windowTitle.c_str(), NULL, NULL);
	if (!window) {
		glfwTerminate();
		throw runtime_error("Error while opening GLFW window");
	}

	glfwMakeContextCurrent(window);
	CenterWindow(window);

	// Setup ImGui binding
    ImGui_ImplGlfw_Init(window, true);
	ImGui::GetIO().IniFilename = NULL;

	int lastFrameBufferWidth, lastFrameBufferHeight;
	glfwGetFramebufferSize(window, &lastFrameBufferWidth, &lastFrameBufferHeight);

	// Start the rendering
	renderSession = new RenderSession(renderConfig);
	renderSession->Start();
	renderSession->UpdateStats();

	filmWidth = renderSession->GetFilm().GetWidth();
	filmHeight = renderSession->GetFilm().GetHeight();

	// Initialize OpenGL
    glGenTextures(1, &renderFrameBufferTexID);

	glViewport(0, 0, lastFrameBufferWidth, lastFrameBufferHeight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.f, filmWidth,
			0.f, filmHeight,
			-1.f, 1.f);

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
						" (Frame buffer size:" << currentFrameBufferWidth << "x" << currentFrameBufferHeight << ")");

				glLoadIdentity();
				glOrtho(0.f, filmWidth,
						0.f, filmHeight,
						-1.f, 1.f);

				lastFrameBufferWidth = currentFrameBufferWidth;
				lastFrameBufferHeight = currentFrameBufferHeight;

				// Stop the session
				renderSession->Stop();

				// Delete the session
				delete renderSession;
				renderSession = NULL;

				// Change the film size
				renderConfig->Parse(
						Property("film.width")(filmWidth) <<
						Property("film.height")(filmHeight));

				// Delete scene.camera.screenwindow so frame buffer resize will
				// automatically adjust the ratio
				Properties cameraProps = renderConfig->GetScene().GetProperties().GetAllProperties("scene.camera");
				cameraProps.DeleteAll(cameraProps.GetAllNames("scene.camera.screenwindow"));
				renderConfig->GetScene().Parse(cameraProps);

				renderSession = new RenderSession(renderConfig);

				// Re-start the rendering
				renderSession->Start();
			}
			
			lastFrameBufferSizeRefresh = WallClockTime();
		}

		// Check if it is time to update the frame buffer texture
		const double screenRefreshTime = renderConfig->GetProperty("screen.refresh.interval").Get<u_int>() / 1000.0;
		currentTime = WallClockTime();
		if (currentTime - lastScreenRefresh >= screenRefreshTime) {
			RefreshRenderingTexture();
			lastScreenRefresh = currentTime;
		}

		DrawRendering();

		//----------------------------------------------------------------------
		// Draw the UI
		//----------------------------------------------------------------------

		glfwPollEvents();
		ImGui_ImplGlfw_NewFrame();

		ImGui::Render();
		glfwSwapBuffers(window);

		//----------------------------------------------------------------------
		// Check if periodic save is enabled
		//----------------------------------------------------------------------

		if (renderSession->NeedPeriodicFilmSave()) {
			// Time to save the image and film
			renderSession->GetFilm().SaveOutputs();
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

	// Stop the rendering
	delete renderSession;

	glfwDestroyWindow(window);
	ImGui_ImplGlfw_Shutdown();
	glfwTerminate();
}
