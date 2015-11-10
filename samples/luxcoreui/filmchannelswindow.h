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

#ifndef _LUXCOREAPP_FILMCHANNELSWINDOW_H
#define	_LUXCOREAPP_FILMCHANNELSWINDOW_H

#include <string>
#include <boost/unordered_map.hpp>

#include <imgui.h>

#include "luxcore/luxcore.h"
#include "objectwindow.h"
#include "typetable.h"

class LuxCoreApp;
class FilmChannelsWindow;

class FilmChannelWindow : public ObjectWindow {
public:
	FilmChannelWindow(FilmChannelsWindow *channelsWindow, const std::string &title,
			const luxcore::Film::FilmChannelType type, const u_int index);
	virtual ~FilmChannelWindow();

	virtual void Open();
	virtual void Draw();

private:
	void RefreshTexture();
	void Copy1(const float *filmPixels, float *pixels,
			const u_int filmWidth, const u_int filmHeight) const;
	void Copy2(const float *filmPixels, float *pixels,
			const u_int filmWidth, const u_int filmHeight) const;
	void Copy1UINT(const u_int *filmPixels, float *pixels,
			const u_int filmWidth, const u_int filmHeight) const;
	void Copy3(const float *filmPixels, float *pixels,
			const u_int filmWidth, const u_int filmHeight) const;
	void Normalize1(const float *filmPixels, float *pixels,
			const u_int filmWidth, const u_int filmHeight) const;
	void Normalize3(const float *filmPixels, float *pixels,
			const u_int filmWidth, const u_int filmHeight) const;
	void AutoLinearToneMap(const float *src, float *dst,
			const u_int filmWidth, const u_int filmHeight) const;

	FilmChannelsWindow *channelsWindow;
	const luxcore::Film::FilmChannelType type;
	const u_int index;

	GLuint channelTexID;
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
	void DeleteWindow(const luxcore::Film::FilmChannelType type, const u_int index);
	void DeleteAllWindow();
	std::string GetKey(const luxcore::Film::FilmChannelType type, const u_int index) const;
	void DrawShowCheckBox(const std::string &label, const luxcore::Film::FilmChannelType type,
			const u_int index);
	void DrawChannelInfo(const std::string &label, const luxcore::Film::FilmChannelType type);

	typedef boost::unordered_map<std::string, FilmChannelWindow *> FilmChannelWindowMap;
	FilmChannelWindowMap filmChannelWindows;
};

#endif	/* _LUXCOREAPP_FILMCHANNELSWINDOW_H */
