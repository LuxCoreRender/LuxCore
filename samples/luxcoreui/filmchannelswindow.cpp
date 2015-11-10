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
// FilmChannelWindow
//------------------------------------------------------------------------------

FilmChannelWindow::FilmChannelWindow(FilmChannelsWindow *cs, const string &title,
			const luxcore::Film::FilmChannelType t, const u_int i) :
	ObjectWindow(cs->app, title), channelsWindow(cs), type(t), index(i) {
	glGenTextures(1, &channelTexID);
}

FilmChannelWindow::~FilmChannelWindow() {
	glDeleteTextures(1, &channelTexID);
}

void FilmChannelWindow::Copy1UINT(const u_int *filmPixels, float *pixels,
		const u_int filmWidth, const u_int filmHeight) const {
	for (u_int y = 0; y < filmHeight; ++y) {
		for (u_int x = 0; x < filmWidth; ++x) {
			const u_int index = x + y * filmWidth;
			const u_int src = filmPixels[index];
			float *dst = &pixels[index * 3];

			dst[0] = ((src & 0x0000ff)) / 255.f;
			dst[1] = ((src & 0x00ff00) >> 8) / 255.f;
			dst[2] = ((src & 0xff0000) >> 16) / 255.f;
		}
	}
}

void FilmChannelWindow::Copy1(const float *filmPixels, float *pixels,
		const u_int filmWidth, const u_int filmHeight) const {
	for (u_int y = 0; y < filmHeight; ++y) {
		for (u_int x = 0; x < filmWidth; ++x) {
			const u_int index = x + y * filmWidth;
			const float *src = &filmPixels[index];
			float *dst = &pixels[index * 3];

			const float v = src[0];
			dst[0] = v;
			dst[1] = v;
			dst[2] = v;
		}
	}
}

void FilmChannelWindow::Copy2(const float *filmPixels, float *pixels,
		const u_int filmWidth, const u_int filmHeight) const {
	for (u_int y = 0; y < filmHeight; ++y) {
		for (u_int x = 0; x < filmWidth; ++x) {
			const u_int index = x + y * filmWidth;
			const float *src = &filmPixels[index * 2];
			float *dst = &pixels[index * 3];

			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = 0.f;
		}
	}
}

void FilmChannelWindow::Copy3(const float *filmPixels, float *pixels,
		const u_int filmWidth, const u_int filmHeight) const {
	copy(&filmPixels[0], &filmPixels[filmWidth * filmHeight * 3], pixels);
}

void FilmChannelWindow::Normalize1(const float *filmPixels, float *pixels,
		const u_int filmWidth, const u_int filmHeight) const {
	for (u_int y = 0; y < filmHeight; ++y) {
		for (u_int x = 0; x < filmWidth; ++x) {
			const u_int index = x + y * filmWidth;
			const float *src = &filmPixels[index * 2];
			float *dst = &pixels[index * 3];

			if (src[1] > 0.f) {
				const float v = src[0] / src[1];
				dst[0] = v;
				dst[1] = v;
				dst[2] = v;
			} else {
				dst[0] = 0.f;
				dst[1] = 0.f;
				dst[2] = 0.f;
			}
		}
	}
}

void FilmChannelWindow::Normalize3(const float *filmPixels, float *pixels,
		const u_int filmWidth, const u_int filmHeight) const {
	for (u_int y = 0; y < filmHeight; ++y) {
		for (u_int x = 0; x < filmWidth; ++x) {
			const u_int index = x + y * filmWidth;
			const float *src = &filmPixels[index * 4];
			float *dst = &pixels[index * 3];

			if (src[3] > 0.f) {
				const float k = 1.f / src[3];
				dst[0] = src[0] * k;
				dst[1] = src[1] * k;
				dst[2] = src[2] * k;
			} else {
				dst[0] = 0.f;
				dst[1] = 0.f;
				dst[2] = 0.f;
			}
		}
	}
}

void FilmChannelWindow::AutoLinearToneMap(const float *src, float *dst,
		const u_int filmWidth, const u_int filmHeight) const {
	float Y = 0.f;
	for (u_int yy = 0; yy < filmHeight; ++yy) {
		for (u_int xx = 0; xx < filmWidth; ++xx) {
			const u_int index = xx + yy * filmWidth;
			const float *p = &src[index * 3];

			// Spectrum::Y()
			const float y = 0.212671f * p[0] + 0.715160f * p[1] + 0.072169f * p[2];
			if ((y <= 0.f) || isinf(y))
				continue;

			Y += y;
		}
	}

	const u_int pixelCount = filmWidth * filmHeight;
	Y /= pixelCount;
	if (Y <= 0.f)
		return;

	const float scale = (1.25f / Y * powf(118.f / 255.f, 2.2f));

	for (u_int i = 0; i < pixelCount * 3; ++i)
		dst[i] = scale * src[i];
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
		case Film::CHANNEL_IRRADIANCE: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			Normalize3(filmPixels, pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_RADIANCE_PER_SCREEN_NORMALIZED: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			AutoLinearToneMap(filmPixels, pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_ALPHA:
		case Film::CHANNEL_MATERIAL_ID_MASK:
		case Film::CHANNEL_DIRECT_SHADOW_MASK:
		case Film::CHANNEL_INDIRECT_SHADOW_MASK: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			Normalize1(filmPixels, pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_RGB_TONEMAPPED: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);
			Copy3(filmPixels, pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_DEPTH:
		case Film::CHANNEL_RAYCOUNT: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			Copy1(filmPixels, pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;			
		}
		case Film::CHANNEL_POSITION:
		case Film::CHANNEL_GEOMETRY_NORMAL:
		case Film::CHANNEL_SHADING_NORMAL: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			AutoLinearToneMap(filmPixels, pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_MATERIAL_ID: {
			const u_int *filmPixels = app->session->GetFilm().GetChannel<u_int>(type, index);

			Copy1UINT(filmPixels, pixels.get(), filmWidth, filmHeight);
			AutoLinearToneMap(pixels.get(), pixels.get(), filmWidth, filmHeight);
			break;
		}
		case Film::CHANNEL_UV: {
			const float *filmPixels = app->session->GetFilm().GetChannel<float>(type, index);

			Copy2(filmPixels, pixels.get(), filmWidth, filmHeight);
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

void FilmChannelWindow::Open() {
	if (!opened) {
		RefreshTexture();

		ObjectWindow::Open();
	}
}

void FilmChannelWindow::Draw() {
	if (!opened)
		return;

	int frameBufferWidth, frameBufferHeight;
	glfwGetFramebufferSize(app->window, &frameBufferWidth, &frameBufferHeight);

	ImGui::SetWindowSize(ImVec2(frameBufferWidth / 3.f, frameBufferHeight / 3.f), ImGuiSetCond_Appearing);
	if (ImGui::Begin(windowTitle.c_str(), &opened, ImGuiWindowFlags_HorizontalScrollbar)) {
		if (ImGui::Button("Refresh"))
			RefreshTexture();

		const u_int filmWidth = app->session->GetFilm().GetWidth();
		const u_int filmHeight = app->session->GetFilm().GetHeight();
		ImGui::Image((void *) (intptr_t) channelTexID,
				ImVec2(filmWidth, filmHeight),
				ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));
	}
	ImGui::End();

	if (!opened)
		channelsWindow->DeleteWindow(type, index);
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
	BOOST_FOREACH(FilmChannelWindowMap::value_type e, filmChannelWindows)
		delete e.second;
}

void FilmChannelsWindow::DeleteWindow(const string &key) {
	delete filmChannelWindows[key];
	filmChannelWindows.erase(key);
}

void FilmChannelsWindow::DeleteWindow(const luxcore::Film::FilmChannelType type,
		const u_int index) {
	DeleteWindow(GetKey(type, index));
}

string FilmChannelsWindow::GetKey(const luxcore::Film::FilmChannelType type,
		const u_int index) const {
	return ToString(type) + "#" + ToString(index);
}

void FilmChannelsWindow::DrawShowCheckBox(const string &label,
		const Film::FilmChannelType type, const u_int index) {
	const string key = GetKey(type, index);

	bool show = (filmChannelWindows.count(key) > 0);
	if (ImGui::Checkbox(("Show channel #" + ToString(index)).c_str(), &show)) {
		if (show) {
			FilmChannelWindow *cw = new FilmChannelWindow(this,
					label + " #" + ToString(index) + " window",type, index);
			cw->Open();
			filmChannelWindows[key] = cw;
		} else {
			delete filmChannelWindows[key];
			filmChannelWindows.erase(key);			
		}
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
		DrawChannelInfo("CHANNEL_RGB_TONEMAPPED", Film::CHANNEL_RGB_TONEMAPPED);
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

		// Draw all channel windows
		BOOST_FOREACH(FilmChannelWindowMap::value_type e, filmChannelWindows)
			e.second->Draw();
	}
	ImGui::End();
	
	if (!opened)
		DeleteAllWindow();
}
