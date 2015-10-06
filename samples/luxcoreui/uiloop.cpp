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

static void RefreshRendering() {
	const u_int filmWidth = renderSession->GetFilm().GetWidth();
	const u_int filmHeight = renderSession->GetFilm().GetHeight();
	const float *pixels = renderSession->GetFilm().GetChannel<float>(Film::CHANNEL_RGB_TONEMAPPED);

	glBindTexture(GL_TEXTURE_2D, renderFrameBufferTexID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, filmWidth, filmHeight, 0, GL_RGB, GL_FLOAT, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

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

	renderSession->UpdateStats();
}

void UILoop(RenderConfig *renderConfig) {
	glfwSetErrorCallback(GLFWErrorCallback);
	if (!glfwInit())
		exit(EXIT_FAILURE);

	// It is important to initialize OpenGL before OpenCL
	// (required in case of OpenGL/OpenCL inter-operability)
	u_int filmWidth, filmHeight;
	renderConfig->GetFilmSize(&filmWidth, &filmHeight, NULL);

	const string windowTitle = "LuxCore UI v" LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR " (http://www.luxrender.net)";
	GLFWwindow *window = glfwCreateWindow(filmWidth, filmHeight, windowTitle.c_str(), NULL, NULL);
	if (!window) {
		glfwTerminate();
		throw runtime_error("Error while opening GLFW window");
	}

	glfwMakeContextCurrent(window);

	// Setup ImGui binding
    ImGui_ImplGlfw_Init(window, true);
	ImGui::GetIO().IniFilename = NULL;

	// Start the rendering
	renderSession = new RenderSession(renderConfig);
	renderSession->Start();
	renderSession->UpdateStats();
	
    glGenTextures(1, &renderFrameBufferTexID);

	bool show_another_window = true;
	while (!glfwWindowShouldClose(window)) {
		//----------------------------------------------------------------------
		// Refresh the screen
		//----------------------------------------------------------------------
		
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		glMatrixMode(GL_PROJECTION);
		glViewport(0, 0, width, height);
		glLoadIdentity();
		glOrtho(0.f, filmWidth,
				0.f, filmHeight,
				-1.f, 1.f);
		
		RefreshRendering();

		//----------------------------------------------------------------------
		// Draw the UI
		//----------------------------------------------------------------------

		glfwPollEvents();
		ImGui_ImplGlfw_NewFrame();

		if (show_another_window) {
			ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiSetCond_FirstUseEver);
			ImGui::Begin("Another Window", &show_another_window);
			ImGui::Text("Hello");
			ImGui::End();
		}

		ImGui::Render();
		glfwSwapBuffers(window);

		//----------------------------------------------------------------------
		// Check if periodic save is enabled
		//----------------------------------------------------------------------

		if (renderSession->NeedPeriodicFilmSave()) {
			// Time to save the image and film
			renderSession->GetFilm().SaveOutputs();
		}
	}

	// Stop the rendering
	delete renderSession;

	glfwDestroyWindow(window);
	ImGui_ImplGlfw_Shutdown();
	glfwTerminate();
}
