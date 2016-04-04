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

#include <imgui.h>
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
		nfdchar_t *outPath = NULL;
		nfdresult_t result = NFD_OpenDialog("cfg;lxs", NULL, &outPath);

		if (result == NFD_OKAY) {
			LoadRenderConfig(outPath);
			free(outPath);
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

		if (session && ImGui::MenuItem("Restart", "Space bar")) {
			// Restart rendering
			session->Stop();
			session->Start();
		}
		
		ImGui::Separator();
	}

#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (ImGui::MenuItem("Fill kernel cache")) {
		if (session) {
			// Stop any current rendering
			DeleteRendering();
		}

		Properties props;
//		props <<
//				Property("opencl.devices.select")("010") <<
//				Property("opencl.code.alwaysenabled")(
//					"MATTE MIRROR GLASS ARCHGLASS MATTETRANSLUCENT GLOSSY2 "
//					"METAL2 ROUGHGLASS ROUGHMATTE ROUGHMATTETRANSLUCENT "
//					"GLOSSYTRANSLUCENT "
//					"GLOSSY2_ABSORPTION GLOSSY2_MULTIBOUNCE "
//					"HOMOGENEOUS_VOL CLEAR_VOL "
//					"IMAGEMAPS_BYTE_FORMAT IMAGEMAPS_HALF_FORMAT "
//					"IMAGEMAPS_1xCHANNELS IMAGEMAPS_3xCHANNELS "
//					"HAS_BUMPMAPS "
//					"INFINITE TRIANGLELIGHT") <<
//				Property("kernelcachefill.renderengine.types")("PATHOCL") <<
//				Property("kernelcachefill.sampler.types")("SOBOL") <<
//				Property("kernelcachefill.camera.types")("perspective") <<
//				Property("kernelcachefill.light.types")("infinite", "trianglelight");
		KernelCacheFill(props, KernelCacheFillProgressHandler);
	}

	ImGui::Separator();
#endif

	if (ImGui::MenuItem("Quit", "ESC"))
		glfwSetWindowShouldClose(window, GL_TRUE);
}

//------------------------------------------------------------------------------
// MenuEngine
//------------------------------------------------------------------------------

void LuxCoreApp::MenuEngine() {
	const string currentEngineType = config->ToProperties().Get("renderengine.type").Get<string>();

	if (ImGui::MenuItem("PATHOCL", "1", (currentEngineType == "PATHOCL"))) {
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
#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (ImGui::MenuItem("RTPATHOCL", "6", (currentEngineType == "RTPATHOCL"))) {
		SetRenderingEngineType("RTPATHOCL");
		CloseAllRenderConfigEditors();
	}
#endif
	if (ImGui::MenuItem("BIASPATHCPU", "7", (currentEngineType == "BIASPATHCPU"))) {
		SetRenderingEngineType("BIASPATHCPU");
		CloseAllRenderConfigEditors();
	}
#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (ImGui::MenuItem("BIASPATHOCL", "8", (currentEngineType == "BIASPATHOCL"))) {
		SetRenderingEngineType("BIASPATHOCL");
		CloseAllRenderConfigEditors();
	}
#endif
#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (ImGui::MenuItem("RTBIASPATHOCL", "9", (currentEngineType == "RTBIASPATHOCL"))) {
		SetRenderingEngineType("RTBIASPATHOCL");
		CloseAllRenderConfigEditors();
	}
#endif
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

		if (ImGui::MenuItem("Use screen resolution")) {
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
// MenuScreen
//------------------------------------------------------------------------------

void LuxCoreApp::MenuImagePipeline() {
	const u_int imagePipelineCount = session->GetFilm().GetChannelCount(Film::CHANNEL_RGB_TONEMAPPED);

	for (u_int i = 0; i < imagePipelineCount; ++i) {
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
}

//------------------------------------------------------------------------------
// MenuTool
//------------------------------------------------------------------------------

void LuxCoreApp::MenuTool() {
	if (ImGui::MenuItem("Camera edit", NULL, (currentTool == TOOL_CAMERA_EDIT)))
		currentTool = TOOL_CAMERA_EDIT;
	if (ImGui::MenuItem("Object selection", NULL, (currentTool == TOOL_OBJECT_SELECTION))) {
		// Check if the session a OBJECT_ID AOV enabled
		if(!session->GetFilm().HasOutput(Film::OUTPUT_OBJECT_ID)) {
			// Enable OBJECT_ID AOV
			RenderConfigParse(
					Properties() <<
						Property("film.outputs.LUXCOREUI_OBJECTSELECTION_AOV.type")("OBJECT_ID") <<
						Property("film.outputs.LUXCOREUI_OBJECTSELECTION_AOV.filename")("dummy.png"));
		}

		currentTool = TOOL_OBJECT_SELECTION;
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
				!boost::starts_with(currentRenderEngineType, "BIAS") && !boost::starts_with(currentRenderEngineType, "RTBIAS")))
			samplerWindow.Toggle();
		if (ImGui::MenuItem("Pixel Filter editor", NULL, pixelFilterWindow.IsOpen()))
			pixelFilterWindow.Toggle();
		if (ImGui::MenuItem("OpenCL Device editor", NULL, oclDeviceWindow.IsOpen(),
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

			if ((!boost::starts_with(currentEngineType, "BIAS")) && (!boost::starts_with(currentEngineType, "RTBIAS")) &&
					ImGui::BeginMenu("Sampler")) {
				MenuSampler();
				ImGui::EndMenu();
			}

			if (boost::starts_with(currentEngineType, "BIAS") && ImGui::BeginMenu("Tiles")) {
				MenuTiles();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Film")) {
				MenuFilm();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Image")) {
				MenuImagePipeline();
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

		ImGui::EndMainMenuBar();
	}
}
