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
#include <boost/foreach.hpp>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// UserImportancePaintWindow
//------------------------------------------------------------------------------

void UserImportancePaintWindow::Draw() {
	if (!opened)
		return;

	ImGui::SetNextWindowSize(ImVec2(320.f, 180.f), ImGuiCond_Appearing);

	if (ImGui::Begin(windowTitle.c_str(), &opened)) {
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);

		ImGui::Checkbox("Show map overlay", &showOverlay);

		if (ImGui::SliderInt("Pen radius", &paintPenRadius, 1, 128))
			paintPenRadius2 = paintPenRadius * paintPenRadius;
	
		if (ImGui::Button("Fill map with 1.0"))
			fill(importanceMap.begin(), importanceMap.end(), 1.f);
		if (ImGui::Button("Fill map with 0.5"))
			fill(importanceMap.begin(), importanceMap.end(), .5f);
		if (ImGui::Button("Fill map with 0.0"))
			fill(importanceMap.begin(), importanceMap.end(), 0.f);

		if (ImGui::Button("Set map"))
			app->session->GetFilm().UpdateOutput<float>(Film::OUTPUT_USER_IMPORTANCE, &importanceMap[0], 0, false);
		ImGui::SameLine();
		if (ImGui::Button("Get map"))
			app->session->GetFilm().GetOutput(Film::OUTPUT_USER_IMPORTANCE, &importanceMap[0], 0, false);

		ImGui::PopStyleVar();
	}
	ImGui::End();
}

void UserImportancePaintWindow::Init() {
	if (app->session) {
		const Film &film = app->session->GetFilm();
		const unsigned int pixelsCount = film.GetWidth() * film.GetHeight();

		importanceMap.resize(pixelsCount);
		fill(importanceMap.begin(), importanceMap.end(), 1.f);
		
		paintPenRadius = 32;
		paintPenRadius2 = paintPenRadius * paintPenRadius;
		
		showOverlay = true;

		Open();
	} else
		Close();
}

void UserImportancePaintWindow::BlendImportanceMap(const float *srcPixels, float *dstPixels) const {
	int frameBufferWidth, frameBufferHeight;
	glfwGetFramebufferSize(app->window, &frameBufferWidth, &frameBufferHeight);

	const Film &film = app->session->GetFilm();
	const int filmWidth = film.GetWidth();
	const int filmHeight = film.GetHeight();
	const unsigned int pixelsCount = filmWidth * filmHeight;

	// Blend the user importance map with the rendering image
	for (unsigned int i = 0; i < pixelsCount; ++i) {
		const u_int index = i * 3;
		const float factor = importanceMap[i] * .5f + .5f;

		dstPixels[index] = srcPixels[index] * factor;
		dstPixels[index + 1] = srcPixels[index + 1] * factor;
		dstPixels[index + 2] = srcPixels[index + 2] * factor;
	}
	
	// Draw the cursor
	const int cursorSize = 8;

	const ImVec2 imGuiScale(filmWidth /  (float)frameBufferWidth,  filmHeight / (float)frameBufferHeight);
	const int mouseX = Floor2Int(ImGui::GetIO().MousePos.x * imGuiScale.x);
	const int mouseY = Floor2Int((frameBufferHeight - ImGui::GetIO().MousePos.y - 1) * imGuiScale.y);

	for (int y = -cursorSize; y <= cursorSize; y++) {
		const int imageX = mouseX;
		const int imageY = mouseY + y;

		if ((imageX >= 0) && (imageX < filmWidth) && (imageY >= 0) && (imageY < filmHeight)) {
			const unsigned int index = (imageX + imageY * filmWidth) * 3;

			dstPixels[index] = 1.f;
			dstPixels[index + 1] = 0.f;
			dstPixels[index + 2] = 0.f;
		}
	}

	for (int x = -cursorSize; x <= cursorSize; x++) {
		const int imageX = mouseX + x;
		const int imageY = mouseY;

		if ((imageX >= 0) && (imageX < filmWidth) && (imageY >= 0) && (imageY < filmHeight)) {
			const unsigned int index = (imageX + imageY * filmWidth) * 3;

			dstPixels[index] = 0.f;
			dstPixels[index + 1] = 0.f;
			dstPixels[index + 2] = 1.f;
		}
	}
}

void UserImportancePaintWindow::Paint(const bool addValues) {
	int frameBufferWidth, frameBufferHeight;
	glfwGetFramebufferSize(app->window, &frameBufferWidth, &frameBufferHeight);

	const Film &film = app->session->GetFilm();
	const int filmWidth = film.GetWidth();
	const int filmHeight = film.GetHeight();

	const ImVec2 imGuiScale(filmWidth /  (float)frameBufferWidth,  filmHeight / (float)frameBufferHeight);
	const int mouseX = Floor2Int(ImGui::GetIO().MousePos.x * imGuiScale.x);
	const int mouseY = Floor2Int((frameBufferHeight - ImGui::GetIO().MousePos.y - 1) * imGuiScale.y);

	for (int y = -paintPenRadius; y <= paintPenRadius; y++) {
		for (int x = -paintPenRadius; x <= paintPenRadius; x++) {
			const int distance2 = x * x + y * y;

			if (distance2 <= paintPenRadius2) {
				const int imageX = x + mouseX;
				const int imageY = y + mouseY;

				if ((imageX >= 0) && (imageX < filmWidth) && (imageY >= 0) && (imageY < filmHeight)) {
					const unsigned int index = imageX + imageY * filmWidth;

					if (addValues) {
						const float value = Min(1.f, 1.f - distance2 / (float)paintPenRadius2 + .5f);
						importanceMap[index] = Max(importanceMap[index], value);
					} else {
						const float value = Max(0.f, distance2 / (float)paintPenRadius2 - .5f);

						importanceMap[index] = Min(importanceMap[index], value);
					}
				}
			}
		}
	}
}
