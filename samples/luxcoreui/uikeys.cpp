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
			app->config->GetScene().GetCamera().Translate(0.f, 0.f, app->optMoveStep);
			app->session->EndSceneEdit();
			break;
		}
		case GLFW_KEY_F: {
			app->session->BeginSceneEdit();
			app->config->GetScene().GetCamera().Translate(0.f, 0.f, -app->optMoveStep);
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

	if (ImGui::GetIO().WantCaptureKeyboard)
		return;

	LuxCoreApp *app = (LuxCoreApp *)glfwGetWindowUserPointer(window);

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
				app->session->Stop();
				app->session->Start();

				// For some test with lux-hdr scene

				/*app->session->BeginSceneEdit();
				app->session->GetFilm().DeleteAllImagePipelines();
				app->session->Parse(Properties().SetFromString(
						"film.imagepipelines.0.0.type = NOP\n"
						"film.imagepipelines.0.1.type = TONEMAP_LINEAR\n"
						"film.imagepipelines.0.1.scale = 0.50000002374872565\n"
						"film.imagepipelines.0.radiancescales.0.enabled = 1\n"
						"film.imagepipelines.0.radiancescales.0.globalscale = 1\n"
						"film.imagepipelines.0.radiancescales.1.enabled = 0\n"
						"film.imagepipelines.0.radiancescales.1.globalscale = 1\n"
						"film.imagepipelines.0.radiancescales.1.rgbscale = 1 1 1\n"
						"film.imagepipelines.0.radiancescales.2.enabled = 1\n"
						"film.imagepipelines.0.radiancescales.2.rgbscale = 1 1 1\n"
						"film.imagepipelines.0.radiancescales.2.globalscale = 1\n"));
				app->session->EndSceneEdit();*/
				
				/*app->session->BeginSceneEdit();
				app->config->GetScene().Parse(Properties().SetFromString(
					"scene.materials.shell01.type = matte\n"
					"scene.materials.shell01.kd = 0.1 0.1 0.1\n"
					"scene.materials.shell01.emission = 10 10 10\n"
					"scene.objects.luxshell.material = shell01\n"
					"scene.objects.luxshell.shape = luxshell\n"
					"scene.objects.luxshell.id = 255\n"
					));
				app->session->EndSceneEdit();*/

				/*static Transform t;
				//t = t * Translate(Vector(.05f, 0.f, 0.f));
				t = t * RotateX(5.f);
				t = t * RotateY(5.f);
				t = t * RotateZ(5.f);
				app->session->BeginSceneEdit();
				app->config->GetScene().UpdateObjectTransformation("luxshell", &t.m.m[0][0]);
				app->session->EndSceneEdit();*/

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
				
				/*static bool pingPong = true;
				app->session->BeginSceneEdit();
				app->config->GetScene().Parse(Properties().SetFromString(
					"scene.lights.l1.type = mappoint\n"
					"scene.lights.l1.position = " + (pingPong ? ToString(-3.f) : ToString(3.f)) + " -5.0 6.0\n"
					"scene.lights.l1.gain = 250.0 250.0 250.0\n"
					"scene.lights.l1.iesfile = scenes/bigmonkey/ASA_15M_R2.ies\n"
					"scene.lights.l1.flipz = 1\n"
					));
				pingPong = !pingPong;
				app->session->EndSceneEdit();*/
				
				/*static Vector orig(.6f, -1.7f, 1.4f);
				static Vector target(0.f, 0.f, .4f);
				orig += (orig - target) * -.2f;
				app->session->BeginSceneEdit();
				app->config->GetScene().Parse(Properties().SetFromString(
					"scene.camera.type = orthographic\n"
					"scene.camera.lookat.orig = " + ToString(orig.x)+" " + ToString(orig.y)+" " + ToString(orig.z)+"\n"
					"scene.camera.lookat.target = " + ToString(target.x)+" " + ToString(target.y)+" " + ToString(target.z)+"\n"));
				app->session->EndSceneEdit();*/

				/*app->session->BeginSceneEdit();
				static float tx = 0.f;
				tx += .5f;
				app->config->GetScene().Parse(Properties().SetFromString(
					"scene.shapes.shape_box2.type = mesh\n"
					"scene.shapes.shape_box2.ply = scenes/simple/simple-mat-cube2.ply\n"
					"scene.shapes.shape_box2.transformation = 1 0 0 0 0 1 0 0 0 0 1 0 " + ToString(tx) + " 0 0 1\n"
//					));
//				app->config->GetScene().Parse(Properties().SetFromString(
					"scene.materials.greenmatte.type = matte\n"
					"scene.materials.greenmatte.kd = 0.0 0.57 0.0\n"
					"scene.materials.greenmatte.emission = 0.0 4.0 0.0\n"
					"scene.objects.box2.material = greenmatte\n"
					"scene.objects.box2.shape = shape_box2\n"
					"scene.objects.box2.transformation = 1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1\n"
					));
				app->session->EndSceneEdit();*/
				break;
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
				if (app->isGPURenderingAvailable()) {
					app->CloseAllRenderConfigEditors();
					app->SetRenderingEngineType("PATHOCL");
				}
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
			case GLFW_KEY_6:
				if (app->isGPURenderingAvailable()) {
					app->CloseAllRenderConfigEditors();
					app->SetRenderingEngineType("RTPATHOCL");
				}
				break;
			case GLFW_KEY_7:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("TILEPATHCPU");
				break;
			case GLFW_KEY_8:
				if (app->isGPURenderingAvailable()) {
					app->CloseAllRenderConfigEditors();
					app->SetRenderingEngineType("TILEPATHOCL");
				}
				break;
			case GLFW_KEY_9:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("RTPATHCPU");
				break;
			case GLFW_KEY_0:
				app->CloseAllRenderConfigEditors();
				app->SetRenderingEngineType("BAKECPU");
				break;

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
