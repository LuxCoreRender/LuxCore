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

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// Key bindings
//------------------------------------------------------------------------------

void LuxCoreApp::GLFW_KeyCallBack(GLFWwindow *window, int key, int scanCode, int action, int mods) {
	ImGui_ImplGlFw_KeyCallback(window, key, scanCode, action, mods);

	LuxCoreApp *app = (LuxCoreApp *)glfwGetWindowUserPointer(window);

	if (ImGui::IsMouseHoveringAnyWindow())
		return;

	if (action == GLFW_PRESS) {
		ImGuiIO &io = ImGui::GetIO();

		switch (key) {
			case GLFW_KEY_ESCAPE:
				glfwSetWindowShouldClose(window, GL_TRUE);
				break;
			case GLFW_KEY_P: {
				if (io.KeyShift)
					app->session->GetFilm().SaveFilm("film.flm");
				else
					app->session->GetFilm().SaveOutputs();
				break;
			}
			case GLFW_KEY_SPACE: {
				// Restart rendering
				app->session->Stop();
				app->session->Start();
			}
			case  GLFW_KEY_A: {
				app->session->BeginSceneEdit();
				app->config->GetScene().GetCamera().TranslateLeft(app->optMoveStep);
				app->session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_D: {
				app->session->BeginSceneEdit();
				app->config->GetScene().GetCamera().TranslateRight(app->optMoveStep);
				app->session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_W: {
				app->session->BeginSceneEdit();
				app->config->GetScene().GetCamera().TranslateForward(app->optMoveStep);
				app->session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_S: {
				app->session->BeginSceneEdit();
				app->config->GetScene().GetCamera().TranslateBackward(app->optMoveStep);
				app->session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_R: {
				app->session->BeginSceneEdit();
				app->config->GetScene().GetCamera().Translate(Vector(0.f, 0.f, app->optMoveStep));
				app->session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_F: {
				app->session->BeginSceneEdit();
				app->config->GetScene().GetCamera().Translate(Vector(0.f, 0.f, -app->optMoveStep));
				app->session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_UP: {
				app->session->BeginSceneEdit();
				app->config->GetScene().GetCamera().RotateUp(app->optRotateStep);
				app->session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_DOWN: {
				app->session->BeginSceneEdit();
				app->config->GetScene().GetCamera().RotateDown(app->optRotateStep);
				app->session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_LEFT: {
				app->session->BeginSceneEdit();
				app->config->GetScene().GetCamera().RotateLeft(app->optRotateStep);
				app->session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_RIGHT: {
				app->session->BeginSceneEdit();
				app->config->GetScene().GetCamera().RotateRight(app->optRotateStep);
				app->session->EndSceneEdit();
				break;
			}
			case GLFW_KEY_N: {
				const u_int screenRefreshInterval = app->config->GetProperty("screen.refresh.interval").Get<u_int>();
				if (screenRefreshInterval > 1000)
					app->config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(1000u, screenRefreshInterval - 1000))));
				else if (screenRefreshInterval > 100)
					app->config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(50u, screenRefreshInterval - 50))));
				else
					app->config->Parse(Properties().Set(Property("screen.refresh.interval")(Max(10u, screenRefreshInterval - 5))));
				break;
			}
			case GLFW_KEY_M: {
				const u_int screenRefreshInterval = app->config->GetProperty("screen.refresh.interval").Get<u_int>();
				if (screenRefreshInterval >= 1000)
					app->config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 1000)));
				else if (screenRefreshInterval >= 100)
					app->config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 50)));
				else
					app->config->Parse(Properties().Set(Property("screen.refresh.interval")(screenRefreshInterval + 5)));
				break;
			}
			default:
				break;
		}
	}
}
