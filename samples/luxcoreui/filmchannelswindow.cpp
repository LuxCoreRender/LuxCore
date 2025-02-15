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
#include <limits>
#include <boost/format.hpp>
#include <boost/foreach.hpp>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// FilmChannelWindow
//------------------------------------------------------------------------------

FilmChannelWindow::FilmChannelWindow(LuxCoreApp *a, const string &title,
			const luxcore::Film::FilmChannelType t, const unsigned int i) :
	ImageWindow(a, title), type(t), index(i) {
	glGenTextures(1, &channelTexID);
}

FilmChannelWindow::~FilmChannelWindow() {
	glDeleteTextures(1, &channelTexID);
}

void FilmChannelWindow::RefreshTexture() {
	app->session->UpdateStats();

	const unsigned int filmWidth = app->session->GetFilm().GetWidth();
	const unsigned int filmHeight = app->session->GetFilm().GetHeight();
	
	unique_ptr<float[]> pixels(new float[filmWidth * filmHeight * 3]);
	switch (type) {
		case Film::CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED:
		case Film::CHANNEL_DIRECT_DIFFUSE:
		case Film::CHANNEL_DIRECT_DIFFUSE_REFLECT:
		case Film::CHANNEL_DIRECT_DIFFUSE_TRANSMIT:
		case Film::CHANNEL_DIRECT_GLOSSY:
		case Film::CHANNEL_DIRECT_GLOSSY_REFLECT:
		case Film::CHANNEL_DIRECT_GLOSSY_TRANSMIT:
		case Film::CHANNEL_EMISSION:
		case Film::CHANNEL_INDIRECT_DIFFUSE:
		case Film::CHANNEL_INDIRECT_DIFFUSE_REFLECT:
		case Film::CHANNEL_INDIRECT_DIFFUSE_TRANSMIT:
		case Film::CHANNEL_INDIRECT_GLOSSY:
		case Film::CHANNEL_INDIRECT_GLOSSY_REFLECT:
		case Film::CHANNEL_INDIRECT_GLOSSY_TRANSMIT:
		case Film::CHANNEL_INDIRECT_SPECULAR:
		case Film::CHANNEL_INDIRECT_SPECULAR_REFLECT:
		case Film::CHANNEL_INDIRECT_SPECULAR_TRANSMIT:
		case Film::CHANNEL_BY_MATERIAL_ID:
		case Film::CHANNEL_IRRADIANCE:
		case Film::CHANNEL_BY_OBJECT_ID: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			Normalize3(filmPixels, pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(filmPixels, pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_ALPHA:
		case Film::CHANNEL_MATERIAL_ID_MASK:
		case Film::CHANNEL_DIRECT_SHADOW_MASK:
		case Film::CHANNEL_INDIRECT_SHADOW_MASK:
		case Film::CHANNEL_OBJECT_ID_MASK:{
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			Normalize1(filmPixels, pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_IMAGEPIPELINE: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);
			Copy3(filmPixels, pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_DEPTH:
		case Film::CHANNEL_RAYCOUNT:
		case Film::CHANNEL_CONVERGENCE:
		case Film::CHANNEL_NOISE: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			Copy1(filmPixels, pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_POSITION:
		case Film::CHANNEL_GEOMETRY_NORMAL:
		case Film::CHANNEL_SHADING_NORMAL: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			UpdateStats(filmPixels, filmWidth, filmHeight);
			AutoLinearToneMap(filmPixels, pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_MATERIAL_ID:
		case Film::CHANNEL_OBJECT_ID: {
			const unsigned int *filmPixels = app->session->GetFilm().GetChannel<unsigned int>(type, index);

			Copy1UINT(filmPixels, pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_UV: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			Copy2(filmPixels, pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_SAMPLECOUNT: {
			const unsigned int *filmPixels = app->session->GetFilm().GetChannel<unsigned int>(type, index);

			Copy1UINT2FLOAT(filmPixels, pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_MATERIAL_ID_COLOR:
		case Film::CHANNEL_ALBEDO:
		case Film::CHANNEL_AVG_SHADING_NORMAL: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			Normalize3(filmPixels, pixels.get(), filmWidth, filmHeight);
			UpdateStats(pixels.get(), filmWidth, filmHeight);
			break;
		}
		default:
			throw runtime_error("Unknown film channel type in FilmChannelWindow::RefreshTexture(): " + ToString(type));
	}
	
	glBindTexture(GL_TEXTURE_2D, channelTexID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, filmWidth, filmHeight, 0, GL_RGB, GL_FLOAT, pixels.get());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, app->renderFrameBufferTexMinFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, app->renderFrameBufferTexMagFilter);
}

//------------------------------------------------------------------------------
// FilmChannelsWindow
//------------------------------------------------------------------------------

FilmChannelsWindow::FilmChannelsWindow(LuxCoreApp *a) :
	ObjectWindow(a, "Film Channels window") {
}

FilmChannelsWindow::~FilmChannelsWindow() {
	DeleteAllWindow();
}

void FilmChannelsWindow::Open() {
	denoiserProps.clear();
	
	ObjectWindow::Open();
}

void FilmChannelsWindow::Close() {
	if (opened) {
		DeleteAllWindow();

		ObjectWindow::Close();
	}
}

void FilmChannelsWindow::DeleteAllWindow() {
	// First close all windows
	BOOST_FOREACH(FilmChannelWindowMap::value_type e, filmChannelWindows)
		delete filmChannelWindows[e.first];
	// Than I can clear the map
	filmChannelWindows.clear();
}

void FilmChannelsWindow::DeleteWindow(const string &key) {
	delete filmChannelWindows[key];
	filmChannelWindows.erase(key);
}

string FilmChannelsWindow::GetKey(const luxcore::Film::FilmChannelType type,
		const unsigned int index) const {
	return ToString(type) + "#" + ToString(index);
}

void FilmChannelsWindow::DrawShowCheckBox(const string &label,
		const Film::FilmChannelType type, const unsigned int index) {
	const string key = GetKey(type, index);

	// Erase closed windows
	if ((filmChannelWindows.count(key) > 0) && (!filmChannelWindows[key]->IsOpen()))
		DeleteWindow(key);

	bool show = (filmChannelWindows.count(key) > 0);
	if (ImGui::Checkbox(("Show channel #" + ToString(index)).c_str(), &show)) {
		if (show) {
			FilmChannelWindow *cw = new FilmChannelWindow(app,
					label + " #" + ToString(index) + " window",type, index);
			cw->Open();
			filmChannelWindows[key] = cw;
		} else
			DeleteWindow(key);
	}
}

bool FilmChannelsWindow::HasDenoiser(const u_int index, string &denoiserPrefix) const {
	const Properties &cfgProps = app->config->ToProperties();

	vector<string> typeProps = cfgProps.GetAllNamesRE("film\\.imagepipelines\\." + ToString(index) + ".[0-9]+\\.type");

	BOOST_FOREACH(string &typeProp, typeProps) {
		if (cfgProps.Get(typeProp).Get<string>() == "BCD_DENOISER") {
			// 5 = ".type".length()
			denoiserPrefix = typeProp.substr(0, typeProp.length() - 5);
			return true;
		}
	}
	
	return false;
}

void FilmChannelsWindow::DrawChannelInfo(const string &label, const Film::FilmChannelType type) {
	const Film &film = app->session->GetFilm();
	unsigned int count = film.GetChannelCount(type);
	
	if ((type == Film::CHANNEL_IMAGEPIPELINE) && (denoiserProps.size() != count))
		denoiserProps.resize(count);

	if (count > 0) {
		ImGui::PushID(label.c_str());
		if (ImGui::CollapsingHeader((label + ": " + ToString(count) + " channel(s)").c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) {
			for (unsigned int i = 0; i < count; ++i) {
				DrawShowCheckBox(label, type, i);
				
				// Some special option for image pipeline channel
				string denoiserPrefix;
				if ((type == Film::CHANNEL_IMAGEPIPELINE) && HasDenoiser(i, denoiserPrefix)) {
					if (ImGui::TreeNode(("Denoiser options " + ToString(i)).c_str())) {
						ImGui::PushID("Denoiser options properties");
						ImGui::PushItemWidth(ImGui::GetWindowSize().x / 3);

						Properties &props = denoiserProps[i];
						if (props.GetSize() == 0) {
							const Properties &cfgProps = app->config->ToProperties();
							props = cfgProps.GetAllProperties(denoiserPrefix);
						}

						bool bval = props.Get(Property(denoiserPrefix + ".applydenoise")(true)).Get<bool>();
						if (ImGui::Checkbox("Apply denoise", &bval))
						props << Property(denoiserPrefix + ".applydenoise")(bval);
						LuxCoreApp::HelpMarker((denoiserPrefix + ".applydenoise").c_str());

						bval = props.Get(Property(denoiserPrefix + ".filterspikes")(false)).Get<bool>();
						if (ImGui::Checkbox("Filter spikes", &bval))
						props << Property(denoiserPrefix + ".filterspikes")(bval);
						LuxCoreApp::HelpMarker((denoiserPrefix + ".filterspikes").c_str());
						
						if (bval) {
							float fval = Max(props.Get(Property(denoiserPrefix + ".spikestddev")(2.f)).Get<float>(), 0.f);
							if (ImGui::InputFloat("Spike threshold", &fval))
								props << Property(denoiserPrefix + ".spikestddev")(fval);
							LuxCoreApp::HelpMarker((denoiserPrefix + ".spikestddev").c_str());
						}

						float fval = Max(props.Get(Property(denoiserPrefix + ".histdistthresh")(1.f)).Get<float>(), 0.f);
						if (ImGui::InputFloat("Histogram distance threshold", &fval))
							props << Property(denoiserPrefix + ".histdistthresh")(fval);
						LuxCoreApp::HelpMarker((denoiserPrefix + ".histdistthresh").c_str());

						int ival = Max(props.Get(Property(denoiserPrefix + ".patchradius")(1)).Get<int>(), 1);
						if (ImGui::InputInt("Patch search radius", &ival))
							props << Property(denoiserPrefix + ".patchradius")(ival);
						LuxCoreApp::HelpMarker((denoiserPrefix + ".patchradius").c_str());
						
						ival = Max(props.Get(Property(denoiserPrefix + ".searchwindowradius")(6)).Get<int>(), 1);
						if (ImGui::InputInt("Search window radius", &ival))
							props << Property(denoiserPrefix + ".searchwindowradius")(ival);
						LuxCoreApp::HelpMarker((denoiserPrefix + ".searchwindowradius").c_str());

						fval = Max(props.Get(Property(denoiserPrefix + ".mineigenvalue")(1.e-8f)).Get<float>(), 0.f);
						if (ImGui::InputFloat("Min. eigen value", &fval))
							props << Property(denoiserPrefix + ".mineigenvalue")(fval);
						LuxCoreApp::HelpMarker((denoiserPrefix + ".mineigenvalue").c_str());
						
						bval = props.Get(Property(denoiserPrefix + ".userandompixelorder")(true)).Get<bool>();
						if (ImGui::Checkbox("Use random pixel order", &bval))
							props << Property(denoiserPrefix + ".userandompixelorder")(bval);
						LuxCoreApp::HelpMarker((denoiserPrefix + ".userandompixelorder").c_str());

						fval = Clamp(props.Get(Property(denoiserPrefix + ".markedpixelsskippingprobability")(1.f)).Get<float>(), 0.f, 1.f);
						if (ImGui::InputFloat("Marked pixel skipping probability", &fval))
							props << Property(denoiserPrefix + ".markedpixelsskippingprobability")(fval);
						LuxCoreApp::HelpMarker((denoiserPrefix + ".markedpixelsskippingprobability").c_str());
						
						ival = Max(props.Get(Property(denoiserPrefix + ".threadcount")(0)).Get<int>(), 0);
						if (ImGui::InputInt("Thread count", &ival))
							props << Property(denoiserPrefix + ".threadcount")(ival);
						LuxCoreApp::HelpMarker((denoiserPrefix + ".threadcount").c_str());

						ival = Max(props.Get(Property(denoiserPrefix + ".scales")(3)).Get<int>(), 1);
						if (ImGui::InputInt("Scales", &ival))
							props << Property(denoiserPrefix + ".scales")(ival);
						LuxCoreApp::HelpMarker((denoiserPrefix + ".scales").c_str());
						
						if (ImGui::Button("Apply")) {
							const Properties &cfgProps = app->config->ToProperties();
							const Properties newImagePipelineProps =
								cfgProps.GetAllProperties(Property::PopPrefix(denoiserPrefix)) <<
								props;
							app->session->Parse(newImagePipelineProps);

							// Check if I have to refresh the channel window
							const string windowKey = GetKey(type, i);
							if (filmChannelWindows.count(windowKey) > 0)
								filmChannelWindows[windowKey]->Refresh();
						}

						ImGui::TreePop();

						ImGui::PopItemWidth();
						ImGui::PopID();
					}
				}
			}
		}

		ImGui::PopID();
	}
}

void FilmChannelsWindow::Draw() {
	if (!opened)
		return;

	if (ImGui::Begin(windowTitle.c_str(), &opened)) {
		DrawChannelInfo("CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED", Film::CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED);
		DrawChannelInfo("CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED", Film::CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED);
		DrawChannelInfo("CHANNEL_ALPHA", Film::CHANNEL_ALPHA);
		DrawChannelInfo("CHANNEL_IMAGEPIPELINE", Film::CHANNEL_IMAGEPIPELINE);
		DrawChannelInfo("CHANNEL_DEPTH", Film::CHANNEL_DEPTH);
		DrawChannelInfo("CHANNEL_POSITION", Film::CHANNEL_POSITION);
		DrawChannelInfo("CHANNEL_GEOMETRY_NORMAL", Film::CHANNEL_GEOMETRY_NORMAL);
		DrawChannelInfo("CHANNEL_SHADING_NORMAL", Film::CHANNEL_SHADING_NORMAL);
		DrawChannelInfo("CHANNEL_MATERIAL_ID", Film::CHANNEL_MATERIAL_ID);
		DrawChannelInfo("CHANNEL_DIRECT_DIFFUSE", Film::CHANNEL_DIRECT_DIFFUSE);
		DrawChannelInfo("CHANNEL_DIRECT_DIFFUSE_REFLECT", Film::CHANNEL_DIRECT_DIFFUSE_REFLECT);
		DrawChannelInfo("CHANNEL_DIRECT_DIFFUSE_TRANSMIT", Film::CHANNEL_DIRECT_DIFFUSE_TRANSMIT);
		DrawChannelInfo("CHANNEL_DIRECT_GLOSSY", Film::CHANNEL_DIRECT_GLOSSY);
		DrawChannelInfo("CHANNEL_DIRECT_GLOSSY_REFLECT", Film::CHANNEL_DIRECT_GLOSSY_REFLECT);
		DrawChannelInfo("CHANNEL_DIRECT_GLOSSY_TRANSMIT", Film::CHANNEL_DIRECT_GLOSSY_TRANSMIT);
		DrawChannelInfo("CHANNEL_EMISSION", Film::CHANNEL_EMISSION);
		DrawChannelInfo("CHANNEL_INDIRECT_DIFFUSE", Film::CHANNEL_INDIRECT_DIFFUSE);
		DrawChannelInfo("CHANNEL_INDIRECT_DIFFUSE_REFLECT", Film::CHANNEL_INDIRECT_DIFFUSE_REFLECT);
		DrawChannelInfo("CHANNEL_INDIRECT_DIFFUSE_TRANSMIT", Film::CHANNEL_INDIRECT_DIFFUSE_TRANSMIT);
		DrawChannelInfo("CHANNEL_INDIRECT_GLOSSY", Film::CHANNEL_INDIRECT_GLOSSY);
		DrawChannelInfo("CHANNEL_INDIRECT_GLOSSY_REFLECT", Film::CHANNEL_INDIRECT_GLOSSY_REFLECT);
		DrawChannelInfo("CHANNEL_INDIRECT_GLOSSY_TRANSMIT", Film::CHANNEL_INDIRECT_GLOSSY_TRANSMIT);
		DrawChannelInfo("CHANNEL_INDIRECT_SPECULAR", Film::CHANNEL_INDIRECT_SPECULAR);
		DrawChannelInfo("CHANNEL_INDIRECT_SPECULAR_REFLECT", Film::CHANNEL_INDIRECT_SPECULAR_REFLECT);
		DrawChannelInfo("CHANNEL_INDIRECT_SPECULAR_TRANSMIT", Film::CHANNEL_INDIRECT_SPECULAR_TRANSMIT);
		DrawChannelInfo("CHANNEL_MATERIAL_ID_MASK", Film::CHANNEL_MATERIAL_ID_MASK);
		DrawChannelInfo("CHANNEL_DIRECT_SHADOW_MASK", Film::CHANNEL_DIRECT_SHADOW_MASK);
		DrawChannelInfo("CHANNEL_INDIRECT_SHADOW_MASK", Film::CHANNEL_INDIRECT_SHADOW_MASK);
		DrawChannelInfo("CHANNEL_UV", Film::CHANNEL_UV);
		DrawChannelInfo("CHANNEL_RAYCOUNT", Film::CHANNEL_RAYCOUNT);
		DrawChannelInfo("CHANNEL_BY_MATERIAL_ID", Film::CHANNEL_BY_MATERIAL_ID);
		DrawChannelInfo("CHANNEL_IRRADIANCE", Film::CHANNEL_IRRADIANCE);
		DrawChannelInfo("CHANNEL_OBJECT_ID", Film::CHANNEL_OBJECT_ID);
		DrawChannelInfo("CHANNEL_OBJECT_ID_MASK", Film::CHANNEL_OBJECT_ID_MASK);
		DrawChannelInfo("CHANNEL_BY_OBJECT_ID", Film::CHANNEL_BY_OBJECT_ID);
		DrawChannelInfo("CHANNEL_SAMPLECOUNT", Film::CHANNEL_SAMPLECOUNT);
		DrawChannelInfo("CHANNEL_CONVERGENCE", Film::CHANNEL_CONVERGENCE);
		DrawChannelInfo("CHANNEL_MATERIAL_ID_COLOR", Film::CHANNEL_MATERIAL_ID_COLOR);
		DrawChannelInfo("CHANNEL_ALBEDO", Film::CHANNEL_ALBEDO);
		DrawChannelInfo("CHANNEL_AVG_SHADING_NORMAL", Film::CHANNEL_AVG_SHADING_NORMAL);
		DrawChannelInfo("CHANNEL_NOISE", Film::CHANNEL_NOISE);
	}
	ImGui::End();

	if (opened) {
		// Draw all channel windows
		BOOST_FOREACH(FilmChannelWindowMap::value_type e, filmChannelWindows)
			e.second->Draw();
	} else
		DeleteAllWindow();
}
