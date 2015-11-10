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
// FilmOutputsWindow
//------------------------------------------------------------------------------

FilmOutputsWindow::FilmOutputsWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Film Outputs") {
	typeTable
		.Add("RGB", 0)
		.Add("RGBA", 1)
		.Add("ALPHA", 2)
		.Add("DEPTH", 3)
		.Add("POSITION", 4)
		.Add("GEOMETRY_NORMAL", 5)
		.Add("SHADING_NORMAL", 6)
		.Add("MATERIAL_ID", 7)
		.Add("DIRECT_DIFFUSE", 8)
		.Add("DIRECT_GLOSSY", 9)
		.Add("EMISSION", 10)
		.Add("INDIRECT_DIFFUSE", 11)
		.Add("INDIRECT_GLOSSY", 12)
		.Add("INDIRECT_SPECULAR", 13)
		.Add("MATERIAL_ID_MASK", 14)
		.Add("DIRECT_SHADOW_MASK", 15)
		.Add("INDIRECT_SHADOW_MASK", 16)
		.Add("RADIANCE_GROUP", 17)
		.Add("UV", 18)
		.Add("RAYCOUNT", 19)
		.Add("BY_MATERIAL_ID", 20)
		.Add("IRRADIANCE", 21)
		.SetDefault("RGB");

	newType = 0;
	newOutputNameBuff[0] = '\0';
	newFileNameBuff[0] = '\0';
	newFileType = 0;
	newID = 0;
}

Properties FilmOutputsWindow::GetFilmOutputsProperties(const Properties &cfgProps) const {
	return cfgProps.GetAllProperties("film.outputs");
}

void FilmOutputsWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = GetFilmOutputsProperties(config->ToProperties());
	} catch(exception &ex) {
		LA_LOG("FilmOutputs parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = GetFilmOutputsProperties(config->GetProperties());
	}
}

void FilmOutputsWindow::ParseObjectProperties(const Properties &props) {
	app->RenderConfigParse(GetFilmOutputsProperties(props));
}

bool FilmOutputsWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	//--------------------------------------------------------------------------
	// To add a new output
	//--------------------------------------------------------------------------

	if (ImGui::CollapsingHeader("New Film output", NULL, true, true)) {
		// Film output name
		ImGui::InputText("Output name", newOutputNameBuff, 4 * 1024);
		const string prefix = "film.outputs." + string(newOutputNameBuff);

		// Film output type
		ImGui::Combo("Film output type", &newType, typeTable.GetTagList());
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", (prefix + ".type").c_str());

		// Film output file name
		ImGui::InputText("", newFileNameBuff, 4 * 1024);
		ImGui::SameLine();
		const string tag = typeTable.GetTag(newType);
		if ((tag == "RGB_TONEMAPPED") ||
				(tag == "RGBA_TONEMAPPED") ||
				(tag == "ALPHA") ||
				(tag == "MATERIAL_ID_MASK") ||
				(tag == "DIRECT_SHADOW_MASK") ||
				(tag == "INDIRECT_SHADOW_MASK") ||
				(tag == "RADIANCE_GROUP") ||
				(tag == "UV") ||
				(tag == "RAYCOUNT") ||
				(tag == "BY_MATERIAL_ID") ||
				(tag == "IRRADIANCE"))
			ImGui::Combo("File name", &newFileType, "EXR\0HDR\0PNG\0JPG\0\0");
		else
			ImGui::Combo("File name", &newFileType, "EXR\0HDR\0\0");
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", (prefix + ".filename").c_str());

		// Film output (optional) ids
		if ((tag == "MATERIAL_ID_MASK") || (tag == "BY_MATERIAL_ID")) {
			ImGui::InputInt("ID", &newID);
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", (prefix + ".id").c_str());
		} else if (tag == "RADIANCE_GROUP") {
			const u_int count = app->session->GetFilm().GetRadianceGroupCount();
			string list;
			for (u_int i = 0; i < count; ++i) {
				list.append(ToString(i));
				list.push_back(0);
			}
			list.push_back(0);
			
			ImGui::Combo("Group", &newID, list.c_str());
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", (prefix + ".id").c_str());
		}

		// Add button
		const string newOutputName(newOutputNameBuff);
		if (ImGui::Button("Add") && (newOutputName.length() > 0)) {
			static string exts[4] = {
				".exr",
				".hdr",
				".png",
				".jpg"
			};

			props <<
					Property(prefix + ".type")(tag) <<
					Property(prefix + ".filename")(newOutputName + exts[newFileType]);
			if ((tag == "MATERIAL_ID_MASK") ||
				(tag == "RADIANCE_GROUP") ||
				(tag == "BY_MATERIAL_ID"))
				props << Property(prefix + ".id")(newID);

			newType = 0;
			newOutputNameBuff[0] = '\0';
			newFileNameBuff[0] = '\0';
			newFileType = 0;
			newID = 0;
			modifiedProps = true;
		}
	}

	//--------------------------------------------------------------------------
	// The list of current outputs
	//--------------------------------------------------------------------------

	if (ImGui::CollapsingHeader("Current Film output(s)", NULL, true, true)) {
		set<string> outputNames;
		vector<string> outputKeys = props.GetAllNames("film.outputs.");
		for (vector<string>::const_iterator outputKey = outputKeys.begin(); outputKey != outputKeys.end(); ++outputKey) {
			const string &key = *outputKey;
			const size_t dot1 = key.find(".", string("film.outputs.").length());
			if (dot1 == string::npos)
				continue;

			// Extract the output type name
			const string outputName = Property::ExtractField(key, 2);
			if (outputName == "") {
				LA_LOG("Syntax error in film output definition: " << outputName);
				continue;
			}

			if (outputNames.count(outputName) > 0)
				continue;

			outputNames.insert(outputName);
			
			ImGui::PushID(outputName.c_str());

			const string type = props.Get("film.outputs." + outputName + ".type").Get<string>();
			ImGui::SetNextTreeNodeOpened(true);
			if (ImGui::TreeNode(type.c_str())) {
				const string fileName = props.Get("film.outputs." + outputName + ".filename").Get<string>();
				char fileNameBuff[4 * 1024];
				const size_t len = fileName.copy(fileNameBuff, 4 * 1024);
				fileNameBuff[len] = '\0';
				if (ImGui::InputText("File name", fileNameBuff, 4 * 1024)) {
					props.Set(Property("film.outputs." + outputName + ".filename")(string(fileNameBuff)));
					modifiedProps = true;
				}
				
				if ((type == "MATERIAL_ID_MASK") ||
						(type == "RADIANCE_GROUP") ||
						(type == "BY_MATERIAL_ID")) {
					int id = props.Get("film.outputs." + outputName + ".id").Get<int>();
					if (ImGui::InputInt("ID", &id)) {
						props.Set(Property("film.outputs." + outputName + ".id")(id));
						modifiedProps = true;
					}
				}

				if (ImGui::Button("Remove")) {
					props.DeleteAll(props.GetAllNames("film.outputs." + outputName));
					modifiedProps = true;
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}
	}

	//--------------------------------------------------------------------------
	// The list of current film channels
	//--------------------------------------------------------------------------

	if (ImGui::CollapsingHeader("Current Film channel(s)", NULL, true, true)) {
		const Film &film = app->session->GetFilm();
		u_int count;

		count = film.GetChannelCount(Film::CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_ALPHA);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_ALPHA:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_RGB_TONEMAPPED);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_RGB_TONEMAPPED:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_DEPTH);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_DEPTH:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_POSITION);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_POSITION:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_GEOMETRY_NORMAL);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_GEOMETRY_NORMAL:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_SHADING_NORMAL);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_SHADING_NORMAL:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_MATERIAL_ID);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_MATERIAL_ID:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_DIRECT_DIFFUSE);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_DIRECT_DIFFUSE:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_DIRECT_GLOSSY);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_DIRECT_GLOSSY:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_EMISSION);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_EMISSION:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_INDIRECT_DIFFUSE);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_INDIRECT_DIFFUSE:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_INDIRECT_GLOSSY);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_INDIRECT_GLOSSY:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_INDIRECT_SPECULAR);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_INDIRECT_SPECULAR:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_MATERIAL_ID_MASK);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_MATERIAL_ID_MASK:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_DIRECT_SHADOW_MASK);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_DIRECT_SHADOW_MASK:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_INDIRECT_SHADOW_MASK);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_INDIRECT_SHADOW_MASK:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_UV);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_UV:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_RAYCOUNT);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_RAYCOUNT:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_BY_MATERIAL_ID);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_BY_MATERIAL_ID:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_IRRADIANCE);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_IRRADIANCE:", "%d", count);
	}

	return false;
}
