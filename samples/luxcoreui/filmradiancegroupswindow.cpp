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
// FilmRadianceGroupsWindow
//------------------------------------------------------------------------------

FilmRadianceGroupsWindow::FilmRadianceGroupsWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Film Radiance Group(s)") {
}

Properties FilmRadianceGroupsWindow::GetFilmRadianceGroupsProperties(const Properties &cfgProps) const {
	return cfgProps.GetAllProperties("film.imagepipelines." + ToString(app->imagePipelineIndex) + ".radiancescales");
}

void FilmRadianceGroupsWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = GetFilmRadianceGroupsProperties(config->ToProperties());
	} catch(exception &ex) {
		LA_LOG("FilmRadianceGroupsWindow parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = GetFilmRadianceGroupsProperties(config->GetProperties());
	}
}

void FilmRadianceGroupsWindow::ParseObjectProperties(const Properties &props) {
	app->RenderSessionParse(GetFilmRadianceGroupsProperties(props));
}

bool FilmRadianceGroupsWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	const unsigned int radianceGroupCount = app->session->GetFilm().GetRadianceGroupCount();

	for (unsigned int radianceGroupIndex = 0; radianceGroupIndex < radianceGroupCount; ++radianceGroupIndex) {
		const string radianceGroupIndexStr = ToString(radianceGroupIndex);
		ImGui::PushID(radianceGroupIndexStr.c_str());

		ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
		if (ImGui::TreeNode(("Radiance group: #" + radianceGroupIndexStr).c_str())) {
			// Check the properties include a definition for this group

			const string prefix = "film.imagepipelines." + ToString(app->imagePipelineIndex) + ".radiancescales." + radianceGroupIndexStr;

			//------------------------------------------------------------------
			// Enable widget
			//------------------------------------------------------------------

			bool enabled = props.Get(Property(prefix + ".enabled")(true)).Get<bool>();
			const bool enabledChanged = ImGui::Checkbox("Enabled", &enabled);
			LuxCoreApp::HelpMarker((prefix + ".globalscale").c_str());

			if (enabledChanged) {
				props.Set(Property(prefix + ".enabled")(enabled));
				modifiedProps = true;
			}

			if (enabled) {
				//--------------------------------------------------------------
				// Global scale widget
				//--------------------------------------------------------------

				float globalScale = props.Get(Property(prefix + ".globalscale")(1.f)).Get<float>();
				if (ImGui::InputFloat("Global scale", &globalScale)) {
					props.Set(Property(prefix + ".globalscale")(globalScale));
					modifiedProps = true;
				}
				LuxCoreApp::HelpMarker((prefix + ".globalscale").c_str());

				//--------------------------------------------------------------
				// Temperature widget
				//--------------------------------------------------------------

				float temperature = props.Get(Property(prefix + ".temperature")(0.f)).Get<float>();
				bool showTemperature = (temperature > 0.f);
				if (ImGui::Checkbox("Use temperature", &showTemperature)) {
					temperature = showTemperature ? 6500.f : 0.f;
					props.Set(Property(prefix + ".temperature")(temperature));
					modifiedProps = true;
				}

				if (showTemperature) {
					if (ImGui::SliderFloat("Temperature", &temperature, 1000.f, 11000.f)) {
						props.Set(Property(prefix + ".temperature")(temperature));
						modifiedProps = true;
					}
				}

				LuxCoreApp::HelpMarker((prefix + ".temperature").c_str());

				//--------------------------------------------------------------
				// RGB widget
				//--------------------------------------------------------------

				if (!showTemperature) {
					float rgbScale[3];
					const Property p = props.Get(Property(prefix + ".rgbscale")(1.f, 1.f, 1.f));
					rgbScale[0] = p.Get<float>(0);
					rgbScale[1] = p.Get<float>(1);
					rgbScale[2] = p.Get<float>(2);

					if (ImGui::ColorEdit3("RGB scale", rgbScale)) {
						props.Set(Property(prefix + ".rgbscale")(rgbScale[0], rgbScale[1], rgbScale[2]));
						modifiedProps = true;
					}

					LuxCoreApp::HelpMarker((prefix + ".rgbscale").c_str());
				}
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	return modifiedProps;
}
