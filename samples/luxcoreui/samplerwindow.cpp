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
// SamplerWindow
//------------------------------------------------------------------------------

void SamplerWindow::Draw() {
	if (!opened)
		return;

	if (ImGui::Begin(title.c_str(), &opened)) {
		RenderConfig *config = app->config;

		// Check if I have fill samplerProps
		if (!samplerProps.GetSize()) {
			// samplerProps is empty

			samplerProps = config->ToProperties().GetAllProperties("sampler");
			modifiedProps = false;

			strncpy(advancedEditor,
					samplerProps.ToString().c_str(),
					LA_ARRAYSIZE(advancedEditor));
			modifiedEditor = false;
		}

		//----------------------------------------------------------------------
		// Sampler properties
		//----------------------------------------------------------------------

		if (ImGui::CollapsingHeader("Sampler properties", NULL, true, true)) {
			//------------------------------------------------------------------
			// sampler.type
			//------------------------------------------------------------------

			const string currentSamplerType = samplerProps.Get("sampler.type").Get<string>();
			int samplerTypeIndex;
			if (currentSamplerType == "SOBOL")
				samplerTypeIndex = 1;
			else if (currentSamplerType == "METROPOLIS")
				samplerTypeIndex = 2;
			else
				samplerTypeIndex = 0;

			if (ImGui::Combo("Sampler type", &samplerTypeIndex, "RANDOM\0SOBOL\0METROPOLIS\0\0")) {
				if (samplerTypeIndex == 1)
					app->EditRenderConfig(Properties() << Property("sampler.type")("SOBOL"));
				else if (samplerTypeIndex == 2)
					app->EditRenderConfig(Properties() << Property("sampler.type")("METROPOLIS"));
				else
					app->EditRenderConfig(Properties() << Property("sampler.type")("RANDOM"));

				// It will be filled at the next iteration
				samplerProps.Clear();

				ImGui::End();
				return;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("sampler.type");

			//------------------------------------------------------------------
			// METROPOLIS
			//
			// At the moment, METROPOLIS is the only sampler with some parameter
			//------------------------------------------------------------------

			if (samplerTypeIndex == 2) {
				int ival;
				float fval;

				fval = samplerProps.Get("sampler.metropolis.largesteprate").Get<float>();
				if (ImGui::InputFloat("Large step rate", &fval)) {
					samplerProps.Set(Property("sampler.metropolis.largesteprate")(fval));
					modifiedProps = true;
				}
				ImGui::SameLine();
				ImGui::TextDisabled("(?)");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("sampler.metropolis.largesteprate");

				ival = samplerProps.Get("sampler.metropolis.maxconsecutivereject").Get<float>();
				if (ImGui::InputInt("Max. consecutive rejections", &ival)) {
					samplerProps.Set(Property("sampler.metropolis.maxconsecutivereject")(ival));
					modifiedProps = true;
				}
				ImGui::SameLine();
				ImGui::TextDisabled("(?)");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("sampler.metropolis.maxconsecutivereject");

				fval = samplerProps.Get("sampler.metropolis.imagemutationrate").Get<float>();
				if (ImGui::InputFloat("Mutation rate", &fval)) {
					samplerProps.Set(Property("sampler.metropolis.imagemutationrate")(fval));
					modifiedProps = true;
				}
				ImGui::SameLine();
				ImGui::TextDisabled("(?)");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("sampler.metropolis.imagemutationrate");
			}

			//------------------------------------------------------------------
			// Refresh + Parse
			//------------------------------------------------------------------

			if (ImGui::Button("Refresh")) {
				// Refresh samplerProps
				samplerProps = config->ToProperties().GetAllProperties("sampler");
				modifiedProps = false;
			}

			ImGui::SameLine();

			if (ImGui::Button(modifiedProps ? "Parse (*)" : "Parse")) {
				// Parse the new sampler definition
				app->EditRenderConfig(samplerProps);

				// It will be filled at the next iteration
				samplerProps.Clear();
			}
		}

		//----------------------------------------------------------------------
		// Advance editing
		//----------------------------------------------------------------------

		if (ImGui::CollapsingHeader("Advance editing", NULL, true, false)) {
			//------------------------------------------------------------------
			// Refresh + Parse
			//------------------------------------------------------------------

			if (ImGui::InputTextMultiline("Advance editor", advancedEditor, LA_ARRAYSIZE(advancedEditor)))
				modifiedEditor = true;

			//------------------------------------------------------------------
			// Refresh + Parse
			//------------------------------------------------------------------

			if (ImGui::Button("Refresh")) {
				// Refresh samplerProps
				strncpy(advancedEditor,
						config->ToProperties().GetAllProperties("sampler").ToString().c_str(),
						LA_ARRAYSIZE(advancedEditor));
				modifiedEditor = false;
			}

			ImGui::SameLine();

			if (ImGui::Button(modifiedEditor ? "Parse (*)" : "Parse")) {
				Properties props;
				props.SetFromString(string(advancedEditor));

				// Parse the new sampler definition
				app->EditRenderConfig(props);

				// It will be filled at the next iteration
				samplerProps.Clear();
			}
		}
	}
	ImGui::End();
}
