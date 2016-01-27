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

void LuxCoreApp::ToolCameraEditKeys(GLFWwindow *window, int key, int scanCode, int action, int mods) {
	LuxCoreApp *app = (LuxCoreApp *)glfwGetWindowUserPointer(window);

	switch (key) {
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
		default:
			break;
	}
}
		
void LuxCoreApp::GLFW_KeyCallBack(GLFWwindow *window, int key, int scanCode, int action, int mods) {
	ImGui_ImplGlFw_KeyCallback(window, key, scanCode, action, mods);

	LuxCoreApp *app = (LuxCoreApp *)glfwGetWindowUserPointer(window);

	if (ImGui::IsMouseHoveringAnyWindow())
		return;

	//--------------------------------------------------------------------------
	// Application related keys
	//--------------------------------------------------------------------------

	if (action == GLFW_PRESS) {
		switch (key) {
			case GLFW_KEY_ESCAPE:
				glfwSetWindowShouldClose(window, GL_TRUE);
				break;
			default:
				break;
		}
	}

	//--------------------------------------------------------------------------
	// RenderSession related keys
	//--------------------------------------------------------------------------

	if (!app->session)
		return;

	if (action == GLFW_PRESS) {
		ImGuiIO &io = ImGui::GetIO();

		switch (key) {
			case GLFW_KEY_P: {
				if (io.KeyShift)
					app->session->GetFilm().SaveFilm("film.flm");
				else
					app->session->GetFilm().SaveOutputs();
				break;
			}
			case GLFW_KEY_SPACE: {
				// Restart rendering
//				app->session->Stop();
//				app->session->Start();

				// For some test with lux-hdr scene
				/*app->session->BeginSceneEdit();
				app->config->GetScene().Parse(Properties().SetFromString(
					"scene.materials.shell01.type = matte\n"
					"scene.materials.shell01.kd = 0.75 0.0 0.0\n"
					"scene.objects.luxshell.material = shell01\n"
					"scene.objects.luxshell.shape = luxshell\n"
					"scene.objects.luxshell.id = 255\n"
					));
				app->session->EndSceneEdit();*/
				
				/*app->session->BeginSceneEdit();
				app->config->GetScene().Parse(Properties().SetFromString(
					"scene.materials.shell.type = mattetranslucent\n"
					"scene.materials.shell.kr = 0.25 0.0 0.0\n"
					"scene.materials.shell.kt = 0.75 0.0 0.0\n"
					));
				app->session->EndSceneEdit();*/

				/*static bool pingPong = true;
				app->session->BeginSceneEdit();
				if (pingPong)
					app->config->GetScene().Parse(Properties().SetFromString(
						"scene.lights.infinitelight.type = infinite\n"
						"scene.lights.infinitelight.file = scenes/simple-mat/sky.exr\n"
						"scene.lights.infinitelight.gamma = 1.0\n"
						"scene.lights.infinitelight.gain = 1.0 1.0 1.0\n"
						"scene.lights.infinitelight.storage = byte\n"
						));
				else
					app->config->GetScene().Parse(Properties().SetFromString(
						"scene.lights.infinitelight.type = infinite\n"
						"scene.lights.infinitelight.file = scenes/simple-mat/arch.exr\n"
						"scene.lights.infinitelight.gamma = 1.0\n"
						"scene.lights.infinitelight.gain = 1.0 1.0 1.0\n"
						"scene.lights.infinitelight.storage = byte\n"
						));
				pingPong = !pingPong;
				app->config->GetScene().RemoveUnusedImageMaps();
				app->config->GetScene().RemoveUnusedTextures();
				app->config->GetScene().RemoveUnusedMaterials();
				app->config->GetScene().RemoveUnusedMeshes();
				app->session->EndSceneEdit();*/
			}
			case GLFW_KEY_N: {
				app->DecScreenRefreshInterval();
				break;
			}
			case GLFW_KEY_M: {
				app->IncScreenRefreshInterval();
				break;
			}
			case GLFW_KEY_1:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("PATHOCL");
				break;
			case GLFW_KEY_2:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("LIGHTCPU");
				break;
			case GLFW_KEY_3:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("PATHCPU");
				break;
			case GLFW_KEY_4:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("BIDIRCPU");
				break;
			case GLFW_KEY_5:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("BIDIRVMCPU");
				break;
#if !defined(LUXRAYS_DISABLE_OPENCL)
			case GLFW_KEY_6:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("RTPATHOCL");
				break;
#endif
			case GLFW_KEY_7:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("BIASPATHCPU");
				break;
#if !defined(LUXRAYS_DISABLE_OPENCL)
			case GLFW_KEY_8:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("BIASPATHOCL");
				break;
#endif
#if !defined(LUXRAYS_DISABLE_OPENCL)
			case GLFW_KEY_9:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("RTBIASPATHOCL");
				break;
#endif
			case GLFW_KEY_H:
				app->helpWindow.Toggle();
				break;
			case GLFW_KEY_J:
				app->statsWindow.Toggle();
				break;
			default:
				break;
		}

		// Tool specific keys
		switch(app->currentTool) {
			case TOOL_CAMERA_EDIT: {
				LuxCoreApp::ToolCameraEditKeys(window, key, scanCode, action, mods);
				break;
			}
			default:
				break;
		}
	}
}
