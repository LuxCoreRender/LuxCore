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

#ifndef _LUXCOREAPP_IMAGEWINDOW_H
#define	_LUXCOREAPP_IMAGEWINDOW_H

#include <string>
#include <boost/unordered_map.hpp>

#include <imgui.h>

#include "luxcore/luxcore.h"
#include "objectwindow.h"

class LuxCoreApp;

class ImageWindow : public ObjectWindow {
public:
	ImageWindow(LuxCoreApp *app, const std::string &title);
	virtual ~ImageWindow();

	virtual void Open();
	virtual void Draw();

protected:
	virtual void RefreshTexture() = 0;

	void Copy1(const float *filmPixels, float *pixels,
			const unsigned int filmWidth, const unsigned int filmHeight) const;
	void Copy2(const float *filmPixels, float *pixels,
			const unsigned int filmWidth, const unsigned int filmHeight) const;
	void Copy1UINT2FLOAT(const unsigned int *filmPixels, float *pixels,
			const unsigned int filmWidth, const unsigned int filmHeight) const;
	void Copy1UINT(const unsigned int *filmPixels, float *pixels,
			const unsigned int filmWidth, const unsigned int filmHeight) const;
	void Copy3(const float *filmPixels, float *pixels,
			const unsigned int filmWidth, const unsigned int filmHeight) const;
	void Normalize1(const float *filmPixels, float *pixels,
			const unsigned int filmWidth, const unsigned int filmHeight) const;
	void Normalize3(const float *filmPixels, float *pixels,
			const unsigned int filmWidth, const unsigned int filmHeight) const;
	void AutoLinearToneMap(const float *src, float *dst,
			const unsigned int filmWidth, const unsigned int filmHeight) const;
	void UpdateStats(const float *pixels,
			const unsigned int filmWidth, const unsigned int filmHeight);

	GLuint channelTexID;
	float imgScale;
	// Some image statistics
	float imgMin[3], imgMax[3], imgAvg[3];
};

#endif	/* _LUXCOREAPP_FILMWINDOW_H */
