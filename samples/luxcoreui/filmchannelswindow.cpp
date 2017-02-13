/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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
#include <limits>
#include <boost/format.hpp>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// FilmChannelWindow
//------------------------------------------------------------------------------

FilmChannelWindow::FilmChannelWindow(LuxCoreApp *a, const string &title,
			const luxcore::Film::FilmChannelType t, const u_int i) :
	ImageWindow(a, title), type(t), index(i) {
	glGenTextures(1, &channelTexID);
}

FilmChannelWindow::~FilmChannelWindow() {
	glDeleteTextures(1, &channelTexID);
}

void FilmChannelWindow::RefreshTexture() {
	app->session->UpdateStats();

	const u_int filmWidth = app->session->GetFilm().GetWidth();
	const u_int filmHeight = app->session->GetFilm().GetHeight();
	
	auto_ptr<float> pixels(new float[filmWidth * filmHeight * 3]);
	switch (type) {
		case Film::CHANNEL_RADIANCE_PER_PIXEL_NORMALIZED:
		case Film::CHANNEL_DIRECT_DIFFUSE:
		case Film::CHANNEL_DIRECT_GLOSSY:
		case Film::CHANNEL_EMISSION:
		case Film::CHANNEL_INDIRECT_DIFFUSE:
		case Film::CHANNEL_INDIRECT_GLOSSY:
		case Film::CHANNEL_INDIRECT_SPECULAR:
		case Film::CHANNEL_BY_MATERIAL_ID:
		case Film::CHANNEL_IRRADIANCE:
		case Film::CHANNEL_BY_OBJECT_ID:{
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
		case Film::CHANNEL_RAYCOUNT: {
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
		case Film::CHANNEL_OBJECT_ID:
		case Film::CHANNEL_FRAMEBUFFER_MASK: {
			const u_int *filmPixels = app->session->GetFilm().GetChannel<u_int>(type, index);

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
		const u_int index) const {
	return ToString(type) + "#" + ToString(index);
}

void FilmChannelsWindow::DrawShowCheckBox(const string &label,
		const Film::FilmChannelType type, const u_int index) {
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

void FilmChannelsWindow::DrawChannelInfo(const string &label, const Film::FilmChannelType type) {
	const Film &film = app->session->GetFilm();
	u_int count = film.GetChannelCount(type);

	if (count > 0) {
		ImGui::PushID(label.c_str());
		if (ImGui::CollapsingHeader((label + ": " + ToString(count) + " chanel(s)").c_str(), NULL, true, true)) {
			for (u_int i = 0; i < count; ++i)
				DrawShowCheckBox(label, type, i);
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
		DrawChannelInfo("CHANNEL_DIRECT_GLOSSY", Film::CHANNEL_DIRECT_GLOSSY);
		DrawChannelInfo("CHANNEL_INDIRECT_SPECULAR", Film::CHANNEL_INDIRECT_SPECULAR);
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
		DrawChannelInfo("CHANNEL_FRAMEBUFFER_MASK", Film::CHANNEL_FRAMEBUFFER_MASK);
	}
	ImGui::End();

	if (opened) {
		// Draw all channel windows
		BOOST_FOREACH(FilmChannelWindowMap::value_type e, filmChannelWindows)
			e.second->Draw();
	} else
		DeleteAllWindow();
}
