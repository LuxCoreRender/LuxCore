/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _TEXMAP_H
#define	_TEXMAP_H

#include "smalllux.h"

using namespace std;

class TextureMap {
public:
	TextureMap(const string &fileName);
	TextureMap(Spectrum *cols, const unsigned int w, const unsigned int h);
	~TextureMap();

	const Spectrum GetColor(const UV &uv) const {
		const float s = uv.u * width - 0.5f;
		const float t = uv.v * height - 0.5f;

		const int s0 = Floor2Int(s);
		const int t0 = Floor2Int(t);

		const float ds = s - s0;
		const float dt = t - t0;

		const float ids = 1.f - ds;
		const float idt = 1.f - dt;

		return ids * idt * GetTexel(s0, t0) +
				ids * dt * GetTexel(s0, t0 + 1) +
				ds * idt * GetTexel(s0 + 1, t0) +
				ds * dt * GetTexel(s0 + 1, t0 + 1);
	}

private:
	const Spectrum &GetTexel(const unsigned int s, const unsigned int t) const {
		const unsigned int u = Mod(s, width);
		const unsigned int v = Mod(t, height);

		const unsigned index = v * width + u;
		assert (index >= 0);
		assert (index < width * height);

		return pixels[index];
	}

	unsigned int width, height;
	Spectrum *pixels;
};

class TextureMapCache {
public:
	TextureMapCache();
	~TextureMapCache();

	TextureMap *GetTextureMap(const string &fileName);

private:
	std::map<std::string, TextureMap *> maps;
};

#endif	/* _TEXMAP_H */
