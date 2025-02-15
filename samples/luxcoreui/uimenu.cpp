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

#include <imgui.h>
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <nfd.h>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// MenuRendering
//------------------------------------------------------------------------------

static void KernelCacheFillProgressHandler(const size_t step, const size_t count) {
	LA_LOG("KernelCache FillProgressHandler Step: " << step << "/" << count);
}

void LuxCoreApp::MenuRendering() {
	if (ImGui::MenuItem("Load")) {

		nfdu8char_t *fileFileName = nullptr;
                nfdu8filteritem_t filters[4] = {
                  { "LuxCore scenes", "cfg,bcf,lxs" },
                  { "LuxCore scene (text)", "cfg" },
                  { "LuxCore scene (binary)", "bcf" },
                  { "LuxRender scene", "lxs" },
                };
                nfdopendialogu8args_t args = {0};
                args.filterList = filters;
                args.filterCount = 4;
		nfdresult_t result = NFD_OpenDialogU8_With(&fileFileName, &args);

		if (result == NFD_OKAY) {
			LoadRenderConfig(fileFileName);
			NFD_FreePathU8(fileFileName);
		}
	}

	if (session) {
		ImGui::Separator();

		if (ImGui::MenuItem("Export")) {
			nfdu8char_t *outPath = nullptr;
                        nfdsavedialogu8args_t args = {0};
			nfdresult_t result = NFD_SaveDialogU8_With(&outPath, &args);

			if (result == NFD_OKAY) {
				LA_LOG("Export current scene to directory in text format: " << outPath);

				boost::filesystem::path dir(outPath);
				boost::filesystem::create_directories(dir);

				// Save the current render engine
				const string renderEngine = config->GetProperty("renderengine.type").Get<string>();

				// Set the render engine to FILESAVER
				RenderConfigParse(Properties() <<
						Property("renderengine.type")("FILESAVER") <<
						Property("filesaver.format")("TXT") <<
						Property("filesaver.directory")(outPath) <<
						Property("filesaver.renderengine.type")(renderEngine));

				// Restore the render engine setting
				RenderConfigParse(Properties() <<
						Property("renderengine.type")(renderEngine));

                                NFD_FreePathU8(outPath);
			}
		}

		if (ImGui::MenuItem("Export (binary)")) {
			nfdu8char_t *fileName = nullptr;
                        nfdsavedialogu8args_t args = {0};
                        nfdu8filteritem_t filters[1] = {
                          { "LuxCore exported scene", "bcf" },
                        };
                        args.filterList = filters;
                        args.filterCount = 1;
			nfdresult_t result = NFD_SaveDialogU8_With(&fileName, &args);

			if (result == NFD_OKAY) {
				LA_LOG("Export current scene to file in binary format: " << fileName);
				config->Save(fileName);
                                NFD_FreePathU8(fileName);
			}
		}

		if (ImGui::MenuItem("Export (glTF)")) {
			nfdu8char_t *fileName = nullptr;
                        nfdsavedialogu8args_t args = {0};
                        nfdu8filteritem_t filters[1] = {
                          { "glTF", "gltf" },
                        };
                        args.filterList = filters;
                        args.filterCount = 1;
			nfdresult_t result = NFD_SaveDialogU8_With(&fileName, &args);

			if (result == NFD_OKAY) {
				LA_LOG("Export current scene to file in glTF format: " << fileName);
				config->ExportGLTF(fileName);
                                NFD_FreePathU8(fileName);
			}
		}
	}
	
	if (session) {
		ImGui::Separator();

		if (ImGui::MenuItem("Bake all objects"))
			BakeAllSceneObjects();
	}

	if (session) {
		ImGui::Separator();

		if (ImGui::MenuItem("Save rendering")) {
			nfdu8char_t *fileName = nullptr;
                        nfdsavedialogu8args_t args = {0};
                        nfdu8filteritem_t filters[1] = {
                          { "LuxCore resume file", "rsm" },
                        };
                        args.filterList = filters;
                        args.filterCount = 1;
			nfdresult_t result = NFD_SaveDialogU8_With(&fileName, &args);


			if (result == NFD_OKAY) {
				// Pause the current rendering
				session->Pause();

				// Save the session
				session->SaveResumeFile(string(fileName));

                                NFD_FreePathU8(fileName);

				// Resume the current rendering
				session->Resume();
			}
		}
	}

	if (ImGui::MenuItem("Resume rendering")) {

		// Select the scene
		nfdu8char_t *fileFileName = nullptr;
                nfdu8filteritem_t filters[4] = {
                  { "LuxCore resume rendering", "rsm" },
                };
                nfdopendialogu8args_t args = {0};
                args.filterList = filters;
                args.filterCount = 4;
		nfdresult_t result = NFD_OpenDialogU8_With(&fileFileName, &args);

		if (result == NFD_OKAY) {
			LoadRenderConfig(fileFileName);
			NFD_FreePathU8(fileFileName);
		}
	}

	ImGui::Separator();

	if (session) {
		if (session->IsInPause()) {
			if (ImGui::MenuItem("Resume"))
				session->Resume();
		} else {
			if (ImGui::MenuItem("Pause"))
				session->Pause();
		}

		if (ImGui::MenuItem("Cancel"))
			DeleteRendering();

		if (ImGui::MenuItem("Restart", "Space bar")) {
			// Restart rendering
			session->Stop();
			session->Start();
		}
		
		ImGui::Separator();
	}

	if (isGPURenderingAvailable() && ImGui::MenuItem("Fill kernel cache")) {
		if (session) {
			// Stop any current rendering
			DeleteRendering();
		}

		Properties props;
		/*props <<
				Property("opencl.devices.select")("100") <<
				Property("scene.epsilon.min")(0.000123f) <<
				Property("scene.epsilon.max")(0.123f);*/
		KernelCacheFill(props, KernelCacheFillProgressHandler);
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Quit", "ESC"))
		glfwSetWindowShouldClose(window, GL_TRUE);
}

//------------------------------------------------------------------------------
// MenuEngine
//------------------------------------------------------------------------------

void LuxCoreApp::MenuEngine() {
	const string currentEngineType = config->ToProperties().Get("renderengine.type").Get<string>();

	if (isGPURenderingAvailable() && ImGui::MenuItem("PATHOCL", "1", (currentEngineType == "PATHOCL"))) {
		SetRenderingEngineType("PATHOCL");
		CloseAllRenderConfigEditors();
	}
	if (ImGui::MenuItem("LIGHTCPU", "2", (currentEngineType == "LIGHTCPU"))) {
		SetRenderingEngineType("LIGHTCPU");
		CloseAllRenderConfigEditors();
	}
	if (ImGui::MenuItem("PATHCPU", "3", (currentEngineType == "PATHCPU"))) {
		SetRenderingEngineType("PATHCPU");
		CloseAllRenderConfigEditors();
	}
	if (ImGui::MenuItem("BIDIRCPU", "4", (currentEngineType == "BIDIRCPU"))) {
		SetRenderingEngineType("BIDIRCPU");
		CloseAllRenderConfigEditors();
	}
	if (ImGui::MenuItem("BIDIRVMCPU", "5", (currentEngineType == "BIDIRVMCPU"))) {
		SetRenderingEngineType("BIDIRVMCPU");
		CloseAllRenderConfigEditors();
	}
	if (isGPURenderingAvailable() && ImGui::MenuItem("RTPATHOCL", "6", (currentEngineType == "RTPATHOCL"))) {
		SetRenderingEngineType("RTPATHOCL");
		CloseAllRenderConfigEditors();
	}
	if (ImGui::MenuItem("TILEPATHCPU", "7", (currentEngineType == "TILEPATHCPU"))) {
		SetRenderingEngineType("TILEPATHCPU");
		CloseAllRenderConfigEditors();
	}
	if (isGPURenderingAvailable() && ImGui::MenuItem("TILEPATHOCL", "8", (currentEngineType == "TILEPATHOCL"))) {
		SetRenderingEngineType("TILEPATHOCL");
		CloseAllRenderConfigEditors();
	}
	if (ImGui::MenuItem("RTPATHCPU", "9", (currentEngineType == "RTPATHCPU"))) {
		SetRenderingEngineType("RTPATHCPU");
		CloseAllRenderConfigEditors();
	}
	if (ImGui::MenuItem("BAKECPU", "0", (currentEngineType == "BAKECPU"))) {
		SetRenderingEngineType("BAKECPU");
		CloseAllRenderConfigEditors();
	}
}

//------------------------------------------------------------------------------
// MenuSampler
//------------------------------------------------------------------------------

void LuxCoreApp::MenuSampler() {
	const string currentSamplerType = config->ToProperties().Get("sampler.type").Get<string>();

	if (ImGui::MenuItem("RANDOM", NULL, (currentSamplerType == "RANDOM"))) {
		samplerWindow.Close();
		RenderConfigParse(Properties() << Property("sampler.type")("RANDOM"));
	}
	if (ImGui::MenuItem("SOBOL", NULL, (currentSamplerType == "SOBOL"))) {
		samplerWindow.Close();
		RenderConfigParse(Properties() << Property("sampler.type")("SOBOL"));
	}
	if (ImGui::MenuItem("METROPOLIS", NULL, (currentSamplerType == "METROPOLIS"))) {
		samplerWindow.Close();
		RenderConfigParse(Properties() << Property("sampler.type")("METROPOLIS"));
	}
}

//------------------------------------------------------------------------------
// MenuCamera
//------------------------------------------------------------------------------

void LuxCoreApp::MenuCamera() {
	if (session && ImGui::MenuItem("Print properties")) {
		const luxrays::Properties &cameraProps = session->GetRenderConfig().
				GetScene().ToProperties().GetAllProperties("scene.camera.");
		LC_LOG("Current camera properties:" << endl << cameraProps.ToString());
	}
}

//------------------------------------------------------------------------------
// MenuTiles
//------------------------------------------------------------------------------

void LuxCoreApp::MenuTiles() {
	bool showPending = config->GetProperties().Get(Property("screen.tiles.pending.show")(true)).Get<bool>();
	if (ImGui::MenuItem("Show pending", NULL, showPending))
		RenderConfigParse(Properties() << Property("screen.tiles.pending.show")(!showPending));

	bool showConverged = config->GetProperties().Get(Property("screen.tiles.converged.show")(false)).Get<bool>();
	if (ImGui::MenuItem("Show converged", NULL, showConverged))
		RenderConfigParse(Properties() << Property("screen.tiles.converged.show")(!showConverged));
	
	bool showNotConverged = config->GetProperties().Get(Property("screen.tiles.notconverged.show")(false)).Get<bool>();
	if (ImGui::MenuItem("Show not converged", NULL, showNotConverged))
		RenderConfigParse(Properties() << Property("screen.tiles.notconverged.show")(!showNotConverged));

	ImGui::Separator();

	bool showPassCount = config->GetProperties().Get(Property("screen.tiles.passcount.show")(false)).Get<bool>();
	if (ImGui::MenuItem("Show pass count", NULL, showPassCount))
		RenderConfigParse(Properties() << Property("screen.tiles.passcount.show")(!showPassCount));

	bool showError = config->GetProperties().Get(Property("screen.tiles.error.show")(false)).Get<bool>();
	if (ImGui::MenuItem("Show error", NULL, showError))
		RenderConfigParse(Properties() << Property("screen.tiles.error.show")(!showError));
}

//------------------------------------------------------------------------------
// MenuFilm
//------------------------------------------------------------------------------

void LuxCoreApp::MenuFilm() {
	if (ImGui::BeginMenu("Set resolution")) {
		if (ImGui::MenuItem("320x240"))
			SetFilmResolution(320, 240);
		if (ImGui::MenuItem("640x480"))
			SetFilmResolution(640, 480);
		if (ImGui::MenuItem("800x600"))
			SetFilmResolution(800, 600);
		if (ImGui::MenuItem("1024x768"))
			SetFilmResolution(1024, 768);
		if (ImGui::MenuItem("1280x720"))
			SetFilmResolution(1280, 720);
		if (ImGui::MenuItem("1920x1080"))
			SetFilmResolution(1920, 1080);

		ImGui::Separator();

		if (ImGui::MenuItem("Use window resolution")) {
			int currentFrameBufferWidth, currentFrameBufferHeight;
			glfwGetFramebufferSize(window, &currentFrameBufferWidth, &currentFrameBufferHeight);

			SetFilmResolution(currentFrameBufferWidth, currentFrameBufferHeight);
		}

		ImGui::Separator();

		ImGui::TextUnformatted("Width x Height");
		ImGui::PushItemWidth(100);
		ImGui::InputInt("##width", &menuFilmWidth);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::TextUnformatted("x");
		ImGui::SameLine();
		ImGui::PushItemWidth(100);
		ImGui::InputInt("##height", &menuFilmHeight);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Apply"))
			SetFilmResolution(menuFilmWidth, menuFilmHeight);

		ImGui::EndMenu();
	}
	ImGui::Separator();
	if (session && ImGui::MenuItem("Save outputs"))
		session->GetFilm().SaveOutputs();
	if (session && ImGui::MenuItem("Save film"))
		session->GetFilm().SaveFilm("film.flm");
}

//------------------------------------------------------------------------------
// MenuImagePipeline
//------------------------------------------------------------------------------

void LuxCoreApp::MenuImagePipelines() {
	const unsigned int imagePipelineCount = session->GetFilm().GetChannelCount(Film::CHANNEL_IMAGEPIPELINE);

	for (unsigned int i = 0; i < imagePipelineCount; ++i) {
		if (ImGui::MenuItem(string("Pipeline #" + ToString(i)).c_str(), NULL, (i == imagePipelineIndex)))
			imagePipelineIndex = i;
	}

	if (imagePipelineIndex > imagePipelineCount)
		imagePipelineIndex = 0;
}

//------------------------------------------------------------------------------
// MenuScreen
//------------------------------------------------------------------------------

void LuxCoreApp::MenuScreen() {
	if (ImGui::BeginMenu("Interpolation mode")) {
		if (ImGui::MenuItem("Nearest", NULL, (renderFrameBufferTexMinFilter == GL_NEAREST))) {
			renderFrameBufferTexMinFilter = GL_NEAREST;
			renderFrameBufferTexMagFilter = GL_NEAREST;
		}
		if (ImGui::MenuItem("Linear", NULL, (renderFrameBufferTexMinFilter == GL_LINEAR))) {
			renderFrameBufferTexMinFilter = GL_LINEAR;
			renderFrameBufferTexMagFilter = GL_LINEAR;
		}

		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Refresh interval")) {
		if (ImGui::MenuItem("5ms"))
			config->Parse(Properties().Set(Property("screen.refresh.interval")(5)));
		if (ImGui::MenuItem("10ms"))
			config->Parse(Properties().Set(Property("screen.refresh.interval")(10)));
		if (ImGui::MenuItem("20ms"))
			config->Parse(Properties().Set(Property("screen.refresh.interval")(20)));
		if (ImGui::MenuItem("50ms"))
			config->Parse(Properties().Set(Property("screen.refresh.interval")(50)));
		if (ImGui::MenuItem("100ms"))
			config->Parse(Properties().Set(Property("screen.refresh.interval")(100)));
		if (ImGui::MenuItem("500ms"))
			config->Parse(Properties().Set(Property("screen.refresh.interval")(500)));
		if (ImGui::MenuItem("1000ms"))
			config->Parse(Properties().Set(Property("screen.refresh.interval")(1000)));
		if (ImGui::MenuItem("2000ms"))
			config->Parse(Properties().Set(Property("screen.refresh.interval")(2000)));

		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Decrease refresh interval","n"))
		DecScreenRefreshInterval();
	if (ImGui::MenuItem("Increase refresh interval","m"))
		IncScreenRefreshInterval();
	if (ImGui::BeginMenu("UI font scale")) {
		ImGuiIO &io = ImGui::GetIO();

		if (ImGui::MenuItem("1.0", NULL, (io.FontGlobalScale == 1.f)))
			io.FontGlobalScale = 1.f;
		if (ImGui::MenuItem("1.25", NULL, (io.FontGlobalScale == 1.25f)))
			io.FontGlobalScale = 1.25f;
		if (ImGui::MenuItem("1.5", NULL, (io.FontGlobalScale == 1.5f)))
			io.FontGlobalScale = 1.5f;
		if (ImGui::MenuItem("1.75", NULL, (io.FontGlobalScale == 1.75f)))
			io.FontGlobalScale = 1.75f;
		if (ImGui::MenuItem("2.0", NULL, (io.FontGlobalScale == 2.f)))
			io.FontGlobalScale = 2.f;

		ImGui::EndMenu();
	}
}

//------------------------------------------------------------------------------
// MenuTool
//------------------------------------------------------------------------------

void LuxCoreApp::MenuTool() {
	if (ImGui::MenuItem("Camera edit", NULL, (currentTool == TOOL_CAMERA_EDIT))) {
		currentTool = TOOL_CAMERA_EDIT;
		RenderConfigParse(Properties() << Property("screen.tool.type")("CAMERA_EDIT"));
	}
	if (ImGui::MenuItem("Object selection", NULL, (currentTool == TOOL_OBJECT_SELECTION))) {
		currentTool = TOOL_OBJECT_SELECTION;

		Properties props;
		props << Property("screen.tool.type")("OBJECT_SELECTION");

		// Check if the session a OBJECT_ID AOV enabled
		if(!session->GetFilm().HasOutput(Film::OUTPUT_OBJECT_ID)) {
			// Enable OBJECT_ID AOV
			props <<
					Property("film.outputs.LUXCOREUI_OBJECTSELECTION_AOV.type")("OBJECT_ID") <<
					Property("film.outputs.LUXCOREUI_OBJECTSELECTION_AOV.filename")("dummy.png");
		}

		RenderConfigParse(props);
	}
	if (ImGui::MenuItem("Image view", NULL, (currentTool == TOOL_IMAGE_VIEW))) {
		currentTool = TOOL_IMAGE_VIEW;
		RenderConfigParse(Properties() << Property("screen.tool.type")("IMAGE_VIEW"));
	}
	if (ImGui::MenuItem("User importance painting", NULL, (currentTool == TOOL_USER_IMPORTANCE_PAINT))) {
		currentTool = TOOL_USER_IMPORTANCE_PAINT;
		
		Properties props;
		props << Property("screen.tool.type")("USER_IMPORTANCE_PAINT");

		// Check if the session a _USER_IMPORTANCE AOV enabled
		if(!session->GetFilm().HasOutput(Film::OUTPUT_USER_IMPORTANCE)) {
			// Enable OBJECT_ID AOV
			props <<
					Property("film.outputs.LUXCOREUI_USER_IMPORTANCE_AOV.type")("USER_IMPORTANCE") <<
					Property("film.outputs.LUXCOREUI_USER_IMPORTANCE_AOV.filename")("dummy.png");
		}

		RenderConfigParse(props);
	}
}

//------------------------------------------------------------------------------
// MenuWindow
//------------------------------------------------------------------------------

void LuxCoreApp::MenuWindow() {
	if (session) {
		const string currentRenderEngineType = config->ToProperties().Get("renderengine.type").Get<string>();

		if (ImGui::MenuItem("Render Engine editor", NULL, renderEngineWindow.IsOpen()))
			renderEngineWindow.Toggle();
		if (ImGui::MenuItem("Sampler editor", NULL, samplerWindow.IsOpen(),
				!boost::starts_with(currentRenderEngineType, "TILE") && !boost::starts_with(currentRenderEngineType, "RT")))
			samplerWindow.Toggle();
		if (ImGui::MenuItem("Pixel Filter editor", NULL, pixelFilterWindow.IsOpen()))
			pixelFilterWindow.Toggle();
		if (isGPURenderingAvailable() && ImGui::MenuItem("OpenCL Device editor", NULL, oclDeviceWindow.IsOpen(),
				boost::ends_with(currentRenderEngineType, "OCL")))
			oclDeviceWindow.Toggle();
		if (ImGui::MenuItem("Light Strategy editor", NULL, lightStrategyWindow.IsOpen()))
			lightStrategyWindow.Toggle();
		if (ImGui::MenuItem("Accelerator editor", NULL, acceleratorWindow.IsOpen()))
			acceleratorWindow.Toggle();
		if (ImGui::MenuItem("Epsilon editor", NULL, epsilonWindow.IsOpen()))
			epsilonWindow.Toggle();
		ImGui::Separator();
		if (ImGui::MenuItem("Film Radiance Groups editor", NULL, filmRadianceGroupsWindow.IsOpen()))
			filmRadianceGroupsWindow.Toggle();
		if (ImGui::MenuItem("Film Outputs editor", NULL, filmOutputsWindow.IsOpen()))
			filmOutputsWindow.Toggle();
		if (ImGui::MenuItem("Film Channels window", NULL, filmChannelsWindow.IsOpen()))
			filmChannelsWindow.Toggle();
		if (ImGui::MenuItem("Halt conditions", NULL, haltConditionsWindow.IsOpen()))
			haltConditionsWindow.Toggle();
		ImGui::Separator();
		if (session && ImGui::MenuItem("Statistics", "j", statsWindow.IsOpen()))
			statsWindow.Toggle();
	}

	if (ImGui::MenuItem("Log console", NULL, logWindow.IsOpen()))
		logWindow.Toggle();
	if (ImGui::MenuItem("Help", NULL, helpWindow.IsOpen()))
		helpWindow.Toggle();
}

//------------------------------------------------------------------------------
// MainMenuBar
//------------------------------------------------------------------------------

void LuxCoreApp::MainMenuBar() {
	if (ImGui::BeginMainMenuBar()) {
		// This flag is a trick to popup menu bar in front of rendering window
		// after the rendering windows has been opened for the first time.
		if (popupMenuBar) {
			ImGui::SetWindowFocus();
			popupMenuBar = false;
		}
		
		if (ImGui::BeginMenu("Rendering")) {
			MenuRendering();
			ImGui::EndMenu();
		}

		if (session) {
			const string currentEngineType = config->ToProperties().Get("renderengine.type").Get<string>();

			if (ImGui::BeginMenu("Engine")) {
				MenuEngine();
				ImGui::EndMenu();
			}

			if ((!boost::starts_with(currentEngineType, "TILE")) &&
					(!boost::starts_with(currentEngineType, "RT")) &&
					ImGui::BeginMenu("Sampler")) {
				MenuSampler();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Camera")) {
				MenuCamera();
				ImGui::EndMenu();
			}

			if (boost::starts_with(currentEngineType, "TILE") && ImGui::BeginMenu("Tiles")) {
				MenuTiles();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Film")) {
				MenuFilm();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Image Pipelines")) {
				MenuImagePipelines();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Screen")) {
				MenuScreen();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Tool")) {
				MenuTool();
				ImGui::EndMenu();
			}
		}

		if (ImGui::BeginMenu("Window")) {
			MenuWindow();
			ImGui::EndMenu();
		}

		menuBarHeight = ImGui::GetWindowHeight();
		ImGui::EndMainMenuBar();
	}
}
