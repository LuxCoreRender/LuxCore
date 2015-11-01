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
// OCLDeviceWindow
//------------------------------------------------------------------------------

OCLDeviceWindow::OCLDeviceWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "OpenCL devices") {
}

void OCLDeviceWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = config->ToProperties().GetAllProperties("opencl.devices.select");
	} catch(exception &ex) {
		LA_LOG("OCLDevice parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = config->GetProperties().GetAllProperties("opencl.devices.select");
	}
}

void OCLDeviceWindow::ParseObjectProperties(const Properties &props) {
	app->EditRenderConfig(props.GetAllProperties("opencl.devices.select"));
}

bool OCLDeviceWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	luxrays::Context ctx;
	vector<luxrays::DeviceDescription *> deviceDescriptions = ctx.GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_OPENCL_ALL, deviceDescriptions);

	string selection = props.Get("opencl.devices.select").Get<string>();
	if (selection.length() != deviceDescriptions.size()) {
		selection.resize(deviceDescriptions.size(), '1');
		modifiedProps = true;
	}

	for (u_int i = 0; i < deviceDescriptions.size(); ++i) {
		luxrays::DeviceDescription *desc = deviceDescriptions[i];

		bool bval = (selection.at(i) == '1');
		ImGui::SetNextTreeNodeOpened(true, ImGuiSetCond_Appearing);
		if (ImGui::TreeNode(("#" + ToString(i) + " => " + desc->GetName() +
				(bval ? " (Used)" : " (Not used)")).c_str())) {
			luxrays::DeviceDescription *desc = deviceDescriptions[i];
			LuxCoreApp::ColoredLabelText("Name:", "%s", desc->GetName().c_str());

			ImGui::SameLine();
			if (ImGui::Checkbox("Use", &bval)) {
				selection.at(i) = bval ? '1' : '0';
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("opencl.devices.select");

			LuxCoreApp::ColoredLabelText("Type:", "%s", luxrays::DeviceDescription::GetDeviceType(desc->GetType()).c_str());
			ImGui::TreePop();
		}
	}

	if (modifiedProps)
		props.Set(Property("opencl.devices.select")(selection));

	return false;
}
