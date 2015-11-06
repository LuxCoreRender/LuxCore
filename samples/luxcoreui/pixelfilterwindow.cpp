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
#include <boost/format.hpp>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// PixelFilterWindow
//------------------------------------------------------------------------------

PixelFilterWindow::PixelFilterWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Pixel Filter") {
	typeTable
		.Add("NONE", 0)
		.Add("BOX", 1)
		.Add("GAUSSIAN", 2)
		.Add("MITCHELL", 3)
		.Add("MITCHELL_SS", 4)
		.Add("BLACKMANHARRIS", 5)
		.SetDefault("BLACKMANHARRIS");
}

void PixelFilterWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = config->ToProperties().GetAllProperties("film.filter");
	} catch(exception &ex) {
		LA_LOG("PixelFilter parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = config->GetProperties().GetAllProperties("film.filter");
	}
}

void PixelFilterWindow::ParseObjectProperties(const Properties &props) {
	app->RenderConfigParse(props.GetAllProperties("film.filter"));
}

bool PixelFilterWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	//--------------------------------------------------------------------------
	// film.filter.type
	//--------------------------------------------------------------------------

	const string currentSamplerType = props.Get(Property("film.filter.type")(typeTable.GetDefaultTag())).Get<string>();
	int typeIndex = typeTable.GetVal(currentSamplerType);

	if (ImGui::Combo("Pixel Filter type", &typeIndex, typeTable.GetTagList())) {
		props.Clear();

		props << Property("film.filter.type")(typeTable.GetTag(typeIndex));

		return true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("film.filter.type");

	//--------------------------------------------------------------------------
	// Common to all filters
	//--------------------------------------------------------------------------

	if (typeIndex != typeTable.GetVal("NONE")) {
		float fval;

		if (props.IsDefined("film.filter.xwidth") || props.IsDefined("film.filter.ywidth")) {
			fval = props.Get("film.filter.xwidth").Get<float>();
			if (ImGui::SliderFloat("Horizontal radius", &fval, .1f, 3.f)) {
				props.Set(Property("film.filter.xwidth")(fval));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("film.filter.xwidth");

			fval = props.Get("film.filter.ywidth").Get<float>();
			if (ImGui::SliderFloat("Vertical radius", &fval, .1f, 3.f)) {
				props.Set(Property("film.filter.ywidth")(fval));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("film.filter.ywidth");
		} else {
			fval = props.Get("film.filter.width").Get<float>();
			if (ImGui::SliderFloat("Radius", &fval, .1f, 3.f)) {
				props.Set(Property("film.filter.width")(fval));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("film.filter.width");

			if (ImGui::Button("Separate filter horizontal and vertical radius")) {
				props.Set(Property("film.filter.xwidth")(1.5f));
				props.Set(Property("film.filter.ywidth")(2.5f));
				modifiedProps = true;
			}
		}
	}

	//--------------------------------------------------------------------------
	// GAUSSIAN
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("GAUSSIAN")) {
		float fval;

		fval = props.Get("film.filter.gaussian.alpha").Get<float>();
		if (ImGui::SliderFloat("Alpha", &fval, .1f, 10.f)) {
			props.Set(Property("film.filter.gaussian.alpha")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("film.filter.gaussian.alpha");
	}

	return false;
}
