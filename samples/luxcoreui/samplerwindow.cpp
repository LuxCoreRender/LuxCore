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
// SamplerWindow
//------------------------------------------------------------------------------

SamplerWindow::SamplerWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Sampler") {
	typeTable
		.Add("RANDOM", 0)
		.Add("SOBOL", 1)
		.Add("METROPOLIS", 2)
		.SetDefault("SOBOL");
}

void SamplerWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = config->ToProperties().GetAllProperties("sampler");
	} catch(exception &ex) {
		LA_LOG("Sampler parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = config->GetProperties().GetAllProperties("sampler");
	}
}

void SamplerWindow::ParseObjectProperties(const Properties &props) {
	app->RenderConfigParse(props.GetAllProperties("sampler"));
}

bool SamplerWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	//--------------------------------------------------------------------------
	// sampler.type
	//--------------------------------------------------------------------------

	const string currentSamplerType = props.Get(Property("sampler.type")(typeTable.GetDefaultTag())).Get<string>();
	int typeIndex = typeTable.GetVal(currentSamplerType);
	if (ImGui::Combo("Sampler type", &typeIndex, typeTable.GetTagList())) {
		props.Clear();

		props << Property("sampler.type")(typeTable.GetTag(typeIndex));

		return true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("sampler.type");

	//--------------------------------------------------------------------------
	// RANDOM
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("RANDOM")) {
		float fval = props.Get("sampler.sobol.adaptive.strength").Get<float>();
		if (ImGui::SliderFloat("Adaptive strength", &fval, 0.f, .95f)) {
			props.Set(Property("sampler.sobol.adaptive.strength")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("sampler.sobol.adaptive.strength");
	}

	//--------------------------------------------------------------------------
	// SOBOL
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("SOBOL")) {
		float fval = props.Get("sampler.sobol.adaptive.strength").Get<float>();
		if (ImGui::SliderFloat("Adaptive strength", &fval, 0.f, .95f)) {
			props.Set(Property("sampler.sobol.adaptive.strength")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("sampler.sobol.adaptive.strength");
	}

	//--------------------------------------------------------------------------
	// METROPOLIS
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("METROPOLIS")) {
		int ival;
		float fval;

		fval = props.Get("sampler.metropolis.largesteprate").Get<float>();
		if (ImGui::SliderFloat("Large step rate", &fval, 0.f, 1.f)) {
			props.Set(Property("sampler.metropolis.largesteprate")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("sampler.metropolis.largesteprate");

		ival = props.Get("sampler.metropolis.maxconsecutivereject").Get<float>();
		if (ImGui::SliderInt("Max. consecutive rejections", &ival, 0, 32768)) {
			props.Set(Property("sampler.metropolis.maxconsecutivereject")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("sampler.metropolis.maxconsecutivereject");

		fval = props.Get("sampler.metropolis.imagemutationrate").Get<float>();
		if (ImGui::SliderFloat("Mutation rate", &fval, 0.f, 1.f)) {
			props.Set(Property("sampler.metropolis.imagemutationrate")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("sampler.metropolis.imagemutationrate");
	}

	return false;
}
