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

#include "luxrays/core/geometry/uv.h"
#include "luxrays/utils/core/spectrum.h"
#include "luxrays/utils/sdl/sdl.h"

namespace luxrays { namespace sdl {

class TextureMap {
public:
	TextureMap(const std::string &fileName, const float gamma);
	TextureMap(Spectrum *cols, const float gamma, const unsigned int w, const unsigned int h);

	/**
	 * Creates a 24-bpp texture with size based on another bitmap, usually an alpha map, and initialized each pixel
	 * with a given color.
	 */
	TextureMap(const std::string& baseFileName, const float gamma,
		const float red, const float green, const float blue);

	~TextureMap();

	/**
	 * Add an alpha map to a texture that didn't have it built in. This is usually employed to
	 * support separate alpha maps stored as greyscale bitmaps (usualluy jpegs). The format
	 * comes in handy also when using procedural textures. In this scheme pure white (1.0,1.0,1.0)
	 * means solid and pure black (0,0,0) means transparent. Levels of grey detemine partial transparent
	 * areas.
	*/
	void AddAlpha(const std::string &alphaMap);
	void AddAlpha(float *alphaMap);
  
	const bool HasAlpha() const { return alpha != NULL; }

	float GetGamma() const { return gamma; }
	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }
	const Spectrum *GetPixels() const { return pixels; };
	const float *GetAlphas() const { return alpha; };

protected:
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

	friend class TexMapInstance;
	friend class BumpMapInstance;
	friend class NormalMapInstance;

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
};

class TexMapInstance {
public:
	TexMapInstance(const TextureMap *tm, const float uscale, const float vscale,
		const float udelta, const float vdelta) : texMap(tm),
		uScale(uscale), vScale(vscale), uDelta(udelta), vDelta(vdelta) { }

	const TextureMap *GetTexMap() const { return texMap; }
	const float GetUScale() const { return uScale; }
	const float GetVScale() const { return vScale; }
	const float GetUDelta() const { return uDelta; }
	const float GetVDelta() const { return vDelta; }

	const bool HasAlpha() const { return texMap->HasAlpha(); }

	Spectrum GetColor(const UV &uv) const {
		const UV mapUV(uv.u * uScale + uDelta, uv.v * vScale + vDelta);

		return texMap->GetColor(mapUV);
	}

	float GetAlpha(const UV &uv) const {
		const UV mapUV(uv.u * uScale + uDelta, uv.v * vScale + vDelta);

		return texMap->GetAlpha(mapUV);
	}

protected:
	const TextureMap *texMap;
	float uScale, vScale, uDelta, vDelta;
};

class BumpMapInstance : public TexMapInstance {
public:
	BumpMapInstance(const TextureMap *tm, const float valueScale, const float uscale, const float vscale,
		const float udelta, const float vdelta) :
		TexMapInstance(tm, uscale, vscale, udelta, vdelta), scale(valueScale) {	
		DuDv.u = 1.f / (uScale * texMap->GetWidth());
		DuDv.v = 1.f / (vScale * texMap->GetHeight());
	}

	float GetScale() const { return scale; }
	const UV &GetDuDv() const { return DuDv; }

private:
	const float scale;
	UV DuDv;
};

class NormalMapInstance : public TexMapInstance {
public:
	NormalMapInstance(const TextureMap *tm, const float uscale, const float vscale,
		const float udelta, const float vdelta) :
		TexMapInstance(tm, uscale, vscale, udelta, vdelta) { }
};

class TextureMapCache {
public:
	TextureMapCache();
	~TextureMapCache();

	void DefineTexMap(const std::string &name, TextureMap *tm);

	TexMapInstance *GetTexMapInstance(const std::string &fileName, const float gamma,
		const float uScale = 1.f, const float vScale = 1.f, const float uDelta = 0.f, const float vDelta = 0.f);
	BumpMapInstance *GetBumpMapInstance(const std::string &fileName, const float scale,
		const float uScale = 1.f, const float vScale = 1.f, const float uDelta = 0.f, const float vDelta = 0.f);
	NormalMapInstance *GetNormalMapInstance(const std::string &fileName,
		const float uScale = 1.f, const float vScale = 1.f, const float uDelta = 0.f, const float vDelta = 0.f);

	void GetTexMaps(std::vector<TextureMap *> &tms);
	unsigned int GetSize()const { return static_cast<unsigned int>(maps.size()); }

	/**
	 * Method to retrieve an already cached diffuse map. This is used when adding an alpha map
	 * stored in a separate file.
	 */
	TextureMap *FindTextureMap(const std::string &fileName, const float gamma);
  
private:
	TextureMap *GetTextureMap(const std::string &fileName, const float gamma);

	std::map<std::string, TextureMap *> maps;
	std::vector<TexMapInstance *> texInstances;
	std::vector<BumpMapInstance *> bumpInstances;
	std::vector<NormalMapInstance *> normalInstances;
};

} }

#endif	/* _LUXRAYS_SDL_TEXMAP_H */
