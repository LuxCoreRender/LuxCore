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

#ifndef _LUXCOREAPP_FILMOUTPUTSWINDOW_H
#define	_LUXCOREAPP_FILMOUTPUTSWINDOW_H

#include <string>

#include <imgui.h>

#include "objecteditorwindow.h"
#include "imagewindow.h"
#include "typetable.h"

class LuxCoreApp;

class FilmOutputWindow : public ImageWindow {
public:
	FilmOutputWindow(LuxCoreApp *app, const std::string &title,
			const luxcore::Film::FilmOutputType type, const u_int index);
	virtual ~FilmOutputWindow();

private:
	virtual void RefreshTexture();
	
	void CopyAlpha(const float *filmPixels, float *pixels,
			const u_int filmWidth, const u_int filmHeight) const;

	const luxcore::Film::FilmOutputType type;
	const u_int index;
};

class FilmOutputsWindow : public ObjectEditorWindow {
public:
	FilmOutputsWindow(LuxCoreApp *a);
	virtual ~FilmOutputsWindow();

	virtual void Close();
	virtual void Draw();

private:
	luxrays::Properties GetFilmOutputsProperties(const luxrays::Properties &cfgProps) const;

	virtual void RefreshObjectProperties(luxrays::Properties &props);
	virtual void ParseObjectProperties(const luxrays::Properties &props);
	virtual bool DrawObjectGUI(luxrays::Properties &props, bool &modified);

	void DeleteWindow(const std::string &key);
	void DeleteWindow(const luxcore::Film::FilmOutputType type, const u_int index);
	void DeleteAllWindow();
	std::string GetKey(const luxcore::Film::FilmOutputType type, const u_int index) const;
	void DrawShowCheckBox(const std::string &label, const luxcore::Film::FilmOutputType type, const u_int index);

	TypeTable typeTable;

	int newType;
	char newOutputNameBuff[4 * 1024];
	char newFileNameBuff[4 * 1024];
	int newFileType;
	int newID;

	typedef boost::unordered_map<std::string, FilmOutputWindow *> FilmOutputWindowMap;
	FilmOutputWindowMap filmOutputWindows;
};

#endif	/* _LUXCOREAPP_FILMOUTPUTSWINDOW_H */
