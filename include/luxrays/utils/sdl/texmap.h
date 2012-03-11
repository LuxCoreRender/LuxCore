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

#ifndef _LUXRAYS_SDL_TEXMAP_H
#define	_LUXRAYS_SDL_TEXMAP_H

#include <string>
#include <vector>
#include <map>

#include "luxrays/utils/sdl/sdl.h"

namespace luxrays { namespace sdl {

class TextureMap {
public:
	TextureMap(const std::string &fileName, const float gamma);
	TextureMap(Spectrum *cols, const unsigned int w, const unsigned int h);

	/**
	 * Creates a 24-bpp texture with size based on another bitmap, usually an alpha map, and initialized each pixel
	 * with a given color.
	 */
	TextureMap(const std::string& baseFileName, const float gamma,
		const float red, const float green, const float blue);

	~TextureMap();

	float GetGamma() const { return gamma; }

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
	};

	/**
	 * Add an alpha map to a texture that didn't have it built in. This is usually employed to
	 * support separate alpha maps stored as greyscale bitmaps (usualluy jpegs). The format
	 * comes in handy also when using procedural textures. In this scheme pure white (1.0,1.0,1.0)
	 * means solid and pure black (0,0,0) means transparent. Levels of grey detemine partial transparent
	 * areas.
	*/
	void AddAlpha(const std::string &alphaMap);
  
	const bool HasAlpha() const { return alpha != NULL; }
	float GetAlpha(const UV &uv) const {
		assert (alpha != NULL);

		const float s = uv.u * width - 0.5f;
		const float t = uv.v * height - 0.5f;

		const int s0 = Floor2Int(s);
		const int t0 = Floor2Int(t);

		const float ds = s - s0;
		const float dt = t - t0;

		const float ids = 1.f - ds;
		const float idt = 1.f - dt;

		return ids * idt * GetAlphaTexel(s0, t0) +
				ids * dt * GetAlphaTexel(s0, t0 + 1) +
				ds * idt * GetAlphaTexel(s0 + 1, t0) +
				ds * dt * GetAlphaTexel(s0 + 1, t0 + 1);
	}

	const UV &GetDuDv() const {
		return DuDv;
	}

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }
	const Spectrum *GetPixels() const { return pixels; };
	const float *GetAlphas() const { return alpha; };

private:
	const Spectrum &GetTexel(const int s, const int t) const {
		const unsigned int u = Mod<int>(s, width);
		const unsigned int v = Mod<int>(t, height);

		const unsigned index = v * width + u;
		assert (index >= 0);
		assert (index < width * height);

		return pixels[index];
	}

	const float &GetAlphaTexel(const unsigned int s, const unsigned int t) const {
		const unsigned int u = Mod(s, width);
		const unsigned int v = Mod(t, height);

		const unsigned index = v * width + u;
		assert (index >= 0);
		assert (index < width * height);

		return alpha[index];
	}

	float gamma;
	unsigned int width, height;
	Spectrum *pixels;
	float *alpha;
	UV DuDv;
};

class TexMapInstance {
public:
	TexMapInstance(const TextureMap *tm) : texMap(tm) { }

	const TextureMap *GetTexMap() const { return texMap; }

private:
	const TextureMap *texMap;
};

class BumpMapInstance {
public:
	BumpMapInstance(const TextureMap *tm, const float valueScale) :
		texMap(tm), scale(valueScale) { }

	const TextureMap *GetTexMap() const { return texMap; }
	float GetScale() const { return scale; }

private:
	const TextureMap *texMap;
	const float scale;
};

class NormalMapInstance {
public:
	NormalMapInstance(const TextureMap *tm) : texMap(tm) { }

	const TextureMap *GetTexMap() const { return texMap; }

private:
	const TextureMap *texMap;
};

class TextureMapCache {
public:
	TextureMapCache();
	~TextureMapCache();

	TexMapInstance *GetTexMapInstance(const std::string &fileName, const float gamma);
	BumpMapInstance *GetBumpMapInstance(const std::string &fileName, const float scale);
	NormalMapInstance *GetNormalMapInstance(const std::string &fileName);

	void GetTexMaps(std::vector<TextureMap *> &tms);
	unsigned int GetSize()const { return maps.size(); }

	/**
	 * Method to retrieve an already cached diffuse map. This is used when adding an alpha map
	 * stored in a separate file.
	 */
	TextureMap *FindTextureMap(const std::string &fileName, const float gamma);

	/**
	 * Method used to add an existing texture to the cache. Without this
	 * the only other method is GetTextureMap() but that creates a new texture from scratch
	 */
	TexMapInstance *AddTextureMap(const std::string &fileName, TextureMap *tm);
  
private:
	TextureMap *GetTextureMap(const std::string &fileName, const float gamma);

	std::map<std::string, TextureMap *> maps;
	std::vector<TexMapInstance *> texInstances;
	std::vector<BumpMapInstance *> bumpInstances;
	std::vector<NormalMapInstance *> normalInstances;
};

} }

#endif	/* _LUXRAYS_SDL_TEXMAP_H */
