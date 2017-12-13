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
// AcceleratorWindow
//------------------------------------------------------------------------------

AcceleratorWindow::AcceleratorWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Accelerator") {
	typeTable
		.Add("AUTO", 0)
		.Add("BVH", 1)
		.Add("MBVH", 2)
		.Add("QBVH", 3)
		.Add("MQBVH", 4)
		.Add("EMBREE", 5)
		.SetDefault("AUTO");
}

void AcceleratorWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = config->ToProperties().GetAllProperties("accelerator");
	} catch(exception &ex) {
		LA_LOG("Accelerator parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = config->GetProperties().GetAllProperties("accelerator");
	}
}

void AcceleratorWindow::ParseObjectProperties(const Properties &props) {
	app->RenderConfigParse(props.GetAllProperties("accelerator"));
}

bool AcceleratorWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	//------------------------------------------------------------------
	// accelerator.type
	//------------------------------------------------------------------

	const string currentSamplerType = props.Get(Property("accelerator.type")(typeTable.GetDefaultTag())).Get<string>();
	int typeIndex = typeTable.GetVal(currentSamplerType);

	if (ImGui::Combo("Ray/triangle intersection accelerator type", &typeIndex, typeTable.GetTagList())) {
		props.Clear();

		props << Property("accelerator.type")(typeTable.GetTag(typeIndex));

		return true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("accelerator.type");

	bool bval = props.Get("accelerator.instances.enable").Get<float>();
	if (ImGui::Checkbox("Instances support", &bval)) {
		props.Set(Property("accelerator.instances.enable")(bval));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("accelerator.instances.enable");
	
	return false;
}
