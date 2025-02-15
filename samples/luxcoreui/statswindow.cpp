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
#include <boost/format.hpp>
#include <boost/foreach.hpp>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// StatsWindow
//------------------------------------------------------------------------------

void StatsWindow::Draw() {
	if (!opened)
		return;

	ImGui::SetNextWindowSize(ImVec2(512.f, 200.f), ImGuiCond_Appearing);

	if (ImGui::Begin(windowTitle.c_str(), &opened)) {
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);

		RenderSession *session = app->session;
		RenderConfig *config = app->config;

		const Properties &stats = session->GetStats();

		// GUI information
		if (ImGui::CollapsingHeader("GUI information", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			LuxCoreApp::ColoredLabelText("GUI loop time:", "%fms (%fms)",
					1000.0 * app->guiLoopTimeShortAvg, 1000.0 * app->guiLoopTimeLongAvg);
			LuxCoreApp::ColoredLabelText("GUI sleep time:", "%fms", 1000.0 * app->guiSleepTime);
			LuxCoreApp::ColoredLabelText("Film update time:", "%fms", 1000.0 * app->guiFilmUpdateTime);
			LuxCoreApp::ColoredLabelText("GUI RT dropped frames count:", "%d (Refresh decoupling = %d)",
					app->droppedFramesCount, app->refreshDecoupling);
		}

		// Rendering information
		if (ImGui::CollapsingHeader("Rendering information", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			const string engineType = config->ToProperties().Get("renderengine.type").Get<string>();
			LuxCoreApp::ColoredLabelText("Render engine:", "%s", engineType.c_str());

			const string samplerName = ((engineType == "TILEPATHCPU") || (engineType == "TILEPATHOCL") ||
				(engineType == "RTPATHOCL")) ?
					"N/A" : config->ToProperties().Get("sampler.type").Get<string>();
			LuxCoreApp::ColoredLabelText("Sampler:", "%s", samplerName.c_str());

			LuxCoreApp::ColoredLabelText("Rendering time:", "%dsecs", int(stats.Get("stats.renderengine.time").Get<double>()));
			LuxCoreApp::ColoredLabelText("Film resolution:", "%d x %d", session->GetFilm().GetWidth(), session->GetFilm().GetHeight());
			int frameBufferWidth, frameBufferHeight;
			glfwGetFramebufferSize(app->window, &frameBufferWidth, &frameBufferHeight);
			LuxCoreApp::ColoredLabelText("Screen resolution:", "%d x %d", frameBufferWidth, frameBufferHeight);
			
#if defined(LUXRAYS_ENABLE_OPENCL)
			if (engineType == "RTPATHOCL") {
				static float fps = 0.f;
				// This is a simple trick to smooth the fps counter
				const double frameTime = stats.Get("stats.rtpathocl.frame.time").Get<double>();
				const double adjustFactor = (frameTime > 0.1) ? 0.25 : .025;
				fps = Lerp<float>(adjustFactor, fps, (frameTime > 0.0) ? (1.0 / frameTime) : 0.0);

				LuxCoreApp::ColoredLabelText("Screen refresh:", "%d/%dms (%.1ffps)",
						int((fps > 0.f) ? (1000.0 / fps) : 0.0),
						config->ToProperties().Get("screen.refresh.interval").Get<unsigned int>(),
						fps);
			} else if (engineType == "RTPATHOCL") {
				static float fps = 0.f;
				// This is a simple trick to smooth the fps counter
				const double frameTime = stats.Get("stats.rtpathocl.frame.time").Get<double>();
				const double adjustFactor = (frameTime > 0.1) ? 0.25 : .025;
				fps = Lerp<float>(adjustFactor, fps, (frameTime > 0.0) ? (1.0 / frameTime) : 0.0);

				LuxCoreApp::ColoredLabelText("Screen refresh:", "%d/%dms (%.1ffps)",
						int((fps > 0.f) ? (1000.0 / fps) : 0.0),
						config->ToProperties().Get("screen.refresh.interval").Get<unsigned int>(),
						fps);
			} else
#endif
			{
				LuxCoreApp::ColoredLabelText("Screen refresh:", "%dms", config->ToProperties().Get("screen.refresh.interval").Get<unsigned int>());
			}
			
			const float convergence = stats.Get("stats.renderengine.convergence").Get<float>();
			LuxCoreApp::ColoredLabelText("Convergence:", "%f%%", 100.f * convergence);
		}

		if (ImGui::CollapsingHeader("Intersection devices used", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			// Intersection devices
			const Property &deviceNames = stats.Get("stats.renderengine.devices");

			double minPerf = numeric_limits<double>::infinity();
			double totalPerf = 0.0;
			for (unsigned int i = 0; i < deviceNames.GetSize(); ++i) {
				const string deviceName = deviceNames.Get<string>(i);

				const double perf = stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>();
				minPerf = Min(minPerf, perf);
				totalPerf += perf;
			}

			for (unsigned int i = 0; i < deviceNames.GetSize(); ++i) {
				const string deviceName = deviceNames.Get<string>(i);

				LuxCoreApp::ColoredLabelText((deviceName + ":").c_str(), "[Rays/sec %dK (%dK + %dK)][Prf Idx %.2f][Wrkld %.1f%%][Mem %dM/%dM]",
					int(stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>() / 1000.0),
					int(stats.Get("stats.renderengine.devices." + deviceName + ".performance.serial").Get<double>() / 1000.0),
					int(stats.Get("stats.renderengine.devices." + deviceName + ".performance.dataparallel").Get<double>() / 1000.0),
					stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>() / minPerf,
					100.0 * stats.Get("stats.renderengine.devices." + deviceName + ".performance.total").Get<double>() / totalPerf,
					int(stats.Get("stats.renderengine.devices." + deviceName + ".memory.used").Get<double>() / (1024 * 1024)),
					int(stats.Get("stats.renderengine.devices." + deviceName + ".memory.total").Get<double>() / (1024 * 1024)));

			}
		}

		if (ImGui::CollapsingHeader("Intersection devices available", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			const Properties props = GetOpenCLDeviceDescs();
			const vector<string>  prefixs = props.GetAllUniqueSubNames("opencl.device");

			// Print device info
			unsigned int i = 0;
			BOOST_FOREACH(const string &prefix, prefixs) {
				if (ImGui::TreeNode(("#" + ToString(i) + " => " + props.Get(prefix + ".name").Get<string>()).c_str())) {
					LuxCoreApp::ColoredLabelText("Name:", "%s", props.Get(prefix + ".name").Get<string>().c_str());
					LuxCoreApp::ColoredLabelText("Type:", "%s", props.Get(prefix + ".type").Get<string>().c_str());
					LuxCoreApp::ColoredLabelText("Compute units:", "%d", props.Get(prefix + ".units").Get<unsigned int>());
					LuxCoreApp::ColoredLabelText("Preferred float vector width:", "%u", props.Get(prefix + ".nativevectorwidthfloat").Get<unsigned int>());
					LuxCoreApp::ColoredLabelText("Max. allocable memory:", "%luMBytes", (unsigned long)(props.Get(prefix + ".maxmemory").Get<unsigned long long>() / (1024 * 1024)));
					LuxCoreApp::ColoredLabelText("Max. allocable memory block size:", "%luMBytes", (unsigned long)(props.Get(prefix + ".maxmemoryallocsize").Get<unsigned long long>() / (1024 * 1024)));
					ImGui::TreePop();
				}
			}
		}

		ImGui::PopStyleVar();
	}
	ImGui::End();
}
