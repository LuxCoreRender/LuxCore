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

#ifndef _LUXCOREAPP_OBJECTEDITORWINDOW_H
#define	_LUXCOREAPP_OBJECTEDITORWINDOW_H

#include "objectwindow.h"

class LuxCoreApp;

class ObjectEditorWindow : public ObjectWindow {
public:
	ObjectEditorWindow(LuxCoreApp *a, const std::string &name) :
		ObjectWindow(a, name + " editor"), objectName(name) { }
	virtual ~ObjectEditorWindow() { }

	virtual bool IsDynamicEditor() const { return false; }

	virtual void Open();
	virtual void Close();

	virtual void Draw();

protected:
	void RefreshGUIProperties();
	void ParseGUIProperties();
	void RefreshEditorProperties();
	void ParseEditorProperties();

	virtual void RefreshObjectProperties(luxrays::Properties &props) = 0;
	virtual void ParseObjectProperties(const luxrays::Properties &props) = 0;
	virtual bool DrawObjectGUI(luxrays::Properties &props, bool &modifiedProps) = 0;

	std::string objectName;

	luxrays::Properties objectGUIProps;
	bool modifiedGUIProps;

	luxrays::Properties objectEditorProps;
	char advancedEditorText[1024 * 32];
	bool modifiedEditorProps;
};

#endif	/* _LUXCOREAPP_OBJECTEDITORWINDOW_H */
