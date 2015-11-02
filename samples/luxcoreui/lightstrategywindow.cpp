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
// LightStrategyWindow
//------------------------------------------------------------------------------

LightStrategyWindow::LightStrategyWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Light Strategy") {
	typeTable
		.Add("UNIFORM", 0)
		.Add("POWER", 1)
		.Add("LOG_POWER", 2)
		.SetDefault("LOG_POWER");
}

void LightStrategyWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = config->ToProperties().GetAllProperties("lightstrategy");
	} catch(exception &ex) {
		LA_LOG("LightStrategy parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = config->GetProperties().GetAllProperties("lightstrategy");
	}
}

void LightStrategyWindow::ParseObjectProperties(const Properties &props) {
	app->EditRenderConfig(props.GetAllProperties("lightstrategy"));
}

bool LightStrategyWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	//------------------------------------------------------------------
	// lightstrategy.type
	//------------------------------------------------------------------

	const string currentSamplerType = props.Get(Property("lightstrategy.type")(typeTable.GetDefaultTag())).Get<string>();
	int typeIndex = typeTable.GetVal(currentSamplerType);

	if (ImGui::Combo("Light Strategy type", &typeIndex, typeTable.GetTagList())) {
		props.Clear();

		props << Property("lightstrategy.type")(typeTable.GetTag(typeIndex));

		return true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("lightstrategy.type");

	return false;
}
