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

#ifndef _LUXCOREAPP_OBJECTWINDOW_H
#define	_LUXCOREAPP_OBJECTWINDOW_H

#include <string>

#include <imgui.h>

class LuxCoreApp;

class ObjectWindow {
public:
	ObjectWindow(LuxCoreApp *a, const std::string &name) :
			windowTitle(name), app(a), opened(false) { }
	virtual ~ObjectWindow() { }

	virtual void Open();
	virtual void Close();
	void Toggle() {
		if (opened)
			Close();
		else
			Open();
	}

	bool IsOpen() const { return opened; }

	virtual void Draw() = 0;

protected:
	std::string windowTitle;

	LuxCoreApp *app;

	bool opened;
};

#endif	/* _LUXCOREAPP_OBJECTWINDOW_H */
