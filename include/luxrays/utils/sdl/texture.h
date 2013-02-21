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

#ifndef _LUXRAYS_SDL_TEXTURE_H
#define	_LUXRAYS_SDL_TEXTURE_H

#include <string>
#include <vector>
#include <map>
#include <set>

#include <boost/lexical_cast.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/utils/core/spectrum.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/sdl/mapping.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/utils/sdl/texture_types.cl"
}

namespace sdl {

//------------------------------------------------------------------------------
// Texture
//------------------------------------------------------------------------------

typedef enum {
	CONST_FLOAT, CONST_FLOAT3, IMAGEMAP, SCALE_TEX, FRESNEL_APPROX_N,
	FRESNEL_APPROX_K, MIX_TEX,
	// Procedural textures
	CHECKERBOARD2D, CHECKERBOARD3D, FBM_TEX, MARBLE, DOTS, BRICK
} TextureType;

struct HitPointStruct;
typedef HitPointStruct HitPoint;
class ImageMapCache;

class Texture {
public:
	Texture() { }
	virtual ~Texture() { }

	std::string GetName() const { return "texture-" + boost::lexical_cast<std::string>(this); }
	virtual TextureType GetType() const = 0;

	virtual float GetFloatValue(const HitPoint &hitPoint) const = 0;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const = 0;

	// Used for bump mapping support
	virtual UV GetDuDv() const = 0;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		referencedTexs.insert(this);
	}
	
	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const = 0;
};

//------------------------------------------------------------------------------
// TextureDefinitions
//------------------------------------------------------------------------------

class TextureDefinitions {
public:
	TextureDefinitions();
	~TextureDefinitions();

	bool IsTextureDefined(const std::string &name) const {
		return (texsByName.count(name) > 0);
	}
	void DefineTexture(const std::string &name, Texture *t);

	Texture *GetTexture(const std::string &name);
	Texture *GetTexture(const u_int index) {
		return texs[index];
	}
	u_int GetTextureIndex(const std::string &name);
	u_int GetTextureIndex(const Texture *t) const;

	u_int GetSize()const { return static_cast<u_int>(texs.size()); }
	std::vector<std::string> GetTextureNames() const;

	void DeleteTexture(const std::string &name);

private:

	std::vector<Texture *> texs;
	std::map<std::string, Texture *> texsByName;
};

//------------------------------------------------------------------------------
// Constant textures
//------------------------------------------------------------------------------

class ConstFloatTexture : public Texture {
public:
	ConstFloatTexture(const float v) : value(v) { }
	virtual ~ConstFloatTexture() { }

	virtual TextureType GetType() const { return CONST_FLOAT; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const { return value; }
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const { return Spectrum(value); }

	virtual UV GetDuDv() const { return UV(0.f, 0.f); }

	float GetValue() const { return value; };

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	float value;
};

class ConstFloat3Texture : public Texture {
public:
	ConstFloat3Texture(const Spectrum &c) : color(c) { }
	virtual ~ConstFloat3Texture() { }

	virtual TextureType GetType() const { return CONST_FLOAT3; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const { return color.Y(); }
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const { return color; }

	virtual UV GetDuDv() const { return UV(0.f, 0.f); }

	const Spectrum &GetColor() const { return color; };

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	Spectrum color;
};

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

class ImageMap {
public:
	ImageMap(const std::string &fileName, const float gamma);
	ImageMap(float *cols, const float gamma, const u_int channels, const u_int width, const u_int height);
	~ImageMap();

	float GetGamma() const { return gamma; }
	u_int GetChannelCount() const { return channelCount; }
	u_int GetWidth() const { return width; }
	u_int GetHeight() const { return height; }
	const float *GetPixels() const { return pixels; }

	void WriteImage(const std::string &fileName) const;

	float GetFloat(const UV &uv) const {
		const float s = uv.u * width - .5f;
		const float t = uv.v * height - .5f;

		const int s0 = Floor2Int(s);
		const int t0 = Floor2Int(t);

		const float ds = s - s0;
		const float dt = t - t0;

		const float ids = 1.f - ds;
		const float idt = 1.f - dt;

		return ids * idt * GetFloatTexel(s0, t0) +
				ids * dt * GetFloatTexel(s0, t0 + 1) +
				ds * idt * GetFloatTexel(s0 + 1, t0) +
				ds * dt * GetFloatTexel(s0 + 1, t0 + 1);
	}

	Spectrum GetSpectrum(const UV &uv) const {
		const float s = uv.u * width - .5f;
		const float t = uv.v * height - .5f;

		const int s0 = Floor2Int(s);
		const int t0 = Floor2Int(t);

		const float ds = s - s0;
		const float dt = t - t0;

		const float ids = 1.f - ds;
		const float idt = 1.f - dt;

		return ids * idt * GetSpectrumTexel(s0, t0) +
				ids * dt * GetSpectrumTexel(s0, t0 + 1) +
				ds * idt * GetSpectrumTexel(s0 + 1, t0) +
				ds * dt * GetSpectrumTexel(s0 + 1, t0 + 1);
	};

	float GetAlpha(const UV &uv) const {
		const float s = uv.u * width - .5f;
		const float t = uv.v * height - .5f;

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

private:
	float GetFloatTexel(const int s, const int t) const {
		const u_int u = Mod<int>(s, width);
		const u_int v = Mod<int>(t, height);

		const unsigned index = v * width + u;
		assert (index >= 0);
		assert (index < width * height);

		if (channelCount == 1)
			return pixels[index];
		else {
			// channelCount = (3 or 4)
			const float *pixel = &pixels[index * channelCount];
			return Spectrum(pixel[0], pixel[1], pixel[2]).Y();
		}
	}

	Spectrum GetSpectrumTexel(const int s, const int t) const {
		const u_int u = Mod<int>(s, width);
		const u_int v = Mod<int>(t, height);

		const unsigned index = v * width + u;
		assert (index >= 0);
		assert (index < width * height);

		if (channelCount == 1) 
			return Spectrum(pixels[index]);
		else {
			// channelCount = (3 or 4)
			const float *pixel = &pixels[index * channelCount];
			return Spectrum(pixel[0], pixel[1], pixel[2]);
		}
	}

	float GetAlphaTexel(const int s, const int t) const {
		if (channelCount == 4) {
			const u_int u = Mod<int>(s, width);
			const u_int v = Mod<int>(t, height);

			const unsigned index = v * width + u;
			assert (index >= 0);
			assert (index < width * height);

			return pixels[index * channelCount + 3];
		} else
			return 1.f;
	}

	float gamma;
	u_int channelCount, width, height;
	float *pixels;
};

class ImageMapCache {
public:
	ImageMapCache();
	~ImageMapCache();

	void DefineImgMap(const std::string &name, ImageMap *im);

	ImageMap *GetImageMap(const std::string &fileName, const float gamma);
	u_int GetImageMapIndex(const ImageMap *im) const;

	void GetImageMaps(std::vector<ImageMap *> &ims);
	u_int GetSize()const { return static_cast<u_int>(maps.size()); }
	bool IsImageMapDefined(const std::string &name) const { return maps.find(name) != maps.end(); }

private:
	std::map<std::string, ImageMap *> maps;
};

class ImageMapTexture : public Texture {
public:
	ImageMapTexture(const ImageMap* im, const TextureMapping *mp, const float g);
	virtual ~ImageMapTexture() { delete mapping; }

	virtual TextureType GetType() const { return IMAGEMAP; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual UV GetDuDv() const { return DuDv; }

	const ImageMap *GetImageMap() const { return imgMap; }
	const TextureMapping *GetTextureMapping() const { return mapping; }
	const float GetGain() const { return gain; }

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const ImageMap *imgMap;
	const TextureMapping *mapping;
	float gain;
	UV DuDv;
};

//------------------------------------------------------------------------------
// Scale texture
//------------------------------------------------------------------------------

class ScaleTexture : public Texture {
public:
	ScaleTexture(const Texture *t1, const Texture *t2) : tex1(t1), tex2(t2) { }
	virtual ~ScaleTexture() { }

	virtual TextureType GetType() const { return SCALE_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}

	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *tex1;
	const Texture *tex2;
};

//------------------------------------------------------------------------------
// FresnelApproxN & FresnelApproxK texture
//
// Used mostly to emulate LuxRender FresnelColor texture.
//------------------------------------------------------------------------------

class FresnelApproxNTexture : public Texture {
public:
	FresnelApproxNTexture(const Texture *t) : tex(t) { }
	virtual ~FresnelApproxNTexture() { }

	virtual TextureType GetType() const { return FRESNEL_APPROX_N; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
	}

	const Texture *GetTexture() const { return tex; }

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *tex;
};

class FresnelApproxKTexture : public Texture {
public:
	FresnelApproxKTexture(const Texture *t) : tex(t) { }
	virtual ~FresnelApproxKTexture() { }

	virtual TextureType GetType() const { return FRESNEL_APPROX_K; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
	}

	const Texture *GetTexture() const { return tex; }

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *tex;
};

//------------------------------------------------------------------------------
// CheckerBoard 2D & 3D texture
//------------------------------------------------------------------------------

class CheckerBoard2DTexture : public Texture {
public:
	CheckerBoard2DTexture(const TextureMapping *mp, const Texture *t1, const Texture *t2) : mapping(mp), tex1(t1), tex2(t2) { }
	virtual ~CheckerBoard2DTexture() { delete mapping; }

	virtual TextureType GetType() const { return CHECKERBOARD2D; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}

	const TextureMapping *GetTextureMapping() const { return mapping; }
	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping *mapping;
	const Texture *tex1;
	const Texture *tex2;
};

class CheckerBoard3DTexture : public Texture {
public:
	CheckerBoard3DTexture(const TextureMapping *mp, const Texture *t1, const Texture *t2) : mapping(mp), tex1(t1), tex2(t2) { }
	virtual ~CheckerBoard3DTexture() { delete mapping; }

	virtual TextureType GetType() const { return CHECKERBOARD3D; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}

	const TextureMapping *GetTextureMapping() const { return mapping; }
	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping *mapping;
	const Texture *tex1;
	const Texture *tex2;
};

//------------------------------------------------------------------------------
// Mix texture
//------------------------------------------------------------------------------

class MixTexture : public Texture {
public:
	MixTexture(const Texture *amnt, const Texture *t1, const Texture *t2) :
		amount(amnt), tex1(t1), tex2(t2) { }
	virtual ~MixTexture() { }

	virtual TextureType GetType() const { return MIX_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		amount->AddReferencedTextures(referencedTexs);
		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}

	const Texture *GetAmountTexture() const { return amount; }
	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *amount;
	const Texture *tex1;
	const Texture *tex2;
};

//------------------------------------------------------------------------------
// FBM texture
//------------------------------------------------------------------------------

class FBMTexture : public Texture {
public:
	FBMTexture(const TextureMapping *mp, const int octs, const float omg) :
		mapping(mp), octaves(octs), omega(omg) { }
	virtual ~FBMTexture() { delete mapping; }

	virtual TextureType GetType() const { return FBM_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;


	virtual UV GetDuDv() const;

	const TextureMapping *GetTextureMapping() const { return mapping; }
	int GetOctaves() const { return octaves; }
	float GetOmega() const { return omega; }

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping *mapping;
	const int octaves;
	const float omega;
};

//------------------------------------------------------------------------------
// Marble texture
//------------------------------------------------------------------------------

class MarbleTexture : public Texture {
public:
	MarbleTexture(const TextureMapping *mp, const int octs, const float omg,
			float sc, float var) :
			mapping(mp), octaves(octs), omega(omg), scale(sc), variation(var) { }
	virtual ~MarbleTexture() { delete mapping; }

	virtual TextureType GetType() const { return MARBLE; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual UV GetDuDv() const;

	const TextureMapping *GetTextureMapping() const { return mapping; }
	int GetOctaves() const { return octaves; }
	float GetOmega() const { return omega; }
	float GetScale() const { return scale; }
	float GetVariation() const { return variation; }

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping *mapping;
	const int octaves;
	const float omega, scale, variation;
};

//------------------------------------------------------------------------------
// Dots texture
//------------------------------------------------------------------------------

class DotsTexture : public Texture {
public:
	DotsTexture(const TextureMapping *mp, const Texture *insideTx, const Texture *outsideTx) :
		mapping(mp), insideTex(insideTx), outsideTex(outsideTx) { }
	virtual ~DotsTexture() { delete mapping; }

	virtual TextureType GetType() const { return DOTS; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		insideTex->AddReferencedTextures(referencedTexs);
		outsideTex->AddReferencedTextures(referencedTexs);
	}

	const TextureMapping *GetTextureMapping() const { return mapping; }
	const Texture *GetInsideTex() const { return insideTex; }
	const Texture *GetOutsideTex() const { return outsideTex; }

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping *mapping;
	const Texture *insideTex;
	const Texture *outsideTex;
};

//------------------------------------------------------------------------------
// Brick texture
//------------------------------------------------------------------------------

typedef enum {
	FLEMISH, RUNNING, ENGLISH, HERRINGBONE, BASKET, KETTING
} MasonryBond;

class BrickTexture : public Texture {
public:
	BrickTexture(const TextureMapping *mp, const Texture *t1,
			const Texture *t2, const Texture *t3,
			float brickw, float brickh, float brickd, float mortar,
			float r, float bev, const std::string &b);
	virtual ~BrickTexture() { delete mapping; }

	virtual TextureType GetType() const { return BRICK; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
		tex3->AddReferencedTextures(referencedTexs);
	}

	const TextureMapping *GetTextureMapping() const { return mapping; }
	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }
	const Texture *GetTexture3() const { return tex3; }
	MasonryBond GetBond() const { return bond; }
	const Point &GetOffset() const { return offset; }
	float GetBrickWidth() const { return brickwidth; }
	float GetBrickHeight() const { return brickheight; }
	float GetBrickDepth() const { return brickdepth; }
	float GetMortarSize() const { return mortarsize; }
	float GetProportion() const { return proportion; }
	float GetInvProportion() const { return invproportion; }
	float GetRun() const { return run; }
	float GetMortarWidth() const { return mortarwidth; }
	float GetMortarHeight() const { return mortarheight; }
	float GetMortarDepth() const { return mortardepth; }
	float GetBevelWidth() const { return bevelwidth; }
	float GetBevelHeight() const { return bevelheight; }
	float GetBevelDepth() const { return beveldepth; }
	bool GetUseBevel() const { return usebevel; }

	virtual Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	bool RunningAlternate(const Point &p, Point &i, Point &b, int nWhole) const;
	bool Basket(const Point &p, Point &i) const;
	bool Herringbone(const Point &p, Point &i) const;
	bool Running(const Point &p, Point &i, Point &b) const;
	bool English(const Point &p, Point &i, Point &b) const;

	const TextureMapping *mapping;
	const Texture *tex1, *tex2, *tex3;

	MasonryBond bond;
	Point offset;
	float brickwidth, brickheight, brickdepth, mortarsize;
	float proportion, invproportion, run;
	float mortarwidth, mortarheight, mortardepth;
	float bevelwidth, bevelheight, beveldepth;
	bool usebevel;
};

} }

#endif	/* _LUXRAYS_SDL_TEXTURE_H */
