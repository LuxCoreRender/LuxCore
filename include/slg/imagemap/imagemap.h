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

#ifndef _SLG_IMAGEMAP_H
#define	_SLG_IMAGEMAP_H

#include <OpenEXR/half.h>
#include <OpenImageIO/imageio.h>
OIIO_NAMESPACE_USING

#include <string>
#include <limits>

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/geometry/uv.h"

namespace slg {

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

}

#endif	/* _SLG_IMAGEMAP_H */
