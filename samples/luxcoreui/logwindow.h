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

#ifndef _LUXCOREAPP_LOGWINDOW_H
#define	_LUXCOREAPP_LOGWINDOW_H

#include <string>

#include <imgui.h>

class LogWindow {
public:
	LogWindow() : title("Log"), opened(false) { }
	~LogWindow() { }

	void Clear();
	void AddMsg(const char *msg);
	void Draw();

	std::string title;
	bool opened;

private:
	ImGuiTextBuffer buffer;
	ImGuiTextFilter filter;
	ImVector<int> lineOffsets; // Index to lines offset

	bool scrollToBottom;
};

#endif	/* _LUXCOREAPP_LOGWINDOW_H */

