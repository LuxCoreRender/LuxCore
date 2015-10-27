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
// RenderEngineWindow
//------------------------------------------------------------------------------

RenderEngineWindow::RenderEngineWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Render Engine") {
	typeTable
		.Add("PATHCPU", 0)
		.Add("BIDIRCPU", 1)
		.SetDefault("PATHCPU");
}

Properties RenderEngineWindow::GetAllRenderEngineProperties(const Properties &cfgProps) const {
	return
			cfgProps.GetAllProperties("renderengine") <<
			cfgProps.GetAllProperties("native.threads") <<
			cfgProps.GetAllProperties("path") <<
			cfgProps.GetAllProperties("light");
}

void RenderEngineWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = GetAllRenderEngineProperties(config->ToProperties());
	} catch(exception &ex) {
		LA_LOG("RenderEngine parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = GetAllRenderEngineProperties(config->GetProperties());
	}
}

void RenderEngineWindow::ParseObjectProperties(const Properties &props) {
	app->EditRenderConfig(GetAllRenderEngineProperties(props));
}

bool RenderEngineWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	//------------------------------------------------------------------
	// renderengine.type
	//------------------------------------------------------------------

	const string currentRenderEngineType = props.Get(Property("renderengine.type")(typeTable.GetDefaultTag())).Get<string>();
	int typeIndex = typeTable.GetVal(currentRenderEngineType);

	if (ImGui::Combo("Render Engine type", &typeIndex, typeTable.GetTagList())) {
		props.Clear();

		props << Property("renderengine.type")(typeTable.GetTag(typeIndex));

		return true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("renderengine.type");

	//------------------------------------------------------------------
	// PATHCPU
	//------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("PATHCPU")) {
		bool bval;
		float fval;
		int ival;

		ival = props.Get("path.maxdepth").Get<int>();
		if (ImGui::SliderInt("Maximum recursion depth", &ival, 1, 32)) {
			props.Set(Property("path.maxdepth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.maxdepth");

		ival = props.Get("path.russianroulette.depth").Get<int>();
		if (ImGui::SliderInt("Russian Roulette start depth", &ival, 1, 32)) {
			props.Set(Property("path.russianroulette.depth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.russianroulette.depth");

		fval = props.Get("path.russianroulette.cap").Get<float>();
		if (ImGui::SliderFloat("Russian Roulette threshold", &fval, 0.f, 1.f)) {
			props.Set(Property("path.russianroulette.cap")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.russianroulette.cap");

		fval = props.Get("path.clamping.variance.maxvalue").Get<float>();
		if (ImGui::InputFloat("Variance clamping", &fval)) {
			props.Set(Property("path.clamping.variance.maxvalue")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.clamping.variance.maxvalue");

		fval = props.Get("path.clamping.pdf.value").Get<float>();
		if (ImGui::InputFloat("PDF clamping", &fval)) {
			props.Set(Property("path.clamping.pdf.value")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.clamping.pdf.value");

		bval = props.Get("path.fastpixelfilter.enable").Get<float>();
		if (ImGui::Checkbox("Use fast pixel filter", &bval)) {
			props.Set(Property("path.fastpixelfilter.enable")(bval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.clamping.pdf.value");

		ival = props.Get("native.threads.count").Get<int>();
		if (ImGui::SliderInt("Threads count", &ival, 1, boost::thread::hardware_concurrency())) {
			props.Set(Property("native.threads.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("native.threads.count");

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.opened = true;
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.opened = true;
	}

	//------------------------------------------------------------------
	// BIDIRCPU
	//------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("BIDIRCPU")) {
		float fval;
		int ival;

		ival = props.Get("path.maxdepth").Get<int>();
		if (ImGui::SliderInt("Maximum eye path recursion depth", &ival, 1, 32)) {
			props.Set(Property("path.maxdepth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.maxdepth");

		ival = props.Get("light.maxdepth").Get<int>();
		if (ImGui::SliderInt("Maximum light path recursion depth", &ival, 1, 32)) {
			props.Set(Property("light.maxdepth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("light.maxdepth");

		ival = props.Get("path.russianroulette.depth").Get<int>();
		if (ImGui::SliderInt("Russian Roulette start depth", &ival, 1, 32)) {
			props.Set(Property("path.russianroulette.depth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.russianroulette.depth");

		fval = props.Get("path.russianroulette.cap").Get<float>();
		if (ImGui::SliderFloat("Russian Roulette threshold", &fval, 0.f, 1.f)) {
			props.Set(Property("path.russianroulette.cap")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.russianroulette.cap");

		ival = props.Get("native.threads.count").Get<int>();
		if (ImGui::SliderInt("Threads count", &ival, 1, boost::thread::hardware_concurrency())) {
			props.Set(Property("native.threads.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("native.threads.count");

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.opened = true;
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.opened = true;
	}

	return false;
}
