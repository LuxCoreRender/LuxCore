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
// HaltConditionsWindow
//------------------------------------------------------------------------------

HaltConditionsWindow::HaltConditionsWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Halt conditions") {
}

void HaltConditionsWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = config->ToProperties().GetAllProperties("batch.halt");
	} catch(exception &ex) {
		LA_LOG("Halt conditions parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = config->GetProperties().GetAllProperties("batch.halt");
	}
}

void HaltConditionsWindow::ParseObjectProperties(const Properties &props) {
	app->RenderConfigParse(props.GetAllProperties("batch.halt"));
}

bool HaltConditionsWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	if (ImGui::CollapsingHeader("Halt threshold", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
		float fval = props.Get("batch.haltnoisethreshold").Get<float>();
		if (ImGui::InputFloat("Convergence threshold", &fval)) {
			props.Set(Property("batch.haltnoisethreshold")(fval));
			modifiedProps = true;
		}

		int ival = props.Get("batch.haltnoisethreshold.warmup").Get<int>();
		if (ImGui::InputInt("Warm up samples/pixel", &ival)) {
			props.Set(Property("batch.haltnoisethreshold.warmup")(ival));
			modifiedProps = true;
		}

		ival = props.Get("batch.haltnoisethreshold.step").Get<int>();
		if (ImGui::InputInt("Samples/pixel steps", &ival)) {
			props.Set(Property("batch.haltnoisethreshold.step")(ival));
			modifiedProps = true;
		}
		
		bool bval = props.Get("batch.haltnoisethreshold.filter.enable").Get<bool>();
		if (ImGui::Checkbox("Use box filter for CONVERGENCE AOV", &bval)) {
			props.Set(Property("batch.haltnoisethreshold.filter.enable")(bval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("batch.haltnoisethreshold.filter.enable");

		bval = props.Get("batch.haltnoisethreshold.stoprendering.enable").Get<bool>();
		if (ImGui::Checkbox("Stop the rendering", &bval)) {
			props.Set(Property("batch.haltnoisethreshold.stoprendering.enable")(bval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("batch.haltnoisethreshold.stoprendering.enable");
	}
	
	if (ImGui::CollapsingHeader("Halt time", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
		float fval = props.Get("batch.halttime").Get<float>();
		if (ImGui::InputFloat("Total time in secs", &fval)) {
			props.Set(Property("batch.halttime")(fval));
			modifiedProps = true;
		}
	}
	
	if (ImGui::CollapsingHeader("Halt samples/pixel", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
		int ival = props.Get("batch.haltspp").Get<int>();
		if (ImGui::InputInt("Total samples/pixel", &ival)) {
			props.Set(Property("batch.haltspp")(ival));
			modifiedProps = true;
		}
	}
	
	return false;
}
