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

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// HelpWindow
//------------------------------------------------------------------------------

void HelpWindow::Draw() {
	if (!opened)
		return;

	if (ImGui::Begin(windowTitle.c_str(), &opened)) {
		//----------------------------------------------------------------------
		// GUI help
		//----------------------------------------------------------------------

		if (ImGui::CollapsingHeader("GUI help", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PushID("GUI help");
			ImGui::ShowUserGuide();
			ImGui::PopID();
		}
		//----------------------------------------------------------------------
		// Keys help
		//----------------------------------------------------------------------

		if (ImGui::CollapsingHeader("Keys help", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::PushID("Keys help");

			LuxCoreApp::ColoredLabelText("h", "%s", "toggle Help");
			LuxCoreApp::ColoredLabelText("arrow Keys or mouse X/Y + mouse button 0", "%s", "rotate camera");
			LuxCoreApp::ColoredLabelText("a, s, d, w or mouse X/Y + mouse button 1", "%s", "move camera");
			LuxCoreApp::ColoredLabelText("p", "%s", "save all AOVs");
			LuxCoreApp::ColoredLabelText("shift+p", "%s", "save film.flm");
			LuxCoreApp::ColoredLabelText("i", "%s", "switch sampler");
			LuxCoreApp::ColoredLabelText("n, m", "%s", "dec./inc. the screen refresh");
			LuxCoreApp::ColoredLabelText("1", "%s", "OpenCL path tracing");
			LuxCoreApp::ColoredLabelText("2", "%s", "CPU light tracing");
			LuxCoreApp::ColoredLabelText("3", "%s", "CPU path tracing");
			LuxCoreApp::ColoredLabelText("4", "%s", "CPU bidir. path tracing");
			LuxCoreApp::ColoredLabelText("5", "%s", "CPU bidir. VM path tracing");
			LuxCoreApp::ColoredLabelText("6", "%s", "RT OpenCL path tracing");
			LuxCoreApp::ColoredLabelText("7", "%s", "CPU tile path tracing");
			LuxCoreApp::ColoredLabelText("8", "%s", "OpenCL tile path tracing");
			LuxCoreApp::ColoredLabelText("9", "%s", "RT CPU path tracing");
			LuxCoreApp::ColoredLabelText("Space bar", "%s", "Restart the rendering");

			ImGui::PopID();
		}
	}
	ImGui::End();
}
