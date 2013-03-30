/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#ifndef _SLG_TEXTURE_H
#define	_SLG_TEXTURE_H

#include <string>
#include <vector>
#include <map>
#include <set>

#include <boost/lexical_cast.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/utils/properties.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/spectrum.h"
#include "slg/sdl/mapping.h"
#include "slg/sdl/hitpoint.h"

namespace slg {

// OpenCL data types
namespace ocl {
using luxrays::ocl::Spectrum;
#include "slg/sdl/texture_types.cl"
}

//------------------------------------------------------------------------------
// Texture
//------------------------------------------------------------------------------

typedef enum {
	CONST_FLOAT, CONST_FLOAT3, IMAGEMAP, SCALE_TEX, FRESNEL_APPROX_N,
	FRESNEL_APPROX_K, MIX_TEX, ADD_TEX, HITPOINTCOLOR, HITPOINTALPHA,
	HITPOINTGREY,
	// Procedural textures
	CHECKERBOARD2D, CHECKERBOARD3D, FBM_TEX, MARBLE, DOTS, BRICK, WINDY,
	WRINKLED, UV_TEX, BAND_TEX
} TextureType;

class ImageMapCache;

class Texture {
public:
	Texture() { }
	virtual ~Texture() { }

	std::string GetName() const { return "texture-" + boost::lexical_cast<std::string>(this); }
	virtual TextureType GetType() const = 0;

	virtual float GetFloatValue(const HitPoint &hitPoint) const = 0;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const = 0;

	// Used for bump mapping support
	virtual luxrays::UV GetDuDv() const = 0;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		referencedTexs.insert(this);
	}
	
	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const = 0;
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
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const { return luxrays::Spectrum(value); }

	virtual luxrays::UV GetDuDv() const { return luxrays::UV(0.f, 0.f); }

	float GetValue() const { return value; };

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	float value;
};

class ConstFloat3Texture : public Texture {
public:
	ConstFloat3Texture(const luxrays::Spectrum &c) : color(c) { }
	virtual ~ConstFloat3Texture() { }

	virtual TextureType GetType() const { return CONST_FLOAT3; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const { return color.Y(); }
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const { return color; }

	virtual luxrays::UV GetDuDv() const { return luxrays::UV(0.f, 0.f); }

	const luxrays::Spectrum &GetColor() const { return color; };

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	luxrays::Spectrum color;
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

	float GetFloat(const luxrays::UV &uv) const {
		const float s = uv.u * width - .5f;
		const float t = uv.v * height - .5f;

		const int s0 = luxrays::Floor2Int(s);
		const int t0 = luxrays::Floor2Int(t);

		const float ds = s - s0;
		const float dt = t - t0;

		const float ids = 1.f - ds;
		const float idt = 1.f - dt;

		return ids * idt * GetFloatTexel(s0, t0) +
				ids * dt * GetFloatTexel(s0, t0 + 1) +
				ds * idt * GetFloatTexel(s0 + 1, t0) +
				ds * dt * GetFloatTexel(s0 + 1, t0 + 1);
	}

	luxrays::Spectrum GetSpectrum(const luxrays::UV &uv) const {
		const float s = uv.u * width - .5f;
		const float t = uv.v * height - .5f;

		const int s0 = luxrays::Floor2Int(s);
		const int t0 = luxrays::Floor2Int(t);

		const float ds = s - s0;
		const float dt = t - t0;

		const float ids = 1.f - ds;
		const float idt = 1.f - dt;

		return ids * idt * GetSpectrumTexel(s0, t0) +
				ids * dt * GetSpectrumTexel(s0, t0 + 1) +
				ds * idt * GetSpectrumTexel(s0 + 1, t0) +
				ds * dt * GetSpectrumTexel(s0 + 1, t0 + 1);
	};

	float GetAlpha(const luxrays::UV &uv) const {
		const float s = uv.u * width - .5f;
		const float t = uv.v * height - .5f;

		const int s0 = luxrays::Floor2Int(s);
		const int t0 = luxrays::Floor2Int(t);

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
		const u_int u = luxrays::Mod<int>(s, width);
		const u_int v = luxrays::Mod<int>(t, height);

		const unsigned index = v * width + u;
		assert (index >= 0);
		assert (index < width * height);

		if (channelCount == 1)
			return pixels[index];
		else {
			// channelCount = (3 or 4)
			const float *pixel = &pixels[index * channelCount];
			return luxrays::Spectrum(pixel[0], pixel[1], pixel[2]).Y();
		}
	}

	luxrays::Spectrum GetSpectrumTexel(const int s, const int t) const {
		const u_int u = luxrays::Mod<int>(s, width);
		const u_int v = luxrays::Mod<int>(t, height);

		const unsigned index = v * width + u;
		assert (index >= 0);
		assert (index < width * height);

		if (channelCount == 1) 
			return luxrays::Spectrum(pixels[index]);
		else {
			// channelCount = (3 or 4)
			const float *pixel = &pixels[index * channelCount];
			return luxrays::Spectrum(pixel[0], pixel[1], pixel[2]);
		}
	}

	float GetAlphaTexel(const int s, const int t) const {
		if (channelCount == 4) {
			const u_int u = luxrays::Mod<int>(s, width);
			const u_int v = luxrays::Mod<int>(t, height);

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
	ImageMapTexture(const ImageMap* im, const TextureMapping2D *mp, const float g);
	virtual ~ImageMapTexture() { delete mapping; }

	virtual TextureType GetType() const { return IMAGEMAP; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const { return DuDv; }

	const ImageMap *GetImageMap() const { return imgMap; }
	const TextureMapping2D *GetTextureMapping() const { return mapping; }
	const float GetGain() const { return gain; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const ImageMap *imgMap;
	const TextureMapping2D *mapping;
	float gain;
	luxrays::UV DuDv;
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
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}

	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

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
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
	}

	const Texture *GetTexture() const { return tex; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *tex;
};

class FresnelApproxKTexture : public Texture {
public:
	FresnelApproxKTexture(const Texture *t) : tex(t) { }
	virtual ~FresnelApproxKTexture() { }

	virtual TextureType GetType() const { return FRESNEL_APPROX_K; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
	}

	const Texture *GetTexture() const { return tex; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *tex;
};

//------------------------------------------------------------------------------
// CheckerBoard 2D & 3D texture
//------------------------------------------------------------------------------

class CheckerBoard2DTexture : public Texture {
public:
	CheckerBoard2DTexture(const TextureMapping2D *mp, const Texture *t1, const Texture *t2) : mapping(mp), tex1(t1), tex2(t2) { }
	virtual ~CheckerBoard2DTexture() { delete mapping; }

	virtual TextureType GetType() const { return CHECKERBOARD2D; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}

	const TextureMapping2D *GetTextureMapping() const { return mapping; }
	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping2D *mapping;
	const Texture *tex1;
	const Texture *tex2;
};

class CheckerBoard3DTexture : public Texture {
public:
	CheckerBoard3DTexture(const TextureMapping3D *mp, const Texture *t1, const Texture *t2) : mapping(mp), tex1(t1), tex2(t2) { }
	virtual ~CheckerBoard3DTexture() { delete mapping; }

	virtual TextureType GetType() const { return CHECKERBOARD3D; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
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
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		amount->AddReferencedTextures(referencedTexs);
		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}

	const Texture *GetAmountTexture() const { return amount; }
	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

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
	FBMTexture(const TextureMapping3D *mp, const int octs, const float omg) :
		mapping(mp), octaves(octs), omega(omg) { }
	virtual ~FBMTexture() { delete mapping; }

	virtual TextureType GetType() const { return FBM_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	int GetOctaves() const { return octaves; }
	float GetOmega() const { return omega; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	const int octaves;
	const float omega;
};

//------------------------------------------------------------------------------
// Marble texture
//------------------------------------------------------------------------------

class MarbleTexture : public Texture {
public:
	MarbleTexture(const TextureMapping3D *mp, const int octs, const float omg,
			float sc, float var) :
			mapping(mp), octaves(octs), omega(omg), scale(sc), variation(var) { }
	virtual ~MarbleTexture() { delete mapping; }

	virtual TextureType GetType() const { return MARBLE; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	int GetOctaves() const { return octaves; }
	float GetOmega() const { return omega; }
	float GetScale() const { return scale; }
	float GetVariation() const { return variation; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	const int octaves;
	const float omega, scale, variation;
};

//------------------------------------------------------------------------------
// Dots texture
//------------------------------------------------------------------------------

class DotsTexture : public Texture {
public:
	DotsTexture(const TextureMapping2D *mp, const Texture *insideTx, const Texture *outsideTx) :
		mapping(mp), insideTex(insideTx), outsideTex(outsideTx) { }
	virtual ~DotsTexture() { delete mapping; }

	virtual TextureType GetType() const { return DOTS; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		insideTex->AddReferencedTextures(referencedTexs);
		outsideTex->AddReferencedTextures(referencedTexs);
	}

	const TextureMapping2D *GetTextureMapping() const { return mapping; }
	const Texture *GetInsideTex() const { return insideTex; }
	const Texture *GetOutsideTex() const { return outsideTex; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping2D *mapping;
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
	BrickTexture(const TextureMapping3D *mp, const Texture *t1,
			const Texture *t2, const Texture *t3,
			float brickw, float brickh, float brickd, float mortar,
			float r, float bev, const std::string &b);
	virtual ~BrickTexture() { delete mapping; }

	virtual TextureType GetType() const { return BRICK; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
		tex3->AddReferencedTextures(referencedTexs);
	}

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }
	const Texture *GetTexture3() const { return tex3; }
	MasonryBond GetBond() const { return bond; }
	const luxrays::Point &GetOffset() const { return offset; }
	float GetBrickWidth() const { return initialbrickwidth; }
	float GetBrickHeight() const { return initialbrickheight; }
	float GetBrickDepth() const { return initialbrickdepth; }
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

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	bool RunningAlternate(const luxrays::Point &p, luxrays::Point &i, luxrays::Point &b, int nWhole) const;
	bool Basket(const luxrays::Point &p, luxrays::Point &i) const;
	bool Herringbone(const luxrays::Point &p, luxrays::Point &i) const;
	bool Running(const luxrays::Point &p, luxrays::Point &i, luxrays::Point &b) const;
	bool English(const luxrays::Point &p, luxrays::Point &i, luxrays::Point &b) const;

	const TextureMapping3D *mapping;
	const Texture *tex1, *tex2, *tex3;

	MasonryBond bond;
	luxrays::Point offset;
	float brickwidth, brickheight, brickdepth, mortarsize;
	float proportion, invproportion, run;
	float mortarwidth, mortarheight, mortardepth;
	float bevelwidth, bevelheight, beveldepth;
	bool usebevel;

	// brickwidth, brickheight, brickdepth are modified by HERRINGBONE
	// and BASKET brick types. I need to save the initial values here.
	float initialbrickwidth, initialbrickheight, initialbrickdepth;
};

//------------------------------------------------------------------------------
// Add texture
//------------------------------------------------------------------------------

class AddTexture : public Texture {
public:
	AddTexture(const Texture *t1, const Texture *t2) : tex1(t1), tex2(t2) { }
	virtual ~AddTexture() { }

	virtual TextureType GetType() const { return ADD_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}

	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *tex1;
	const Texture *tex2;
};

//------------------------------------------------------------------------------
// Windy texture
//------------------------------------------------------------------------------

class WindyTexture : public Texture {
public:
	WindyTexture(const TextureMapping3D *mp) : mapping(mp) { }
	virtual ~WindyTexture() { }

	virtual TextureType GetType() const { return WINDY; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	const TextureMapping3D *GetTextureMapping() const { return mapping; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
};

//------------------------------------------------------------------------------
// Wrinkled texture
//------------------------------------------------------------------------------

class WrinkledTexture : public Texture {
public:
	WrinkledTexture(const TextureMapping3D *mp, const int octs, const float omg) :
		mapping(mp), octaves(octs), omega(omg) { }
	virtual ~WrinkledTexture() { delete mapping; }

	virtual TextureType GetType() const { return WRINKLED; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	const TextureMapping3D *GetTextureMapping() const { return mapping; }
	int GetOctaves() const { return octaves; }
	float GetOmega() const { return omega; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping3D *mapping;
	const int octaves;
	const float omega;
};

//------------------------------------------------------------------------------
// UV texture
//------------------------------------------------------------------------------

class UVTexture : public Texture {
public:
	UVTexture(const TextureMapping2D *mp) : mapping(mp) { }
	virtual ~UVTexture() { delete mapping; }

	virtual TextureType GetType() const { return UV_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	const TextureMapping2D *GetTextureMapping() const { return mapping; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const TextureMapping2D *mapping;
};

//------------------------------------------------------------------------------
// Band texture
//------------------------------------------------------------------------------

class BandTexture : public Texture {
public:
	BandTexture(const Texture *amnt, const std::vector<float> &os, const std::vector<luxrays::Spectrum> &vs) :
		amount(amnt), offsets(os), values(vs) { }
	virtual ~BandTexture() { }

	virtual TextureType GetType() const { return BAND_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const;

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		amount->AddReferencedTextures(referencedTexs);
	}

	const Texture *GetAmountTexture() const { return amount; }
	const std::vector<float> &GetOffsets() const { return offsets; }
	const std::vector<luxrays::Spectrum> &GetValues() const { return values; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *amount;
	const std::vector<float> offsets;
	const std::vector<luxrays::Spectrum> values; 
};

//------------------------------------------------------------------------------
// HitPointColor texture
//------------------------------------------------------------------------------

class HitPointColorTexture : public Texture {
public:
	HitPointColorTexture() { }
	virtual ~HitPointColorTexture() { }

	virtual TextureType GetType() const { return HITPOINTCOLOR; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const { return luxrays::UV(0.f, 0.f); }

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;
};

//------------------------------------------------------------------------------
// HitPointAlpha texture
//------------------------------------------------------------------------------

class HitPointAlphaTexture : public Texture {
public:
	HitPointAlphaTexture() { }
	virtual ~HitPointAlphaTexture() { }

	virtual TextureType GetType() const { return HITPOINTALPHA; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const { return luxrays::UV(0.f, 0.f); }

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;
};

//------------------------------------------------------------------------------
// HitPointGrey texture
//------------------------------------------------------------------------------

class HitPointGreyTexture : public Texture {
public:
	HitPointGreyTexture(const u_int ch) : channel(ch) { }
	virtual ~HitPointGreyTexture() { }

	virtual TextureType GetType() const { return HITPOINTGREY; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;

	virtual luxrays::UV GetDuDv() const { return luxrays::UV(0.f, 0.f); }

	virtual void AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);
	}

	u_int GetChannel() const { return channel; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	u_int channel;
};

}

#endif	/* _SLG_TEXTURE_H */
