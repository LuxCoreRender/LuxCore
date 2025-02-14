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

#include "logwindow.h"

using namespace std;

//------------------------------------------------------------------------------
// LogWindow
//------------------------------------------------------------------------------

void LogWindow::Clear() {
	buffer.clear();
	lineOffsets.clear();
}

void LogWindow::AddMsg(const char *msg) {
	int oldSize = buffer.size();
	buffer.append("%s\n", msg);

	for (int newSize = buffer.size(); oldSize < newSize; oldSize++)
		if (buffer[oldSize] == '\n')
			lineOffsets.push_back(oldSize);

	scrollToBottom = true;
}

void LogWindow::Draw() {
	if (!opened)
		return;

	ImGui::SetNextWindowSize(ImVec2(512.f, 200.f), ImGuiCond_Appearing);

	if (ImGui::Begin(windowTitle.c_str(), &opened)) {
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);

		if (ImGui::Button("Clear"))
			Clear();
		ImGui::SameLine();

		const bool copy = ImGui::Button("Copy");
		ImGui::SameLine();

		filter.Draw("Filter", -100.0f);
		ImGui::Separator();
		ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		if (copy)
			ImGui::LogToClipboard();

		if (filter.IsActive()) {
			const char *bufferBegin = buffer.begin();
			const char *line = bufferBegin;
			for (int lineNumber = 0; line != NULL; lineNumber++) {
				const char *line_end = (lineNumber < lineOffsets.Size) ? bufferBegin + lineOffsets[lineNumber] : NULL;

				if (filter.PassFilter(line, line_end))
					ImGui::TextUnformatted(line, line_end);
				line = line_end && line_end[1] ? line_end + 1 : NULL;
			}
		} else
			ImGui::TextUnformatted(buffer.begin());

		if (scrollToBottom)
			ImGui::SetScrollHereY(1.0f);
		scrollToBottom = false;

		ImGui::EndChild();
		ImGui::PopStyleVar();
	}
	ImGui::End();
}
