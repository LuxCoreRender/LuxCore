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
#include <memory>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/unordered_set.hpp>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// FilmOutputWindow
//------------------------------------------------------------------------------

FilmOutputWindow::FilmOutputWindow(LuxCoreApp *a, const string &title,
			const luxcore::Film::FilmOutputType t, const unsigned int i) :
	ImageWindow(a, title), type(t), index(i) {
	glGenTextures(1, &channelTexID);
}

FilmOutputWindow::~FilmOutputWindow() {
	glDeleteTextures(1, &channelTexID);
}

void FilmOutputWindow::CopyAlpha(const float *filmPixels, float *pixels,
		const unsigned int filmWidth, const unsigned int filmHeight) const {
	for (unsigned int y = 0; y < filmHeight; ++y) {
		for (unsigned int x = 0; x < filmWidth; ++x) {
			const unsigned int index = x + y * filmWidth;
			const float *src = &filmPixels[index * 4];
			float *dst = &pixels[index * 3];

			float band[3];
			if ((x % 16 <= 1) || (y % 16 <= 1)) {
				band[0] = 0.f;
				band[1] = 0.f;
				band[2] = 0.f;
			} else {
				band[0] = 1.f;
				band[1] = 1.f;
				band[2] = 1.f;
			}

			const float k = 1.f - src[3];
			dst[0] = Lerp(k, src[0], band[0]);
			dst[1] = Lerp(k, src[1], band[1]);
			dst[2] = Lerp(k, src[2], band[2]);
		}
	}
}

void FilmOutputWindow::RefreshTexture() {
	app->session->UpdateStats();

	const unsigned int filmWidth = app->session->GetFilm().GetWidth();
	const unsigned int filmHeight = app->session->GetFilm().GetHeight();
	
	unique_ptr<float[]> pixels(new float[filmWidth * filmHeight * 3]);
	switch (type) {
		case Film::OUTPUT_RGB:
		case Film::OUTPUT_POSITION:
		case Film::OUTPUT_GEOMETRY_NORMAL:
		case Film::OUTPUT_SHADING_NORMAL:
		case Film::OUTPUT_DIRECT_DIFFUSE:
		case Film::OUTPUT_DIRECT_DIFFUSE_REFLECT:
		case Film::OUTPUT_DIRECT_DIFFUSE_TRANSMIT:
		case Film::OUTPUT_DIRECT_GLOSSY:
		case Film::OUTPUT_DIRECT_GLOSSY_REFLECT:
		case Film::OUTPUT_DIRECT_GLOSSY_TRANSMIT:
		case Film::OUTPUT_EMISSION:
		case Film::OUTPUT_INDIRECT_DIFFUSE:
		case Film::OUTPUT_INDIRECT_DIFFUSE_REFLECT:
		case Film::OUTPUT_INDIRECT_DIFFUSE_TRANSMIT:
		case Film::OUTPUT_INDIRECT_GLOSSY:
		case Film::OUTPUT_INDIRECT_GLOSSY_REFLECT:
		case Film::OUTPUT_INDIRECT_GLOSSY_TRANSMIT:
		case Film::OUTPUT_INDIRECT_SPECULAR:
		case Film::OUTPUT_INDIRECT_SPECULAR_REFLECT:
		case Film::OUTPUT_INDIRECT_SPECULAR_TRANSMIT:
		case Film::OUTPUT_RADIANCE_GROUP:
		case Film::OUTPUT_BY_MATERIAL_ID:
		case Film::OUTPUT_IRRADIANCE:
		case Film::OUTPUT_BY_OBJECT_ID:
		case Film::OUTPUT_AVG_SHADING_NORMAL:
		case Film::OUTPUT_CAUSTIC: {
			app->session->GetFilm().GetOutput<float>(type, pixels.get(), index);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::OUTPUT_RGBA: {
			unique_ptr<float[]> filmPixels;
			filmPixels.reset(new float[app->session->GetFilm().GetOutputSize(type)]);
			app->session->GetFilm().GetOutput<float>(type, filmPixels.get(), index);
			
			CopyAlpha(filmPixels.get(), pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::OUTPUT_MATERIAL_ID_COLOR:
		case Film::OUTPUT_ALBEDO:
		case Film::OUTPUT_RGB_IMAGEPIPELINE: {
			app->session->GetFilm().GetOutput<float>(type, pixels.get(), index);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::OUTPUT_RGBA_IMAGEPIPELINE: {
			unique_ptr<float[]> filmPixels;
			filmPixels.reset(new float[app->session->GetFilm().GetOutputSize(type)]);
			app->session->GetFilm().GetOutput<float>(type, filmPixels.get(), index);
			
			CopyAlpha(filmPixels.get(), pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::OUTPUT_ALPHA:
		case Film::OUTPUT_DEPTH:
		case Film::OUTPUT_MATERIAL_ID_MASK:
		case Film::OUTPUT_DIRECT_SHADOW_MASK:
		case Film::OUTPUT_INDIRECT_SHADOW_MASK:
		case Film::OUTPUT_RAYCOUNT:
		case Film::OUTPUT_OBJECT_ID_MASK:
		case Film::OUTPUT_CONVERGENCE:
		case Film::OUTPUT_NOISE: {
			unique_ptr<float[]> filmPixels;
			filmPixels.reset(new float[app->session->GetFilm().GetOutputSize(type)]);
			app->session->GetFilm().GetOutput<float>(type, filmPixels.get(), index);
			
			Copy1(filmPixels.get(), pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::OUTPUT_MATERIAL_ID:
		case Film::OUTPUT_OBJECT_ID: {
			unique_ptr<unsigned int> filmPixels;
			filmPixels.reset(new unsigned int[app->session->GetFilm().GetOutputSize(type)]);
			app->session->GetFilm().GetOutput<unsigned int>(type, filmPixels.get(), index);

			Copy1UINT(filmPixels.get(), pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::OUTPUT_SAMPLECOUNT: {
			unique_ptr<unsigned int> filmPixels;
			filmPixels.reset(new unsigned int[app->session->GetFilm().GetOutputSize(type)]);
			app->session->GetFilm().GetOutput<unsigned int>(type, filmPixels.get(), index);

			Copy1UINT2FLOAT(filmPixels.get(), pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::OUTPUT_UV: {
			unique_ptr<float[]> filmPixels;
			filmPixels.reset(new float[app->session->GetFilm().GetOutputSize(type)]);
			app->session->GetFilm().GetOutput<float>(type, filmPixels.get(), index);

			Copy2(filmPixels.get(), pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		default:
			throw runtime_error("Unknown film channel type in FilmOutputWindow::RefreshTexture(): " + ToString(type));
	}
	
	glBindTexture(GL_TEXTURE_2D, channelTexID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, filmWidth, filmHeight, 0, GL_RGB, GL_FLOAT, pixels.get());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, app->renderFrameBufferTexMinFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, app->renderFrameBufferTexMagFilter);
}

//------------------------------------------------------------------------------
// FilmOutputsWindow
//------------------------------------------------------------------------------

FilmOutputsWindow::FilmOutputsWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Film Outputs") {
	typeTable
		.Add("RGB", 0)
		.Add("RGBA", 1)
		.Add("RGB_IMAGEPIPELINE", 2)
		.Add("RGBA_IMAGEPIPELINE", 3)
		.Add("ALPHA", 4)
		.Add("DEPTH", 5)
		.Add("POSITION", 6)
		.Add("GEOMETRY_NORMAL", 7)
		.Add("SHADING_NORMAL", 8)
		.Add("MATERIAL_ID", 9)
		.Add("DIRECT_DIFFUSE", 10)
		.Add("DIRECT_DIFFUSE_REFLECT", 11)
		.Add("DIRECT_DIFFUSE_TRANSMIT", 12)
		.Add("DIRECT_GLOSSY", 13)
		.Add("DIRECT_GLOSSY_REFLECT", 14)
		.Add("DIRECT_GLOSSY_TRANSMIT", 15)
		.Add("EMISSION", 16)
		.Add("INDIRECT_DIFFUSE", 17)
		.Add("INDIRECT_DIFFUSE_REFLECT", 18)
		.Add("INDIRECT_DIFFUSE_TRANSMIT", 19)
		.Add("INDIRECT_GLOSSY", 20)
		.Add("INDIRECT_GLOSSY_REFLECT", 21)
		.Add("INDIRECT_GLOSSY_TRANSMIT", 22)
		.Add("INDIRECT_SPECULAR", 23)
		.Add("INDIRECT_SPECULAR_REFLECT", 24)
		.Add("INDIRECT_SPECULAR_TRANSMIT", 25)
		.Add("MATERIAL_ID_MASK", 26)
		.Add("DIRECT_SHADOW_MASK", 27)
		.Add("INDIRECT_SHADOW_MASK", 28)
		.Add("RADIANCE_GROUP", 29)
		.Add("UV", 30)
		.Add("RAYCOUNT", 31)
		.Add("BY_MATERIAL_ID", 32)
		.Add("IRRADIANCE", 33)
		.Add("OBJECT_ID", 34)
		.Add("OBJECT_ID_MASK", 35)
		.Add("BY_OBJECT_ID", 36)
		.Add("SAMPLECOUNT", 37)
		.Add("CONVERGENCE", 38)
	//OUTPUT_SERIALIZED_FILM = 39
		.Add("MATERIAL_ID_COLOR", 40)
		.Add("ALBEDO", 41)
		.Add("AVG_SHADING_NORMAL", 42)
		.Add("NOISE", 43)
		.Add("CAUSTIC", 44)
		.SetDefault("RGB");

	newType = 0;
	newOutputNameBuff[0] = '\0';
	newFileNameBuff[0] = '\0';
	newFileType = 0;
	newID = 0;
}

FilmOutputsWindow::~FilmOutputsWindow() {
	DeleteAllWindow();
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

void FilmOutputsWindow::Close() {
	if (opened) {
		DeleteAllWindow();

		ObjectEditorWindow::Close();
	}
}

void FilmOutputsWindow::DeleteAllWindow() {
	BOOST_FOREACH(FilmOutputWindowMap::value_type e, filmOutputWindows)
		DeleteWindow(e.first);
}

void FilmOutputsWindow::DeleteWindow(const string &key) {
	delete filmOutputWindows[key];
	filmOutputWindows.erase(key);
}

void FilmOutputsWindow::DeleteWindow(const luxcore::Film::FilmOutputType type,
		const unsigned int index) {
	DeleteWindow(GetKey(type, index));
}

string FilmOutputsWindow::GetKey(const luxcore::Film::FilmOutputType type,
		const unsigned int index) const {
	return ToString(type) + "#" + ToString(index);
}

void FilmOutputsWindow::DrawShowCheckBox(const string &label,
		const Film::FilmOutputType type, const unsigned int index) {
	const string key = GetKey(type, index);

	// Erase closed windows
	if ((filmOutputWindows.count(key) > 0) && (!filmOutputWindows[key]->IsOpen()))
		DeleteWindow(key);

	bool show = (filmOutputWindows.count(key) > 0);
	if (ImGui::Checkbox("Show", &show)) {
		if (show) {
			FilmOutputWindow *ow = new FilmOutputWindow(app,
					label + " #" + ToString(index) + " window", type, index);
			ow->Open();
			filmOutputWindows[key] = ow;
		} else
			DeleteWindow(key);
	}
}

bool FilmOutputsWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	//--------------------------------------------------------------------------
	// To add a new output
	//--------------------------------------------------------------------------

	if (ImGui::CollapsingHeader("New Film output", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
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
		static string imageExts[4] = {
			".exr",
			".hdr",
			".png",
			".jpg"
		};
		string imageExt;

		ImGui::InputText("", newFileNameBuff, 4 * 1024);
		ImGui::SameLine();
		const string tag = typeTable.GetTag(newType);
		if ((tag == "RGB_IMAGEPIPELINE") ||
				(tag == "RGBA_IMAGEPIPELINE") ||
				(tag == "ALPHA") ||
				(tag == "MATERIAL_ID_MASK") ||
				(tag == "DIRECT_SHADOW_MASK") ||
				(tag == "INDIRECT_SHADOW_MASK") ||
				(tag == "RADIANCE_GROUP") ||
				(tag == "UV") ||
				(tag == "RAYCOUNT") ||
				(tag == "BY_MATERIAL_ID") ||
				(tag == "IRRADIANCE") ||
				(tag == "OBJECT_ID_MASK") ||
				(tag == "BY_OBJECT_ID") ||
				(tag == "SAMPLECOUNT") ||
				(tag == "CONVERGENCE") ||
				(tag == "MATERIAL_ID_COLOR") ||
				(tag == "ALBEDO") ||
				(tag == "NOISE")) {
			ImGui::Combo("File name", &newFileType, "EXR\0HDR\0PNG\0JPG\0\0");
			imageExt = imageExts[newFileType];
		} else if ((tag == "MATERIAL_ID") ||
				(tag == "OBJECT_ID")) {
			ImGui::Combo("File name", &newFileType, "PNG\0JPG\0\0");
			imageExt = imageExts[newFileType + 2];
		} else {
			ImGui::Combo("File name", &newFileType, "EXR\0HDR\0\0");
			imageExt = imageExts[newFileType];
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", (prefix + ".filename").c_str());

		// Film output (optional) ids
		if ((tag == "MATERIAL_ID_MASK") || (tag == "BY_MATERIAL_ID") ||
				(tag == "OBJECT_ID_MASK") || (tag == "BY_OBJECT_ID")) {
			ImGui::InputInt("ID", &newID);
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", (prefix + ".id").c_str());
		} else if (tag == "RADIANCE_GROUP") {
			const unsigned int count = app->session->GetFilm().GetRadianceGroupCount();
			string list;
			for (unsigned int i = 0; i < count; ++i) {
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
			props <<
					Property(prefix + ".type")(tag) <<
					Property(prefix + ".filename")(newOutputName + imageExt);
			if ((tag == "MATERIAL_ID_MASK") ||
				(tag == "RADIANCE_GROUP") ||
				(tag == "BY_MATERIAL_ID") ||
				(tag == "OBJECT_ID_MASK") ||
				(tag == "BY_OBJECT_ID"))
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

	if (ImGui::CollapsingHeader("Current Film output(s)", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
		boost::unordered_set<string> outputNames;
		boost::unordered_map<string, unsigned int> typeCount;
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

			if (typeCount.find(type) == typeCount.end())
				typeCount[type] = 0;
			else
				typeCount[type] += 1;
			const unsigned int index = typeCount[type];

			ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
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
						(type == "BY_MATERIAL_ID") ||
						(type == "OBJECT_ID_MASK") ||
						(type == "BY_OBJECT_ID")) {
					int id = props.Get("film.outputs." + outputName + ".id").Get<int>();
					if (ImGui::InputInt("ID", &id)) {
						props.Set(Property("film.outputs." + outputName + ".id")(id));
						modifiedProps = true;
					}
				}

				if (!modifiedProps)
					DrawShowCheckBox(type, (Film::FilmOutputType)typeTable.GetVal(type), index);
				else
					DeleteAllWindow();

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

	if (ImGui::CollapsingHeader("Current Film channel(s)", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
		const Film &film = app->session->GetFilm();
		unsigned int count;

		count = film.GetChannelCount(Film::CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_ALPHA);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_ALPHA:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_IMAGEPIPELINE);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_IMAGEPIPELINE:", "%d", count);

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

		count = film.GetChannelCount(Film::CHANNEL_OBJECT_ID);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_OBJECT_ID:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_OBJECT_ID_MASK);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_OBJECT_ID_MASK:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_BY_OBJECT_ID);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_BY_OBJECT_ID:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_SAMPLECOUNT);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_SAMPLECOUNT:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_CONVERGENCE);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_CONVERGENCE:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_MATERIAL_ID_COLOR);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_MATERIAL_ID_COLOR:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_ALBEDO);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_ALBEDO:", "%d", count);

		count = film.GetChannelCount(Film::CHANNEL_AVG_SHADING_NORMAL);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_AVG_SHADING_NORMAL:", "%d", count);
		
		count = film.GetChannelCount(Film::CHANNEL_NOISE);
		if (count)
			LuxCoreApp::ColoredLabelText("CHANNEL_NOISE:", "%d", count);
	}

	return false;
}

void FilmOutputsWindow::Draw() {
	ObjectEditorWindow::Draw();

	if (opened) {
		// Draw all channel windows
		BOOST_FOREACH(FilmOutputWindowMap::value_type e, filmOutputWindows)
			e.second->Draw();
	} else
		DeleteAllWindow();
}
