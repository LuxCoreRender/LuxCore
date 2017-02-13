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

#ifndef _LUXCOREAPP_LOGWINDOW_H
#define	_LUXCOREAPP_LOGWINDOW_H

#include "objectwindow.h"

class LogWindow : public ObjectWindow {
public:
	LogWindow(LuxCoreApp *a) : ObjectWindow(a, "Log window") { }
	virtual ~LogWindow() { }

	void Clear();
	void AddMsg(const char *msg);

	virtual void Draw();

private:
	ImGuiTextBuffer buffer;
	ImGuiTextFilter filter;
	ImVector<int> lineOffsets; // Index to lines offset

	bool scrollToBottom;
};

#endif	/* _LUXCOREAPP_LOGWINDOW_H */

