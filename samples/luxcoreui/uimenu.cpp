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

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// MenuFile
//------------------------------------------------------------------------------

void LuxCoreApp::MenuFile() {
	if (ImGui::MenuItem("Restart", "Space bar")) {
		// Restart rendering
		session->Stop();
		session->Start();
	}
	if (ImGui::MenuItem("Quit", "ESC"))
		glfwSetWindowShouldClose(window, GL_TRUE);
}

//------------------------------------------------------------------------------
// MenuFilm
//------------------------------------------------------------------------------

void LuxCoreApp::MenuFilm() {
	if (ImGui::MenuItem("Save outputs"))
		session->GetFilm().SaveOutputs();
	if (ImGui::MenuItem("Save film"))
		session->GetFilm().SaveFilm("film.flm");
}

//------------------------------------------------------------------------------
// MainMenuBar
//------------------------------------------------------------------------------

void LuxCoreApp::MainMenuBar() {
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Rendering")) {
			MenuFile();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Film")) {
			MenuFilm();
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}
