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

#ifndef _LUXCOREAPP_FILMCHANNELSWINDOW_H
#define	_LUXCOREAPP_FILMCHANNELSWINDOW_H

#include <string>
#include <boost/unordered_map.hpp>

#include <imgui.h>

#include "luxcore/luxcore.h"
#include "imagewindow.h"
#include "typetable.h"

class LuxCoreApp;

class FilmChannelWindow : public ImageWindow {
public:
	FilmChannelWindow(LuxCoreApp *app, const std::string &title,
			const luxcore::Film::FilmChannelType type, const unsigned int index);
	virtual ~FilmChannelWindow();

private:
	virtual void RefreshTexture();

	const luxcore::Film::FilmChannelType type;
	const unsigned int index;
};

class FilmChannelsWindow : public ObjectWindow {
public:
	FilmChannelsWindow(LuxCoreApp *a);
	virtual ~FilmChannelsWindow();

	virtual void Close();
	virtual void Draw();

	friend class FilmChannelWindow;

private:
	void DeleteWindow(const std::string &key);
	void DeleteAllWindow();
	std::string GetKey(const luxcore::Film::FilmChannelType type, const unsigned int index) const;
	void DrawShowCheckBox(const std::string &label, const luxcore::Film::FilmChannelType type,
			const unsigned int index);
	void DrawChannelInfo(const std::string &label, const luxcore::Film::FilmChannelType type);

	typedef boost::unordered_map<std::string, FilmChannelWindow *> FilmChannelWindowMap;
	FilmChannelWindowMap filmChannelWindows;
};

#endif	/* _LUXCOREAPP_FILMCHANNELSWINDOW_H */
