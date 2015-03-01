/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_TEXTURE_H
#define	_SLG_TEXTURE_H

#include <OpenEXR/half.h>
#include <OpenImageIO/imageio.h>
OIIO_NAMESPACE_USING

#include <string>
#include <vector>
#include <map>
#include <set>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/utils/properties.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/color/color.h"
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
	FRESNEL_APPROX_K, MIX_TEX, ADD_TEX, SUBTRACT_TEX, HITPOINTCOLOR, HITPOINTALPHA,
	HITPOINTGREY, NORMALMAP_TEX,
	// Procedural textures
	BLENDER_BLEND, BLENDER_CLOUDS, BLENDER_DISTORTED_NOISE, BLENDER_MAGIC, BLENDER_MARBLE,
	BLENDER_MUSGRAVE, BLENDER_NOISE, BLENDER_STUCCI, BLENDER_WOOD,  BLENDER_VORONOI,
	CHECKERBOARD2D, CHECKERBOARD3D, FBM_TEX,
	MARBLE, DOTS, BRICK, WINDY, WRINKLED, UV_TEX, BAND_TEX
} TextureType;

class ImageMap;
class ImageMapCache;

class Texture {
public:
	Texture() { }
	virtual ~Texture() { }

	std::string GetName() const { return "texture-" + boost::lexical_cast<std::string>(this); }
	virtual TextureType GetType() const = 0;

	virtual float GetFloatValue(const HitPoint &hitPoint) const = 0;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const = 0;
	virtual float Y() const = 0;
	virtual float Filter() const = 0;

	// Used for bump/normal mapping support
    virtual luxrays::UV GetDuv(const HitPoint &hitPoint,
        const luxrays::Vector &dpdu, const luxrays::Vector &dpdv,
        const luxrays::Normal &dndu, const luxrays::Normal &dndv,
        const float sampleDistance) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		referencedTexs.insert(this);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const = 0;
};

float Noise(float x, float y = .5f, float z = .5f);
inline float Noise(const luxrays::Point &P) {
	return Noise(P.x, P.y, P.z);
}

float tex_sin(float a);
float tex_saw(float a);
float tex_tri(float a);
float Turbulence(const luxrays::Point &P, const float omega, const int maxOctaves);

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
	boost::unordered_map<std::string, Texture *> texsByName;
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
	virtual float Y() const { return value; }
	virtual float Filter() const { return value; }

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
	virtual float Y() const { return color.Y(); }
	virtual float Filter() const { return color.Filter(); }

	const luxrays::Spectrum &GetColor() const { return color; };

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	luxrays::Spectrum color;
};

//------------------------------------------------------------------------------
// ImageMapPixel
//------------------------------------------------------------------------------

template <class T, u_int CHANNELS> class ImageMapPixel {
public:
	ImageMapPixel(T v = 0) {
		for (u_int i = 0; i < CHANNELS; ++i)
			c[i] = v;
	}

	ImageMapPixel(T cs[CHANNELS]) {
		for (u_int i = 0; i < CHANNELS; ++i)
			c[i] = cs[i];
	}

	~ImageMapPixel() { }
	
	u_int GetChannelCount() const { return CHANNELS; }

	float GetFloat() const;
	luxrays::Spectrum GetSpectrum() const;
	float GetAlpha() const;
	
	void ReverseGammaCorrection(const float gamma);

	T c[CHANNELS];
};

//------------------------------------------------------------------------------
// u_char x 1 specialization
//------------------------------------------------------------------------------

template<> inline float ImageMapPixel<u_char, 1>::GetFloat() const {
	const float norm = 1.f / std::numeric_limits<u_char>::max();
	return c[0] * norm;
}

template<> inline luxrays::Spectrum ImageMapPixel<u_char, 1>::GetSpectrum() const {
	const float norm = 1.f / std::numeric_limits<u_char>::max();
	return luxrays::Spectrum(c[0] * norm);
}

template<> inline float ImageMapPixel<u_char, 1>::GetAlpha() const {
	return 1.f;
}

template<> inline void ImageMapPixel<u_char, 1>::ReverseGammaCorrection(const float gamma) {
	const u_char maxv = std::numeric_limits<u_char>::max();
	const float norm = 1.f / maxv;
	c[0] = (u_char)floorf(powf(c[0] * norm, gamma) * maxv + .5f);
}

//------------------------------------------------------------------------------
// u_char x 3 specialization
//------------------------------------------------------------------------------

template<> inline float ImageMapPixel<u_char, 3>::GetFloat() const {
	const float norm = 1.f / std::numeric_limits<u_char>::max();
	return luxrays::Spectrum(c[0] * norm, c[1] * norm, c[2] * norm).Y();
}

template<> inline luxrays::Spectrum ImageMapPixel<u_char, 3>::GetSpectrum() const {
	const float norm = 1.f / std::numeric_limits<u_char>::max();
	return luxrays::Spectrum(c[0] * norm, c[1] * norm, c[2] * norm);
}

template<> inline float ImageMapPixel<u_char, 3>::GetAlpha() const {
	return 1.f;
}

template<> inline void ImageMapPixel<u_char, 3>::ReverseGammaCorrection(const float gamma) {
	const u_char maxv = std::numeric_limits<u_char>::max();
	const float norm = 1.f / maxv;
	c[0] = (u_char)floorf(powf(c[0] * norm, gamma) * maxv + .5f);
	c[1] = (u_char)floorf(powf(c[1] * norm, gamma) * maxv + .5f);
	c[2] = (u_char)floorf(powf(c[2] * norm, gamma) * maxv + .5f);
}

//------------------------------------------------------------------------------
// u_char x 4 specialization
//------------------------------------------------------------------------------

template<> inline float ImageMapPixel<u_char, 4>::GetFloat() const {
	const float norm = 1.f / std::numeric_limits<u_char>::max();
	return luxrays::Spectrum(c[0] * norm, c[1] * norm, c[2] * norm).Y();
}

template<> inline luxrays::Spectrum ImageMapPixel<u_char, 4>::GetSpectrum() const {
	const float norm = 1.f / std::numeric_limits<u_char>::max();
	return luxrays::Spectrum(c[0] * norm, c[1] * norm, c[2] * norm);
}

template<> inline float ImageMapPixel<u_char, 4>::GetAlpha() const {
	const float norm = 1.f / std::numeric_limits<u_char>::max();
	return c[3] * norm;
}

template<> inline void ImageMapPixel<u_char, 4>::ReverseGammaCorrection(const float gamma) {
	const u_char maxv = std::numeric_limits<u_char>::max();
	const float norm = 1.f / maxv;
	c[0] = (u_char)floorf(powf(c[0] * norm, gamma) * maxv + .5f);
	c[1] = (u_char)floorf(powf(c[1] * norm, gamma) * maxv + .5f);
	c[2] = (u_char)floorf(powf(c[2] * norm, gamma) * maxv + .5f);
}

//------------------------------------------------------------------------------
// half x 1 specialization
//------------------------------------------------------------------------------

template<> inline float ImageMapPixel<half, 1>::GetFloat() const {
	return c[0];
}

template<> inline luxrays::Spectrum ImageMapPixel<half, 1>::GetSpectrum() const {
	return luxrays::Spectrum(c[0]);
}

template<> inline float ImageMapPixel<half, 1>::GetAlpha() const {
	return 1.f;
}

template<> inline void ImageMapPixel<half, 1>::ReverseGammaCorrection(const float gamma) {
	c[0] = powf(c[0], gamma);
}

//------------------------------------------------------------------------------
// half x 3 specialization
//------------------------------------------------------------------------------

template<> inline float ImageMapPixel<half, 3>::GetFloat() const {
	return luxrays::Spectrum(c[0], c[1], c[2]).Y();
}

template<> inline luxrays::Spectrum ImageMapPixel<half, 3>::GetSpectrum() const {
	return luxrays::Spectrum(c[0], c[1], c[2]);
}

template<> inline float ImageMapPixel<half, 3>::GetAlpha() const {
	return 1.f;
}

template<> inline void ImageMapPixel<half, 3>::ReverseGammaCorrection(const float gamma) {
	c[0] = powf(c[0], gamma);
	c[1] = powf(c[1], gamma);
	c[2] = powf(c[2], gamma);
}

//------------------------------------------------------------------------------
// half x 4 specialization
//------------------------------------------------------------------------------

template<> inline float ImageMapPixel<half, 4>::GetFloat() const {
	return luxrays::Spectrum(c[0], c[1], c[2]).Y();
}

template<> inline luxrays::Spectrum ImageMapPixel<half, 4>::GetSpectrum() const {
	return luxrays::Spectrum(c[0], c[1], c[2]);
}

template<> inline float ImageMapPixel<half, 4>::GetAlpha() const {
	return c[3];
}

template<> inline void ImageMapPixel<half, 4>::ReverseGammaCorrection(const float gamma) {
	c[0] = powf(c[0], gamma);
	c[1] = powf(c[1], gamma);
	c[2] = powf(c[2], gamma);
}

//------------------------------------------------------------------------------
// float x 1 specialization
//------------------------------------------------------------------------------

template<> inline float ImageMapPixel<float, 1>::GetFloat() const {
	return c[0];
}

template<> inline luxrays::Spectrum ImageMapPixel<float, 1>::GetSpectrum() const {
	return luxrays::Spectrum(c[0]);
}

template<> inline float ImageMapPixel<float, 1>::GetAlpha() const {
	return 1.f;
}

template<> inline void ImageMapPixel<float, 1>::ReverseGammaCorrection(const float gamma) {
	c[0] = powf(c[0], gamma);
}

//------------------------------------------------------------------------------
// float x 3 specialization
//------------------------------------------------------------------------------

template<> inline float ImageMapPixel<float, 3>::GetFloat() const {
	return luxrays::Spectrum(c[0], c[1], c[2]).Y();
}

template<> inline luxrays::Spectrum ImageMapPixel<float, 3>::GetSpectrum() const {
	return luxrays::Spectrum(c[0], c[1], c[2]);
}

template<> inline float ImageMapPixel<float, 3>::GetAlpha() const {
	return 1.f;
}

template<> inline void ImageMapPixel<float, 3>::ReverseGammaCorrection(const float gamma) {
	c[0] = powf(c[0], gamma);
	c[1] = powf(c[1], gamma);
	c[2] = powf(c[2], gamma);
}

//------------------------------------------------------------------------------
// float x 4 specialization
//------------------------------------------------------------------------------

template<> inline float ImageMapPixel<float, 4>::GetFloat() const {
	return luxrays::Spectrum(c[0], c[1], c[2]).Y();
}

template<> inline luxrays::Spectrum ImageMapPixel<float, 4>::GetSpectrum() const {
	return luxrays::Spectrum(c[0], c[1], c[2]);
}

template<> inline float ImageMapPixel<float, 4>::GetAlpha() const {
	return c[3];
}

template<> inline void ImageMapPixel<float, 4>::ReverseGammaCorrection(const float gamma) {
	c[0] = powf(c[0], gamma);
	c[1] = powf(c[1], gamma);
	c[2] = powf(c[2], gamma);
}

//------------------------------------------------------------------------------
// ImageMapStorage
//------------------------------------------------------------------------------

class ImageMapStorage {
public:
	typedef enum {
		BYTE,
		HALF,
		FLOAT
	} StorageType;

	ImageMapStorage(const u_int w, const u_int h) {
		width = w;
		height = h;
	}
	virtual ~ImageMapStorage() { }

	virtual StorageType GetStorageType() const = 0;
	virtual u_int GetChannelCount() const = 0;
	virtual void *GetPixelsData() const = 0;

	virtual float GetFloat(const luxrays::UV &uv) const = 0;
	virtual float GetFloat(const u_int index) const = 0;
	virtual luxrays::Spectrum GetSpectrum(const luxrays::UV &uv) const = 0;
	virtual luxrays::Spectrum GetSpectrum(const u_int index) const = 0;
	virtual float GetAlpha(const luxrays::UV &uv) const = 0;
	virtual float GetAlpha(const u_int index) const = 0;

	virtual void ReverseGammaCorrection(const float gamma) = 0;

	u_int width, height;	
};

template <class T, u_int CHANNELS> class ImageMapStorageImpl : public ImageMapStorage {
public:
	ImageMapStorageImpl(ImageMapPixel<T, CHANNELS> *ps, const u_int w, const u_int h) :
		ImageMapStorage(w, h), pixels(ps) { }
	virtual ~ImageMapStorageImpl() { delete[] pixels; }

	virtual StorageType GetStorageType() const;
	virtual u_int GetChannelCount() const { return CHANNELS; }
	virtual void *GetPixelsData() const { return pixels; }

	virtual float GetFloat(const luxrays::UV &uv) const;
	virtual float GetFloat(const u_int index) const;
	virtual luxrays::Spectrum GetSpectrum(const luxrays::UV &uv) const;
	virtual luxrays::Spectrum GetSpectrum(const u_int index) const;
	virtual float GetAlpha(const luxrays::UV &uv) const;
	virtual float GetAlpha(const u_int index) const;

	virtual void ReverseGammaCorrection(const float gamma);

private:
	const ImageMapPixel<T, CHANNELS> *GetTexel(const int s, const int t) const;

	ImageMapPixel<T, CHANNELS> *pixels;
};

template<> inline ImageMapStorage::StorageType ImageMapStorageImpl<u_char, 1>::GetStorageType() const {
	return ImageMapStorage::BYTE;
}

template<> inline ImageMapStorage::StorageType ImageMapStorageImpl<u_char, 3>::GetStorageType() const {
	return ImageMapStorage::BYTE;
}

template<> inline ImageMapStorage::StorageType ImageMapStorageImpl<u_char, 4>::GetStorageType() const {
	return ImageMapStorage::BYTE;
}

template<> inline ImageMapStorage::StorageType ImageMapStorageImpl<half, 1>::GetStorageType() const {
	return ImageMapStorage::HALF;
}

template<> inline ImageMapStorage::StorageType ImageMapStorageImpl<half, 3>::GetStorageType() const {
	return ImageMapStorage::HALF;
}

template<> inline ImageMapStorage::StorageType ImageMapStorageImpl<half, 4>::GetStorageType() const {
	return ImageMapStorage::HALF;
}

template<> inline ImageMapStorage::StorageType ImageMapStorageImpl<float, 1>::GetStorageType() const {
	return ImageMapStorage::FLOAT;
}

template<> inline ImageMapStorage::StorageType ImageMapStorageImpl<float, 3>::GetStorageType() const {
	return ImageMapStorage::FLOAT;
}

template<> inline ImageMapStorage::StorageType ImageMapStorageImpl<float, 4>::GetStorageType() const {
	return ImageMapStorage::FLOAT;
}

template <class T> ImageMapStorage *AllocImageMapStorage(T *pixels,
		const u_int channels, const u_int width, const u_int height) {
	switch (channels) {
		case 1:
			return new ImageMapStorageImpl<T, 1>((ImageMapPixel<T, 1> *)pixels, width, height);
		case 3:
			return new ImageMapStorageImpl<T, 3>((ImageMapPixel<T, 3> *)pixels, width, height);
		case 4:
			return new ImageMapStorageImpl<T, 4>((ImageMapPixel<T, 4> *)pixels, width, height);
		default:
			return NULL;
	}
}

//------------------------------------------------------------------------------
// ImageMap
//------------------------------------------------------------------------------

class ImageMap {
public:
	typedef enum {
		DEFAULT,
		RED,
		GREEN,
		BLUE,
		ALPHA,
		MEAN,
		WEIGHTED_MEAN,
		RGB
	} ChannelSelectionType;

	ImageMap(const std::string &fileName, const float gamma,
		const ChannelSelectionType selectionType);
	ImageMap(ImageMapStorage *pixels, const float gamma);
	~ImageMap();

	float GetGamma() const { return gamma; }
	u_int GetChannelCount() const { return pixelStorage->GetChannelCount(); }
	u_int GetWidth() const { return pixelStorage->width; }
	u_int GetHeight() const { return pixelStorage->height; }
	const ImageMapStorage *GetStorage() const { return pixelStorage; }

	float GetFloat(const luxrays::UV &uv) const { return pixelStorage->GetFloat(uv); }
	luxrays::Spectrum GetSpectrum(const luxrays::UV &uv) const { return pixelStorage->GetSpectrum(uv); }
	float GetAlpha(const luxrays::UV &uv) const { return pixelStorage->GetAlpha(uv); }

	void Resize(const u_int newWidth, const u_int newHeight);

	void WriteImage(const std::string &fileName) const;

	float GetSpectrumMean() const;
	float GetSpectrumMeanY() const;

	// The following 3 methods always return an ImageMap with FLOAT storage
	static ImageMap *Merge(const ImageMap *map0, const ImageMap *map1, const u_int channels);
	static ImageMap *Merge(const ImageMap *map0, const ImageMap *map1, const u_int channels,
		const u_int width, const u_int height);
	static ImageMap *Resample(const ImageMap *map, const u_int channels,
		const u_int width, const u_int height);
		
	// Mostly an utility allocator for compatibility with the past
	template <class T> static ImageMap *AllocImageMap(T *pixels, const float gamma, const u_int channels,
		const u_int width, const u_int height) {
		ImageMapStorage *imageMapStorage = AllocImageMapStorage<T>(pixels, channels, width, height);

		return new ImageMap(imageMapStorage, gamma);
	}

private:
	float gamma;
	ImageMapStorage *pixelStorage;
};

//------------------------------------------------------------------------------
// ImageMapCache
//------------------------------------------------------------------------------

class ImageMapCache {
public:
	ImageMapCache();
	~ImageMapCache();

	void SetImageResize(const float s) { allImageScale = s; }

	void DefineImageMap(const std::string &name, ImageMap *im);

	ImageMap *GetImageMap(const std::string &fileName, const float gamma,
		const ImageMap::ChannelSelectionType selectionType);

	// Get a path/name from imageMap object
	const std::string &GetPath(const ImageMap *im)const {
		for (boost::unordered_map<std::string, ImageMap *>::const_iterator it = mapByName.begin(); it != mapByName.end(); ++it) {
			if (it->second == im)
				return it->first;
		}
		throw std::runtime_error("Unknown ImageMap in ImageMapCache::GetPath()");
	}

	void DeleteImageMap(const ImageMap *im) {
		for (boost::unordered_map<std::string, ImageMap *>::iterator it = mapByName.begin(); it != mapByName.end(); ++it) {
			if (it->second == im) {
				delete it->second;

				maps.erase(std::find(maps.begin(), maps.end(), it->second));
				mapByName.erase(it);
				return;
			}
		}
	}

	u_int GetImageMapIndex(const ImageMap *im) const;

	void GetImageMaps(std::vector<const ImageMap *> &ims);
	u_int GetSize()const { return static_cast<u_int>(mapByName.size()); }
	bool IsImageMapDefined(const std::string &name) const { return mapByName.find(name) != mapByName.end(); }

private:
	std::string GetCacheKey(const std::string &fileName, const float gamma,
		const ImageMap::ChannelSelectionType selectionType) const;
	std::string GetCacheKey(const std::string &fileName) const;

	boost::unordered_map<std::string, ImageMap *> mapByName;
	// Used to preserve insertion order and to retrieve insertion index
	std::vector<ImageMap *> maps;

	float allImageScale;
};

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

class ImageMapTexture : public Texture {
public:
	ImageMapTexture(const ImageMap* im, const TextureMapping2D *mp, const float g);
	virtual ~ImageMapTexture() { delete mapping; }

	virtual TextureType GetType() const { return IMAGEMAP; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const { return imageY; }
	virtual float Filter() const { return imageFilter; }

	const ImageMap *GetImageMap() const { return imgMap; }
	const TextureMapping2D *GetTextureMapping() const { return mapping; }
	const float GetGain() const { return gain; }

	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		referencedImgMaps.insert(imgMap);
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const ImageMap *imgMap;
	const TextureMapping2D *mapping;
	float gain;
	// Cached image information
	float imageY, imageFilter;
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
	virtual float Y() const { return tex1->Y() * tex2->Y(); }
	virtual float Filter() const { return tex1->Filter() * tex2->Filter(); }

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex1->AddReferencedImageMaps(referencedImgMaps);
		tex2->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex1 == oldTex)
			tex1 = newTex;
		if (tex2 == oldTex)
			tex2 = newTex;
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
	virtual float Y() const;
	virtual float Filter() const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex == oldTex)
			tex = newTex;
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
	virtual float Y() const;
	virtual float Filter() const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex == oldTex)
			tex = newTex;
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
	virtual float Y() const { return (tex1->Y() + tex2->Y()) * .5f; }
	virtual float Filter() const { return (tex1->Filter() + tex2->Filter()) * .5f; }

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex1->AddReferencedImageMaps(referencedImgMaps);
		tex2->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex1 == oldTex)
			tex1 = newTex;
		if (tex2 == oldTex)
			tex2 = newTex;
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
	virtual float Y() const { return (tex1->Y() + tex2->Y()) * .5f; }
	virtual float Filter() const { return (tex1->Filter() + tex2->Filter()) * .5f; }

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex1->AddReferencedImageMaps(referencedImgMaps);
		tex2->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex1 == oldTex)
			tex1 = newTex;
		if (tex2 == oldTex)
			tex2 = newTex;
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
	virtual float Y() const;
	virtual float Filter() const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		amount->AddReferencedTextures(referencedTexs);
		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex1->AddReferencedImageMaps(referencedImgMaps);
		tex2->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (amount == oldTex)
			amount = newTex;
		if (tex1 == oldTex)
			tex1 = newTex;
		if (tex2 == oldTex)
			tex2 = newTex;
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
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

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
	virtual float Y() const;
	virtual float Filter() const;

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
	virtual float Y() const {
		return (insideTex->Y() + outsideTex->Y()) * .5f;
	}
	virtual float Filter() const {
		return (insideTex->Filter() + outsideTex->Filter()) * .5f;
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		insideTex->AddReferencedTextures(referencedTexs);
		outsideTex->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		insideTex->AddReferencedImageMaps(referencedImgMaps);
		outsideTex->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (insideTex == oldTex)
			insideTex = newTex;
		if (outsideTex == oldTex)
			outsideTex = newTex;
	}

	const TextureMapping2D *GetTextureMapping() const { return mapping; }
	const Texture *GetInsideTex() const { return insideTex; }
	const Texture *GetOutsideTex() const { return outsideTex; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	bool Evaluate(const HitPoint &hitPoint) const;

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
	virtual float Y() const {
		const float m = powf(luxrays::Clamp(1.f - mortarsize, 0.f, 1.f), 3);
		return luxrays::Lerp(m, tex2->Y(), tex1->Y());
	}
	virtual float Filter() const {
		const float m = powf(luxrays::Clamp(1.f - mortarsize, 0.f, 1.f), 3);
		return luxrays::Lerp(m, tex2->Filter(), tex1->Filter());
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
		tex3->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex1->AddReferencedImageMaps(referencedImgMaps);
		tex2->AddReferencedImageMaps(referencedImgMaps);
		tex3->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex1 == oldTex)
			tex1 = newTex;
		if (tex2 == oldTex)
			tex2 = newTex;
		if (tex3 == oldTex)
			tex3 = newTex;
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
	virtual float Y() const { 
		return tex1->Y() + tex2->Y(); 
	}
	virtual float Filter() const { 
		return tex1->Filter() + tex2->Filter(); 
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex1->AddReferencedImageMaps(referencedImgMaps);
		tex2->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex1 == oldTex)
			tex1 = newTex;
		if (tex2 == oldTex)
			tex2 = newTex;
	}

	const Texture *GetTexture1() const { return tex1; }
	const Texture *GetTexture2() const { return tex2; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *tex1;
	const Texture *tex2;
};

//------------------------------------------------------------------------------
// Subtract texture
//------------------------------------------------------------------------------

class SubtractTexture : public Texture {
public:
	SubtractTexture(const Texture *t1, const Texture *t2) : tex1(t1), tex2(t2) { }
	virtual ~SubtractTexture() { }
	
	virtual TextureType GetType() const { return SUBTRACT_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	virtual float Y() const {
		return tex1->Y() - tex2->Y();
	}
	virtual float Filter() const {
		return tex1->Filter() - tex2->Filter();
	}
	
	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);
		
		tex1->AddReferencedTextures(referencedTexs);
		tex2->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex1->AddReferencedImageMaps(referencedImgMaps);
		tex2->AddReferencedImageMaps(referencedImgMaps);
	}
	
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex1 == oldTex)
			tex1 = newTex;
		if (tex2 == oldTex)
			tex2 = newTex;
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
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

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
	virtual float Y() const { return .5f; }
	virtual float Filter() const { return .5f; }

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
	virtual float Y() const {
		return luxrays::Spectrum(.5f, .5f, 0.f).Y();
	}
	virtual float Filter() const {
		return luxrays::Spectrum(.5f, .5f, 0.f).Filter();
	}

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
	virtual float Y() const {
		float ret = offsets[0] * values[0].Y();
		for (u_int i = 0; i < offsets.size() - 1; ++i)
			ret += .5f * (offsets[i + 1] - offsets[i]) *
				(values[i + 1].Y() + values[i].Y());
		return ret;
	}
	virtual float Filter() const {
		float ret = offsets[0] * values[0].Filter();
		for (u_int i = 0; i < offsets.size() - 1; ++i)
			ret += .5f * (offsets[i + 1] - offsets[i]) *
				(values[i + 1].Filter() + values[i].Filter());
		return ret;
	}

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		amount->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		amount->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (amount == oldTex)
			amount = newTex;
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
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return 1.f; }
	virtual float Filter() const { return 1.f; }

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
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return 1.f; }
	virtual float Filter() const { return 1.f; }

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
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return 1.f; }
	virtual float Filter() const { return 1.f; }

	u_int GetChannel() const { return channel; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	u_int channel;
};

//------------------------------------------------------------------------------
// NormalMap textures
//------------------------------------------------------------------------------

class NormalMapTexture : public Texture {
public:
	NormalMapTexture(const Texture *t) : tex(t) { }
	virtual ~NormalMapTexture() { }

	virtual TextureType GetType() const { return NORMALMAP_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const { return 0.f; }
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const { return luxrays::Spectrum(); }
	virtual float Y() const { return 0.f; }
	virtual float Filter() const { return 0.f; }

    virtual luxrays::UV GetDuv(const HitPoint &hitPoint,
        const luxrays::Vector &dpdu, const luxrays::Vector &dpdv,
        const luxrays::Normal &dndu, const luxrays::Normal &dndv,
        const float sampleDistance) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex == oldTex)
			tex = newTex;
	}

	const Texture *GetTexture() const { return tex; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *tex;
};

}

#endif	/* _SLG_TEXTURE_H */
