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

#include <imgui.h>
#include "imgui_impl_glfw.h"

#include "luxcore/luxcore.h"

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// Camera handling with mouse
//------------------------------------------------------------------------------

void LuxCoreApp::GLFW_MousePositionCallBack(GLFWwindow *window, double x, double y) {
	LuxCoreApp *app = (LuxCoreApp *)glfwGetWindowUserPointer(window);

	if (!app->session || ImGui::GetIO().WantCaptureMouse) {
		app->mouseButton0 = false;
		app->mouseButton2 = false;
		return;
	}

	const double minInterval = 0.2;

	if (app->mouseButton0) {
		// Check elapsed time since last update
		if (app->optRealTimeMode || (WallClockTime() - app->lastMouseUpdate > minInterval)) {
			const int distX = x - app->mouseGrabLastX;
			const int distY = y - app->mouseGrabLastY;

			switch(app->currentTool) {
				case TOOL_CAMERA_EDIT: {
					app->session->BeginSceneEdit();

					if (app->optMouseGrabMode) {
						app->config->GetScene().GetCamera().RotateUp(.04f * distY * app->optRotateStep);
						app->config->GetScene().GetCamera().RotateLeft(.04f * distX * app->optRotateStep);
					}
					else {
						app->config->GetScene().GetCamera().RotateDown(.04f * distY * app->optRotateStep);
						app->config->GetScene().GetCamera().RotateRight(.04f * distX * app->optRotateStep);
					};

					app->session->EndSceneEdit();
					break;
				}
				default:
					break;
			}

			app->mouseGrabLastX = x;
			app->mouseGrabLastY = y;

			app->lastMouseUpdate = WallClockTime();
		}
	} else if (app->mouseButton2) {
		// Check elapsed time since last update
		if (app->optRealTimeMode || (WallClockTime() - app->lastMouseUpdate > minInterval)) {
			const int distX = x - app->mouseGrabLastX;
			const int distY = y - app->mouseGrabLastY;

			switch(app->currentTool) {
				case TOOL_CAMERA_EDIT: {
					app->session->BeginSceneEdit();

					if (app->optMouseGrabMode) {
						app->config->GetScene().GetCamera().TranslateLeft(.04f * distX * app->optMoveStep);
						app->config->GetScene().GetCamera().TranslateForward(.04f * distY * app->optMoveStep);
					}
					else {
						app->config->GetScene().GetCamera().TranslateRight(.04f * distX * app->optMoveStep);
						app->config->GetScene().GetCamera().TranslateBackward(.04f * distY * app->optMoveStep);
					}

					app->session->EndSceneEdit();
					break;
				}
				default:
					break;
					
			}

			app->mouseGrabLastX = x;
			app->mouseGrabLastY = y;

			app->lastMouseUpdate = WallClockTime();
		}
	}
}

void LuxCoreApp::GLFW_MouseButtonCallBack(GLFWwindow *window, int button, int action, int mods) {
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

	LuxCoreApp *app = (LuxCoreApp *)glfwGetWindowUserPointer(window);

	if (!app->session || ImGui::GetIO().WantCaptureMouse) {
		app->mouseButton0 = false;
		app->mouseButton2 = false;
		return;
	}

	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			// Record start position
			glfwGetCursorPos(window, &app->mouseGrabLastX, &app->mouseGrabLastY);
			app->mouseButton0 = true;
		} else if (action == GLFW_RELEASE) {
			app->mouseButton0 = false;
		}
	} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS) {
			// Record start position
			glfwGetCursorPos(window, &app->mouseGrabLastX, &app->mouseGrabLastY);
			app->mouseButton2 = true;
		} else if (action == GLFW_RELEASE) {
			app->mouseButton2 = false;
		}
	}
}
