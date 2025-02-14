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

#include "luxcoreapp.h"
#include "imagewindow.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// ImageWindow
//------------------------------------------------------------------------------

ImageWindow::ImageWindow(LuxCoreApp *a, const string &title) :
	ObjectWindow(a, title), imgScale(100.f) {
	glGenTextures(1, &channelTexID);
}

ImageWindow::~ImageWindow() {
	glDeleteTextures(1, &channelTexID);
}

void ImageWindow::Copy1UINT2FLOAT(const unsigned int *filmPixels, float *pixels,
		const unsigned int filmWidth, const unsigned int filmHeight) const {
	for (unsigned int y = 0; y < filmHeight; ++y) {
		for (unsigned int x = 0; x < filmWidth; ++x) {
			const unsigned int index = x + y * filmWidth;
			const unsigned int src = filmPixels[index];
			float *dst = &pixels[index * 3];

			const float v = src;
			dst[0] = v;
			dst[1] = v;
			dst[2] = v;
		}
	}
}

void ImageWindow::Copy1UINT(const unsigned int *filmPixels, float *pixels,
		const unsigned int filmWidth, const unsigned int filmHeight) const {
	for (unsigned int y = 0; y < filmHeight; ++y) {
		for (unsigned int x = 0; x < filmWidth; ++x) {
			const unsigned int index = x + y * filmWidth;
			const unsigned int src = filmPixels[index];
			float *dst = &pixels[index * 3];

			dst[0] = ((src & 0x0000ff)) / 255.f;
			dst[1] = ((src & 0x00ff00) >> 8) / 255.f;
			dst[2] = ((src & 0xff0000) >> 16) / 255.f;
		}
	}
}

void ImageWindow::Copy1(const float *filmPixels, float *pixels,
		const unsigned int filmWidth, const unsigned int filmHeight) const {
	for (unsigned int y = 0; y < filmHeight; ++y) {
		for (unsigned int x = 0; x < filmWidth; ++x) {
			const unsigned int index = x + y * filmWidth;
			const float *src = &filmPixels[index];
			float *dst = &pixels[index * 3];

			const float v = src[0];
			dst[0] = v;
			dst[1] = v;
			dst[2] = v;
		}
	}
}

void ImageWindow::Copy2(const float *filmPixels, float *pixels,
		const unsigned int filmWidth, const unsigned int filmHeight) const {
	for (unsigned int y = 0; y < filmHeight; ++y) {
		for (unsigned int x = 0; x < filmWidth; ++x) {
			const unsigned int index = x + y * filmWidth;
			const float *src = &filmPixels[index * 2];
			float *dst = &pixels[index * 3];

			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = 0.f;
		}
	}
}

void ImageWindow::Copy3(const float *filmPixels, float *pixels,
		const unsigned int filmWidth, const unsigned int filmHeight) const {
	copy(&filmPixels[0], &filmPixels[filmWidth * filmHeight * 3], pixels);
}

void ImageWindow::Normalize1(const float *filmPixels, float *pixels,
		const unsigned int filmWidth, const unsigned int filmHeight) const {
	for (unsigned int y = 0; y < filmHeight; ++y) {
		for (unsigned int x = 0; x < filmWidth; ++x) {
			const unsigned int index = x + y * filmWidth;
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

void ImageWindow::Normalize3(const float *filmPixels, float *pixels,
		const unsigned int filmWidth, const unsigned int filmHeight) const {
	for (unsigned int y = 0; y < filmHeight; ++y) {
		for (unsigned int x = 0; x < filmWidth; ++x) {
			const unsigned int index = x + y * filmWidth;
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

void ImageWindow::AutoLinearToneMap(const float *src, float *dst,
		const unsigned int filmWidth, const unsigned int filmHeight) const {
	// In order to work with negative values (i.e. POSITION AOV, etc.)
	float minVal[3] = {
		numeric_limits<float>::min(),
		numeric_limits<float>::min(),
		numeric_limits<float>::min()
	};
	for (unsigned int yy = 0; yy < filmHeight; ++yy) {
		for (unsigned int xx = 0; xx < filmWidth; ++xx) {
			const unsigned int index = xx + yy * filmWidth;
			const float *p = &src[index * 3];

			minVal[0] = Min(minVal[0], p[0]);
			minVal[1] = Min(minVal[1], p[1]);
			minVal[2] = Min(minVal[2], p[2]);
		}
	}

	float Y = 0.f;
	for (unsigned int yy = 0; yy < filmHeight; ++yy) {
		for (unsigned int xx = 0; xx < filmWidth; ++xx) {
			const unsigned int index = (xx + yy * filmWidth) * 3;
			const float *s = &src[index];
			float *d = &dst[index];

			d[0] = s[0] - minVal[0];
			d[1] = s[1] - minVal[1];
			d[2] = s[2] - minVal[2];

			// Spectrum::Y()
			const float y = 0.212671f * d[0] + 0.715160f * d[1] + 0.072169f * d[2];
			if ((y <= 0.f) || isinf(y))
				continue;

			Y += y;
		}
	}

	const unsigned int pixelCount = filmWidth * filmHeight;
	Y /= pixelCount;
	if (Y <= 0.f)
		return;

	const float scale = (1.25f / Y * powf(118.f / 255.f, 2.2f));

	for (unsigned int i = 0; i < pixelCount * 3; ++i)
		dst[i] = scale * src[i];
}

void ImageWindow::UpdateStats(const float *pixels,
		const unsigned int filmWidth, const unsigned int filmHeight) {
	imgMin[0] = imgMin[1] = imgMin[2] = numeric_limits<float>::max();
	imgMax[0] = imgMax[1] = imgMax[2] = numeric_limits<float>::min();
	imgAvg[0] = imgAvg[1] = imgAvg[2] = 0.f;

	for (unsigned int y = 0; y < filmHeight; ++y) {
		for (unsigned int x = 0; x < filmWidth; ++x) {
			const unsigned int index = x + y * filmWidth;
			const float *src = &pixels[index * 3];

			for (unsigned int i = 0; i < 3; ++i) {
				imgMin[i] = Min(imgMin[i], src[i]);
				imgMax[i] = Max(imgMax[i], src[i]);
				imgAvg[i] += src[i];
			}
		}
	}

	const unsigned int pixelCount = filmWidth * filmHeight;
	imgAvg[0] /= pixelCount;
	imgAvg[1] /= pixelCount;
	imgAvg[2] /= pixelCount;
}

void ImageWindow::Open() {
	if (!opened) {
		RefreshTexture();

		ObjectWindow::Open();
	}
}

void ImageWindow::Draw() {
	if (!opened)
		return;

	int frameBufferWidth, frameBufferHeight;
	glfwGetFramebufferSize(app->window, &frameBufferWidth, &frameBufferHeight);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(ImVec2(450.f, 300.f)));
	if (ImGui::Begin(windowTitle.c_str(), &opened)) {
		if (ImGui::Button("Refresh image"))
			RefreshTexture();

		ImGui::SameLine();
		ImGui::SliderFloat("Zoom", &imgScale, 10.f, 1000.f, "%.1f%%");

		LuxCoreApp::ColoredLabelText("Min. value:", "(%f, %f, %f)", imgMin[0], imgMin[1], imgMin[2]);
		LuxCoreApp::ColoredLabelText("Max. value:", "(%f, %f, %f)", imgMax[0], imgMax[1], imgMax[2]);
		LuxCoreApp::ColoredLabelText("Avg. value:", "(%f, %f, %f)", imgAvg[0], imgAvg[1], imgAvg[2]);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.f);
		ImGui::BeginChild("Image", ImVec2(0.f, 0.f), true, ImGuiWindowFlags_HorizontalScrollbar);

		const unsigned int filmWidth = app->session->GetFilm().GetWidth();
		const unsigned int filmHeight = app->session->GetFilm().GetHeight();
		ImGui::Image((ImTextureID) (intptr_t) channelTexID,
				ImVec2(.01f * imgScale * filmWidth, .01f * imgScale * filmHeight),
				ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));

		ImGui::EndChild();
		ImGui::PopStyleVar();
	}
	ImGui::End();
	ImGui::PopStyleVar();
}
