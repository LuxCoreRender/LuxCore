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
// OCLDeviceWindow
//------------------------------------------------------------------------------

OCLDeviceWindow::OCLDeviceWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "OpenCL devices") {
}

Properties OCLDeviceWindow::GetOpenCLDeviceProperties(const Properties &cfgProps) const {
	Properties props = 
			cfgProps.GetAllProperties("opencl.platform") <<
			cfgProps.GetAllProperties("opencl.cpu") <<
			cfgProps.GetAllProperties("opencl.gpu") <<
			cfgProps.GetAllProperties("opencl.devices.select");

	return props;
}

void OCLDeviceWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = GetOpenCLDeviceProperties(config->ToProperties());
	} catch(exception &ex) {
		LA_LOG("OCLDevice parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = GetOpenCLDeviceProperties(config->GetProperties());
	}
}

void OCLDeviceWindow::ParseObjectProperties(const Properties &props) {
	app->RenderConfigParse(GetOpenCLDeviceProperties(props));
}

bool OCLDeviceWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	int oclPlatformIndex = props.Get("opencl.platform.index").Get<int>();
	if (ImGui::InputInt("OpenCL platform index", &oclPlatformIndex)) {
		props.Set(Property("opencl.platform.index")(oclPlatformIndex));
		modifiedProps = true;
	}
	LuxCoreApp::HelpMarker("opencl.platform.index");

	try {
		// Get the list of intersection devices
		const Properties oclDevDescs = GetOpenCLDeviceDescs();
		const vector<string> oclDevDescPrefixs = oclDevDescs.GetAllUniqueSubNames("opencl.device");

		// Get the device selection string
		string selection = props.Get("opencl.devices.select").Get<string>();
		if (selection.length() != oclDevDescPrefixs.size()) {
			const bool useCPU = props.Get("opencl.cpu.use").Get<bool>();
			const bool useGPU = props.Get("opencl.gpu.use").Get<bool>();

			selection.resize(oclDevDescPrefixs.size(), '0');
			for (unsigned int i = 0; i < oclDevDescPrefixs.size(); ++i) {
				const string devType = oclDevDescs.Get(oclDevDescPrefixs[i] + ".type").Get<string>();

				if ((useCPU && (devType == "OPENCL_CPU")) ||
						(useGPU && ((devType == "OPENCL_GPU") || (devType == "CUDA_GPU"))))
					selection.at(i) = '1';
			}

			modifiedProps = true;
		}

		if (ImGui::CollapsingHeader("General", NULL, true, true)) {
			int ival;
			bool bval;

			bval = props.Get("opencl.cpu.use").Get<bool>();
			if (ImGui::Checkbox("Use all CPU devices", &bval)) {
				props.Set(Property("opencl.cpu.use")(bval));
				modifiedProps = true;

				for (unsigned int i = 0; i < oclDevDescPrefixs.size(); ++i) {
					const string devType = oclDevDescs.Get(oclDevDescPrefixs[i] + ".type").Get<string>();

					if (devType == "OPENCL_CPU")
						selection.at(i) = bval ? '1' : '0';;
				}
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("opencl.cpu.use");

			bval = props.Get("opencl.gpu.use").Get<bool>();
			if (ImGui::Checkbox("Use all GPU devices", &bval)) {
				props.Set(Property("opencl.gpu.use")(bval));
				modifiedProps = true;

				for (unsigned int i = 0; i < oclDevDescPrefixs.size(); ++i) {
					const string devType = oclDevDescs.Get(oclDevDescPrefixs[i] + ".type").Get<string>();

					if ((devType == "OPENCL_GPU") || (devType == "CUDA_GPU"))
						selection.at(i) = bval ? '1' : '0';;
				}
			}
			LuxCoreApp::HelpMarker("opencl.gpu.use");

			ival = props.Get("opencl.cpu.workgroup.size").Get<int>();
			if (ImGui::InputInt("Workgroup size for CPU devices", &ival)) {
				props.Set(Property("opencl.cpu.workgroup.size")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("opencl.cpu.workgroup.size");

			ival = props.Get("opencl.gpu.workgroup.size").Get<int>();
			if (ImGui::InputInt("Workgroup size for GPU devices", &ival)) {
				props.Set(Property("opencl.gpu.workgroup.size")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("opencl.gpu.workgroup.size");
		}

		if (ImGui::CollapsingHeader("Device list", NULL, true, true)) {
			// Show the device list
			for (unsigned int i = 0; i < oclDevDescPrefixs.size(); ++i) {
				const string devName = oclDevDescs.Get(oclDevDescPrefixs[i] + ".name").Get<string>();
				const string devType = oclDevDescs.Get(oclDevDescPrefixs[i] + ".type").Get<string>();

				bool bval = (selection.at(i) == '1');

				ImGui::SetNextTreeNodeOpened(true, ImGuiSetCond_Appearing);
				if (ImGui::TreeNode(("#" + ToString(i) + " => " + devName +
						(bval ? " (Used)" : " (Not used)")).c_str())) {
					LuxCoreApp::ColoredLabelText("Name:", "%s", devName.c_str());

					ImGui::SameLine();
					if (ImGui::Checkbox("Use", &bval)) {
						selection.at(i) = bval ? '1' : '0';
						modifiedProps = true;
					}
					LuxCoreApp::HelpMarker("opencl.devices.select");

					LuxCoreApp::ColoredLabelText("Type:", "%s", devType.c_str());
					ImGui::TreePop();
				}
			}
		}

		if (modifiedProps)
			props.Set(Property("opencl.devices.select")(selection));
	} catch(exception &ex) {
		// A not valid OpenCL platform index
		LA_LOG("Wrong OpenCL platform index: " << oclPlatformIndex << endl << ex.what());

		props.Set(Property("opencl.platform.index")(-1));
		props.Set(Property("opencl.cpu.use")(false));
		props.Set(Property("opencl.cpu.use")(true));
		props.Set(Property("opencl.devices.select")(""));
		modifiedProps = true;

		return true;
	}

	return false;
}
