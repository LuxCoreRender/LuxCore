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
// EpsilonWindow
//------------------------------------------------------------------------------

EpsilonWindow::EpsilonWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Epsilon") {
}

Properties EpsilonWindow::GetEpsilonProperties(const Properties &cfgProps) const {
	return cfgProps.GetAllProperties("scene.epsilon");
}

void EpsilonWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = GetEpsilonProperties(config->ToProperties());
	} catch(exception &ex) {
		LA_LOG("Epsilon parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = GetEpsilonProperties(config->GetProperties());
	}
}

void EpsilonWindow::ParseObjectProperties(const Properties &props) {
	app->RenderConfigParse(GetEpsilonProperties(props));
}

bool EpsilonWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	float fval;

	fval = props.Get("scene.epsilon.min").Get<float>();
	if (ImGui::InputFloat("Min. epsilon value", &fval, 0.f, 0.f, "%.15f")) {
		props.Set(Property("scene.epsilon.min")(fval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("scene.epsilon.min");

	fval = props.Get("scene.epsilon.max").Get<float>();
	if (ImGui::InputFloat("Max. epsilon value", &fval, 0.f, 0.f, "%.15f")) {
		props.Set(Property("scene.epsilon.max")(fval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("scene.epsilon.max");

	return false;
}
