/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/utils/strutils.h"
#include "luxrays/core/randomgen.h"
#include "slg/slg.h"
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/typedesc.h>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <memory>
#include <boost/format.hpp>
#include <filesystem>

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;

#include <Imath/half.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/dassert.h>
#include <OpenImageIO/half.h>

#include "luxrays/utils/properties.h"
#include "slg/core/sdl.h"
#include "slg/imagemap/imagemap.h"
#include "slg/imagemap/imagemapcache.h"
#include "slg/utils/filenameresolver.h"
#include "slg/usings.h"

using namespace std;
using namespace luxrays;
using namespace slg;
OIIO_NAMESPACE_USING

//------------------------------------------------------------------------------
// ImageMapPixel
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// u_char x 1 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<u_char, 1> *ImageMapPixel<u_char, 1>::GetWhite() {
	static const ImageMapPixel<u_char, 1> white(255);
	return &white;
}

template<> inline const ImageMapPixel<u_char, 1> *ImageMapPixel<u_char, 1>::GetBlack() {
	static const ImageMapPixel<u_char, 1> black;
	return &black;
}

//------------------------------------------------------------------------------
// u_char x 2 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<u_char, 2> *ImageMapPixel<u_char, 2>::GetWhite() {
	static const ImageMapPixel<u_char, 2> white(255);
	return &white;
}

template<> inline const ImageMapPixel<u_char, 2> *ImageMapPixel<u_char, 2>::GetBlack() {
	static const ImageMapPixel<u_char, 2> black;
	return &black;
}

//------------------------------------------------------------------------------
// u_char x 3 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<u_char, 3> *ImageMapPixel<u_char, 3>::GetWhite() {
	static const ImageMapPixel<u_char, 3> white(255);
	return &white;
}

template<> inline const ImageMapPixel<u_char, 3> *ImageMapPixel<u_char, 3>::GetBlack() {
	static const ImageMapPixel<u_char, 3> black;
	return &black;
}

//------------------------------------------------------------------------------
// u_char x 4 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<u_char, 4> *ImageMapPixel<u_char, 4>::GetWhite() {
	static const ImageMapPixel<u_char, 4> white(255);
	return &white;
}

template<> inline const ImageMapPixel<u_char, 4> *ImageMapPixel<u_char, 4>::GetBlack() {
	static const ImageMapPixel<u_char, 4> black;
	return &black;
}

//------------------------------------------------------------------------------
// half x 1 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<half, 1> *ImageMapPixel<half, 1>::GetWhite() {
	static const ImageMapPixel<half, 1> white(1.f);
	return &white;
}

template<> inline const ImageMapPixel<half, 1> *ImageMapPixel<half, 1>::GetBlack() {
	static const ImageMapPixel<half, 1> black;
	return &black;
}

//------------------------------------------------------------------------------
// half x 2 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<half, 2> *ImageMapPixel<half, 2>::GetWhite() {
	static const ImageMapPixel<half, 2> white(1.f);
	return &white;
}

template<> inline const ImageMapPixel<half, 2> *ImageMapPixel<half, 2>::GetBlack() {
	static const ImageMapPixel<half, 2> black;
	return &black;
}

//------------------------------------------------------------------------------
// half x 3 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<half, 3> *ImageMapPixel<half, 3>::GetWhite() {
	static const ImageMapPixel<half, 3> white(1.f);
	return &white;
}

template<> inline const ImageMapPixel<half, 3> *ImageMapPixel<half, 3>::GetBlack() {
	static const ImageMapPixel<half, 3> black;
	return &black;
}

//------------------------------------------------------------------------------
// half x 4 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<half, 4> *ImageMapPixel<half, 4>::GetWhite() {
	static const ImageMapPixel<half, 4> white(1.f);
	return &white;
}

template<> inline const ImageMapPixel<half, 4> *ImageMapPixel<half, 4>::GetBlack() {
	static const ImageMapPixel<half, 4> black;
	return &black;
}

//------------------------------------------------------------------------------
// float x 1 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<float, 1> *ImageMapPixel<float, 1>::GetWhite() {
	static const ImageMapPixel<float, 1> white(1.f);
	return &white;
}

template<> inline const ImageMapPixel<float, 1> *ImageMapPixel<float, 1>::GetBlack() {
	static const ImageMapPixel<float, 1> black;
	return &black;
}

//------------------------------------------------------------------------------
// float x 2 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<float, 2> *ImageMapPixel<float, 2>::GetWhite() {
	static const ImageMapPixel<float, 2> white(1.f);
	return &white;
}

template<> inline const ImageMapPixel<float, 2> *ImageMapPixel<float, 2>::GetBlack() {
	static const ImageMapPixel<float, 2> black;
	return &black;
}

//------------------------------------------------------------------------------
// float x 3 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<float, 3> *ImageMapPixel<float, 3>::GetWhite() {
	static const ImageMapPixel<float, 3> white(1.f);
	return &white;
}

template<> inline const ImageMapPixel<float, 3> *ImageMapPixel<float, 3>::GetBlack() {
	static const ImageMapPixel<float, 3> black;
	return &black;
}

//------------------------------------------------------------------------------
// float x 4 specialization
//------------------------------------------------------------------------------

template<> inline const ImageMapPixel<float, 4> *ImageMapPixel<float, 4>::GetWhite() {
	static const ImageMapPixel<float, 4> white(1.f);
	return &white;
}

template<> inline const ImageMapPixel<float, 4> *ImageMapPixel<float, 4>::GetBlack() {
	static const ImageMapPixel<float, 4> black;
	return &black;
}

//------------------------------------------------------------------------------
// ImageMapStorage
//------------------------------------------------------------------------------

ImageMapStorage::ImageMapStorage(
	const u_int w, const u_int h, const WrapType wm, const FilterType ft
) :
	width(w),
	height(h),
	wrapType(wm),
	filterType(ft)
{}

u_int ImageMapStorage::GetIndex(u_int x, u_int y) const { return x + y * GetWidth() ; }

ImageMapStorage::StorageType ImageMapStorage::String2StorageType(const string &type) {
	if (type == "auto")
		return ImageMapStorage::AUTO;
	else if (type == "byte")
		return ImageMapStorage::BYTE;
	else if (type == "half")
		return ImageMapStorage::HALF;
	else if (type == "float")
		return ImageMapStorage::FLOAT;
	else
		throw runtime_error("Unknown storage type: " + type);
}

template<typename T>
OIIO::image_span<T> ImageMapStorage::GetPixelsSpan(u_int channelCount) {

	switch(channelCount) {
		case 1: {
			auto downcast = dynamic_cast<ImageMapStorageImpl<T, 1> *>(this);
			assert(downcast);
			auto span = downcast->GetPixelsSpan();
			return span;
		}
		case 2: {
			auto downcast = dynamic_cast<ImageMapStorageImpl<T, 2> *>(this);
			assert(downcast);
			auto span = downcast->GetPixelsSpan();
			return span;
		}
		case 3: {
			auto downcast = dynamic_cast<ImageMapStorageImpl<T, 3> *>(this);
			assert(downcast);
			auto span = downcast->GetPixelsSpan();
			return span;
		}
		case 4: {
			auto downcast = dynamic_cast<ImageMapStorageImpl<T, 4> *>(this);
			assert(downcast);
			auto span = downcast->GetPixelsSpan();
			return span;
		}
		default: throw std::runtime_error("GetPixelsSpan: unhandlded channel count");
	}
}

string ImageMapStorage::StorageType2String(const StorageType type) {
	switch (type) {
		case ImageMapStorage::BYTE:
			return "byte";
		case ImageMapStorage::HALF:
			return "half";
		case ImageMapStorage::FLOAT:
			return "float";
		default:
			throw runtime_error("Unsupported storage type in ImageMapStorage::StorageType2String(): " + ToString(type));
	}
}

ImageMapStorage::WrapType ImageMapStorage::String2WrapType(const string &type) {
	if (type == "repeat")
		return ImageMapStorage::REPEAT;
	else if (type == "black")
		return ImageMapStorage::BLACK;
	else if (type == "white")
		return ImageMapStorage::WHITE;
	else if (type == "clamp")
		return ImageMapStorage::CLAMP;
	else
		throw runtime_error("Unknown wrap type: " + type);
}

string ImageMapStorage::WrapType2String(const WrapType type) {
	switch (type) {
		case ImageMapStorage::REPEAT:
			return "repeat";
		case ImageMapStorage::BLACK:
			return "black";
		case ImageMapStorage::WHITE:
			return "white";
		case ImageMapStorage::CLAMP:
			return "clamp";
		default:
			throw runtime_error("Unsupported wrap type in ImageMapStorage::WrapType2String(): " + ToString(type));
	}
}

ImageMapStorage::FilterType ImageMapStorage::String2FilterType(const string &type) {
	if (type == "nearest")
		return ImageMapStorage::NEAREST;
	else if (type == "linear")
		return ImageMapStorage::LINEAR;
	else
		throw runtime_error("Unknown filter type: " + type);
}

string ImageMapStorage::FilterType2String(const FilterType type) {
	switch (type) {
		case ImageMapStorage::NEAREST:
			return "nearest";
		case ImageMapStorage::LINEAR:
			return "linear";
		default:
			throw runtime_error("Unsupported wrap type in ImageMapStorage::FilterType2String(): " + ToString(type));
	}
}

ImageMapStorage::ChannelSelectionType ImageMapStorage::String2ChannelSelectionType(
		const string &type) {
	if (type == "default")
		return ImageMapStorage::DEFAULT;
	else if (type == "red")
		return ImageMapStorage::RED;
	else if (type == "green")
		return ImageMapStorage::GREEN;
	else if (type == "blue")
		return ImageMapStorage::BLUE;
	else if (type == "alpha")
		return ImageMapStorage::ALPHA;
	else if (type == "mean")
		return ImageMapStorage::MEAN;
	else if (type == "colored_mean")
		return ImageMapStorage::WEIGHTED_MEAN;
	else if (type == "rgb")
		return ImageMapStorage::RGB;
	else if (type == "directx2opengl_normalmap")
		return ImageMapStorage::DIRECTX2OPENGL_NORMALMAP;
	else
		throw runtime_error("Unknown channel selection type in imagemap: " + type);
}

//------------------------------------------------------------------------------
// ImageMapStorageImpl
//------------------------------------------------------------------------------
template <class T, u_int CHANNELS>
OIIO::image_span<T> ImageMapStorageImpl<T, CHANNELS>::GetPixelsSpan() {
	T* ptr = &pixels[0][0];
	return OIIO::image_span<T, 3>(ptr, CHANNELS, width, height);
}

template <class T, u_int CHANNELS>
void ImageMapStorageImpl<T, CHANNELS>::SetFloat(const u_int index, const float v) {
	pixels[index].SetFloat(v);
}

template <class T, u_int CHANNELS>
void ImageMapStorageImpl<T, CHANNELS>::SetSpectrum(const u_int index, const Spectrum &v) {
	pixels[index].SetSpectrum(v);
}

template <class T, u_int CHANNELS>
void ImageMapStorageImpl<T, CHANNELS>::SetAlpha(const u_int index, const float v) {
	pixels[index].SetAlpha(v);
}

template <class T, u_int CHANNELS>
float ImageMapStorageImpl<T, CHANNELS>::GetFloat(const UV &uv) const {
	switch (filterType) {
		case NEAREST: {
			const float s = uv.u * width;
			const float t = uv.v * height;

			const int s0 = Floor2Int(s);
			const int t0 = Floor2Int(t);

			return GetTexel(s0, t0)->GetFloat();
		}
		case LINEAR: {
			const float s = uv.u * width - .5f;
			const float t = uv.v * height - .5f;

			const int s0 = Floor2Int(s);
			const int t0 = Floor2Int(t);

			const float ds = s - s0;
			const float dt = t - t0;

			const float ids = 1.f - ds;
			const float idt = 1.f - dt;

			return ids * idt * GetTexel(s0, t0)->GetFloat() +
					ids * dt * GetTexel(s0, t0 + 1)->GetFloat() +
					ds * idt * GetTexel(s0 + 1, t0)->GetFloat() +
					ds * dt * GetTexel(s0 + 1, t0 + 1)->GetFloat();
		}
		default:
			throw runtime_error("Unknown filter mode in ImageMapStorageImpl<T, CHANNELS>::GetFloat(): " + ToString(filterType));
	}
}

template <class T, u_int CHANNELS>
float ImageMapStorageImpl<T, CHANNELS>::GetFloat(const u_int index) const {
	assert (index >= 0);
	assert (index < width * height);

	return pixels[index].GetFloat();
}

template <class T, u_int CHANNELS>
Spectrum ImageMapStorageImpl<T, CHANNELS>::GetSpectrum(const UV &uv) const {
	switch (filterType) {
		case NEAREST: {
			const float s = uv.u * width;
			const float t = uv.v * height;

			const int s0 = Floor2Int(s);
			const int t0 = Floor2Int(t);

			return GetTexel(s0, t0)->GetSpectrum();
		}
		case LINEAR: {
			const float s = uv.u * width - .5f;
			const float t = uv.v * height - .5f;

			const int s0 = Floor2Int(s);
			const int t0 = Floor2Int(t);

			const float ds = s - s0;
			const float dt = t - t0;

			const float ids = 1.f - ds;
			const float idt = 1.f - dt;

			return ids * idt * GetTexel(s0, t0)->GetSpectrum() +
					ids * dt * GetTexel(s0, t0 + 1)->GetSpectrum() +
					ds * idt * GetTexel(s0 + 1, t0)->GetSpectrum() +
					ds * dt * GetTexel(s0 + 1, t0 + 1)->GetSpectrum();
		}
		default:
			throw runtime_error("Unknown filter mode in ImageMapStorageImpl<T, CHANNELS>::GetSpectrum(): " + ToString(filterType));
	}
}

template <class T, u_int CHANNELS>
Spectrum ImageMapStorageImpl<T, CHANNELS>::GetSpectrum(const u_int index) const {
	assert (index >= 0);
	assert (index < width * height);

	return pixels[index].GetSpectrum();
}

template <class T, u_int CHANNELS>
float ImageMapStorageImpl<T, CHANNELS>::GetAlpha(const UV &uv) const {
	const float s = uv.u * width - .5f;
	const float t = uv.v * height - .5f;

	const int s0 = Floor2Int(s);
	const int t0 = Floor2Int(t);

	const float ds = s - s0;
	const float dt = t - t0;

	const float ids = 1.f - ds;
	const float idt = 1.f - dt;

	return ids * idt * GetTexel(s0, t0)->GetAlpha() +
			ids * dt * GetTexel(s0, t0 + 1)->GetAlpha() +
			ds * idt * GetTexel(s0 + 1, t0)->GetAlpha() +
			ds * dt * GetTexel(s0 + 1, t0 + 1)->GetAlpha();
}

template <class T, u_int CHANNELS>
float ImageMapStorageImpl<T, CHANNELS>::GetAlpha(const u_int index) const {
	assert (index >= 0);
	assert (index < width * height);

	return pixels[index].GetAlpha();
}

template <class T, u_int CHANNELS>
UV ImageMapStorageImpl<T, CHANNELS>::GetDuv(const UV &uv) const {
	const float s = uv.u * width;
	const float t = uv.v * height;

	const int is = Floor2Int(s);
	const int it = Floor2Int(t);

	const float as = s - is;
	const float at = t - it;

	int s0, s1;
	if (as < .5f) {
		s0 = is - 1;
		s1 = is;
	} else {
		s0 = is;
		s1 = is + 1;
	}
	int t0, t1;
	if (at < .5f) {
		t0 = it - 1;
		t1 = it;
	} else {
		t0 = it;
		t1 = it + 1;
	}

	UV duv;
	duv.u = Lerp(at, GetTexel(s1, it)->GetFloat() - GetTexel(s0, it)->GetFloat(),
		GetTexel(s1, it + 1)->GetFloat() - GetTexel(s0, it + 1)->GetFloat()) *
		width;
	duv.v = Lerp(as, GetTexel(is, t1)->GetFloat() - GetTexel(is, t0)->GetFloat(),
		GetTexel(is + 1, t1)->GetFloat() - GetTexel(is + 1, t0)->GetFloat()) *
		height;
	return duv;
}

template <class T, u_int CHANNELS>
UV ImageMapStorageImpl<T, CHANNELS>::GetDuv(const u_int index) const {
	UV uv((index % width) + .5f, (index / height) + .5f);
	return GetDuv(uv);
}

template <class T, u_int CHANNELS>
const ImageMapPixel<T, CHANNELS> *ImageMapStorageImpl<T, CHANNELS>::GetTexel(const int s, const int t) const {
	u_int u, v;
	switch (wrapType) {
		case REPEAT:
			u = static_cast<u_int>(Mod<int>(s, width));
			v = static_cast<u_int>(Mod<int>(t, height));
			break;
		case BLACK:
			if ((s < 0) || (s >= static_cast<int>(width)) || (t < 0) || (t >= static_cast<int>(height)))
				return ImageMapPixel<T, CHANNELS>::GetBlack();
			u = static_cast<u_int>(s);
			v = static_cast<u_int>(t);
			break;
		case WHITE:
			if ((s < 0) || (s >= static_cast<int>(width)) || (t < 0) || (t >= static_cast<int>(height)))
				return ImageMapPixel<T, CHANNELS>::GetWhite();
			u = static_cast<u_int>(s);
			v = static_cast<u_int>(t);
			break;
		case CLAMP:
			u = static_cast<u_int>(Clamp<int>(s, 0, width - 1));
			v = static_cast<u_int>(Clamp<int>(t, 0, height - 1));
			break;
		default:
			throw runtime_error("Unknown wrap mode in ImageMapStorageImpl::GetTexel(): " + ToString(wrapType));
	}

	const u_int index = v * width + u;
	assert (index >= 0);
	assert (index < width * height);

	return &pixels[index];
}

template <class T, u_int CHANNELS>
void ImageMapStorageImpl<T, CHANNELS>::ReverseGammaCorrection(const float gamma) {
	if (gamma != 1.f) {
		#pragma omp parallel for
		for (
				// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
				unsigned
#endif
				int i = 0; i < width * height; i++)
			pixels[i].ReverseGammaCorrection(gamma);
	}
}

template <class T, u_int CHANNELS>
OIIO::image_span<std::byte> ImageMapStorageImpl<T, CHANNELS>::ToSpan() {
	auto span = OIIO::image_span<T>(
		static_cast<T*>(GetPixelsData()),
		CHANNELS,
		width,
		height
	);
	return span.as_writable_bytes_image_span();
}

template <class T, u_int CHANNELS>
OIIO::image_span<const std::byte> ImageMapStorageImpl<T, CHANNELS>::ToSpan() const {
	auto span = OIIO::image_span<const T>(
		static_cast<const T*>(GetPixelsData()),
		CHANNELS,
		width,
		height
	);
	return span.as_bytes_image_span();
}

template <class T, u_int CHANNELS>
ImageMapStorageUPtr ImageMapStorageImpl<T, CHANNELS>::Copy() const {
	const u_int pixelCount = width * height;
	std::vector<ImageMapPixel<T, CHANNELS>> newPixels(pixelCount);

	std::copy(pixels.begin(), pixels.end(), newPixels.begin());

	return std::make_unique<ImageMapStorageImpl<T, CHANNELS>>(
		width, height, wrapType, filterType, std::move(newPixels)
	);
}

template <class T, u_int CHANNELS>
ImageMapStorageUPtr ImageMapStorageImpl<T, CHANNELS>::SelectChannel(
	const ChannelSelectionType selectionType
) const {

	const u_int pixelCount = width * height;

	auto createIMS = [&](u_int channel) {
		std::vector<ImageMapPixel<T, 1>> newPixels;
		newPixels.reserve(pixelCount);
		for (auto& p : pixels) {
			newPixels.emplace_back(p[channel]);
		}
		return std::make_unique<ImageMapStorageImpl<T, 1>>(
			width, height, wrapType, filterType, std::move(newPixels)
		);
	};

	// Convert the image if required
	switch (selectionType) {
		case ImageMapStorage::DEFAULT:
			// Nothing to do
			return nullptr;
		case ImageMapStorage::RED:
		case ImageMapStorage::GREEN:
		case ImageMapStorage::BLUE:
		case ImageMapStorage::ALPHA: {
			if (CHANNELS == 1) {
				// Nothing to do
				return nullptr;
			}

			if (CHANNELS == 2) {
				const u_int channel = (
					(selectionType == ImageMapStorage::RED) ||
					(selectionType == ImageMapStorage::GREEN) ||
					(selectionType == ImageMapStorage::BLUE)) ? 0 : 1;
				return createIMS(channel);
			}

			// CHANNEL >= 3
			const u_int channel = selectionType - ImageMapStorage::RED;
			return createIMS(channel);
		}
		case ImageMapStorage::MEAN:
		case ImageMapStorage::WEIGHTED_MEAN: {
			if (CHANNELS == 1) {
				// Nothing to do
				return nullptr;
			}
			if (CHANNELS == 2) {
				const u_int channel = 0;
				return createIMS(channel);
			}

			// CHANNELS >= 3
			std::vector<ImageMapPixel<T, 1>> newPixels;
			newPixels.reserve(pixelCount);

			if (selectionType == ImageMapStorage::MEAN) {
				for (auto& p: pixels) {
					auto& newPix = newPixels.emplace_back();
					newPix.SetFloat(p.GetSpectrum().Filter());
				}
			} else {
				for (auto& p: pixels) {
					auto& newPix = newPixels.emplace_back();
					newPix.SetFloat(p.GetSpectrum().Y());
				}
			}

			return std::make_unique<ImageMapStorageImpl<T, 1>>(
				width, height, wrapType, filterType, std::move(newPixels)
			);
		}
		case ImageMapStorage::RGB: {
			if ((CHANNELS == 1) || (CHANNELS == 2) || (CHANNELS == 3)) {
				// Nothing to do
				return nullptr;
			}

			std::vector<ImageMapPixel<T, 3>> newPixels;
			newPixels.reserve(pixelCount);
			for (auto& p: pixels) {
				newPixels.emplace_back(
					std::initializer_list<T>{p[0], p[1], p[2]}
				);
			}
			return std::make_unique<ImageMapStorageImpl<T, 3>>(
				width, height, wrapType, filterType, std::move(newPixels)
			);
		}
		case ImageMapStorage::DIRECTX2OPENGL_NORMALMAP: {
			if ((CHANNELS == 1) || (CHANNELS == 2)) {
				// Nothing to do
				return nullptr;
			}
			std::vector<ImageMapPixel<T, 3>> newPixels;

			newPixels.reserve(pixelCount);

			for(auto& p: pixels) {
				Spectrum c = p.GetSpectrum();
				// Invert G channel
				c.c[1] = 1.f - c.c[1];
				auto& newPix = newPixels.emplace_back();
				newPix.SetSpectrum(c);
			}

			return std::make_unique<ImageMapStorageImpl<T, 3>>(
				width, height, wrapType, filterType, std::move(newPixels)
			);
		}
		default:
			throw runtime_error(
				"Unknown channel selection type in an ImageMap: "
				+ ToString(selectionType)
			);
	}
}

//------------------------------------------------------------------------------
// ImageMapConfig
//------------------------------------------------------------------------------

ImageMapConfig::ImageMapConfig() : colorSpaceCfg(1.f) {
	storageType = ImageMapStorage::StorageType::FLOAT;
	wrapType = ImageMapStorage::WrapType::REPEAT;
	selectionType = ImageMapStorage::ChannelSelectionType::DEFAULT;
	filterType = ImageMapStorage::LINEAR;
}

ImageMapConfig::ImageMapConfig(const float gamma,
		const ImageMapStorage::StorageType store,
		const ImageMapStorage::WrapType wrap,
		const ImageMapStorage::ChannelSelectionType selection,
		const ImageMapStorage::FilterType filter) : colorSpaceCfg(gamma) {
	storageType = store;
	wrapType = wrap;
	selectionType = selection;
	filterType = filter;
}

ImageMapConfig::ImageMapConfig(const string &configName, const string &colorSpaceName,
		const ImageMapStorage::StorageType store,
		const ImageMapStorage::WrapType wrap,
		const ImageMapStorage::ChannelSelectionType selection,
		const ImageMapStorage::FilterType filter) : colorSpaceCfg(configName, colorSpaceName) {
	storageType = store;
	wrapType = wrap;
	selectionType = selection;
	filterType = filter;
}

ImageMapConfig::ImageMapConfig(const Properties &props, const string &prefix) {
	FromProperties(props, prefix, *this);
}

void ImageMapConfig::FromProperties(const Properties &props, const string &prefix, ImageMapConfig &imgCfg) {
	ColorSpaceConfig::FromProperties(props, prefix, imgCfg.colorSpaceCfg, ColorSpaceConfig::defaultLuxCoreConfig);

	imgCfg.SetStorageType(
		ImageMapStorage::String2StorageType(
		props.Get(Property(prefix + ".storage")("auto")).Get<string>())
	);
	imgCfg.SetWrapType(
		ImageMapStorage::String2WrapType(
			props.Get(Property(prefix + ".wrap")("repeat")).Get<string>())
	);
	imgCfg.SetFilterType(
		ImageMapStorage::String2FilterType(
			props.Get(Property(prefix + ".filter")("linear")).Get<string>())
	);
	imgCfg.SetSelectionType(
		ImageMapStorage::String2ChannelSelectionType(
			props.Get(Property(prefix + ".channel")("default")).Get<string>())
	);
}

//------------------------------------------------------------------------------
// ImageMap
//------------------------------------------------------------------------------

ImageMap::ImageMap() {
	pixelStorage = nullptr;
	instrumentationInfo = nullptr;
}

ImageMap::ImageMap(
	const string &fileName,
	const ImageMapConfig &cfg,
	const u_int widthHint,
	const u_int heightHint
) :
	NamedObject(fileName),
	imageMapConfig(cfg),
	instrumentationInfo(
		std::make_unique<InstrumentationInfo>(widthHint, heightHint, cfg)
	)
{
	Init(fileName, cfg, widthHint, heightHint);
}

ImageMap::ImageMap(
	ImageMapStorageUPtr&& pixels,
	const float im,
	const float imy,
	const ImageMapConfig &cfg
) :
	pixelStorage(std::move(pixels)),
	imageMean(im),
	imageMeanY(imy),
	imageMapConfig(cfg),
	instrumentationInfo(
		std::make_unique<InstrumentationInfo>(
			pixelStorage->GetWidth(), pixelStorage->GetHeight(), cfg
		)
	)
{}

ImageMap::~ImageMap() {
}

void ImageMap::Reload() {
	if (!instrumentationInfo)
		throw runtime_error("ImageMap::Reload() called on a not instrumented image map: " + GetName());

	pixelStorage.reset();
	Init(GetName(), instrumentationInfo->originalImgCfg, 0, 0);
}

void ImageMap::Reload(
	const string &fileName, const u_int widthHint, const u_int heightHint
) {
	if (!instrumentationInfo)
		throw runtime_error("ImageMap::Reload() called on a not instrumented image map: " + GetName() + " from " + fileName);

	pixelStorage.reset();
	Init(fileName, instrumentationInfo->originalImgCfg, widthHint, heightHint);
}


template<typename T>
static auto createBuffer(u_int channelCount, u_int width, u_int height) {
	switch(channelCount) {
		case 1: return std::vector<ImageMapPixel<T, 1>>(width * height);
		case 2: return std::vector<ImageMapPixel<T, 2>>(width * height);
		case 3: return std::vector<ImageMapPixel<T, 3>>(width * height);
		case 4: return std::vector<ImageMapPixel<T, 4>>(width * height);
		default: throw std::runtime_error(
			"createBuffer: unhandled channelCount (" + ToString(channelCount) + ")"
		);
	}
}

void ImageMap::Init(
	const string &fileName,
	const ImageMapConfig &cfg,
	const u_int widthHint,
	const u_int heightHint
) {
	const string resolvedFileName = SLG_FileNameResolver.ResolveFile(fileName);
#ifndef NDEBUG
	SDL_LOG("OIIO version: " << OIIO::openimageio_version());
#endif
	SDL_LOG("Reading texture map: " << resolvedFileName);

	if (!std::filesystem::exists(resolvedFileName))
		throw runtime_error(
			"ImageMap file doesn't exist: " + resolvedFileName + "(" + GetName() + ")"
		);


	OIIO::attribute("openexr:core", 1);  // Nota: crash if not set
	ImageSpec config;
	config["oiio:UnassociatedAlpha"] = 1;
	auto in = ImageInput::open(resolvedFileName, &config);

	if (!in)
		throw runtime_error(
			"Error opening image file: " + resolvedFileName +
			" (error = " + geterror() +")"
		);

	// Check the mipmap level available
	int mipmapLevel = 0;
	std::stringstream ss;
	std::vector<pair<u_int, u_int> > mipmapSizes;
	while (in->seek_subimage(0, mipmapLevel)) {
		const ImageSpec &spec = in->spec();

		mipmapSizes.push_back(make_pair(spec.width, spec.height));
		ss << "[" << spec.width << "x" << spec.height << "]";

		++mipmapLevel;
	}
	SDL_LOG("Mip map available: " << ss.str());

	// Select the best mipmap
	u_int bestMipmapIndex = 0;
	u_int bestMipmapWidth = mipmapSizes[0].first;
	u_int bestMipmapHeight = mipmapSizes[0].second;

	if ((widthHint > 0) || (heightHint > 0)) {
		// Only if I have size hints
		for (u_int i = 1 ; i < mipmapSizes.size(); ++i) {
			if ((mipmapSizes[i].first >= widthHint) &&
					(mipmapSizes[i].second >= heightHint) &&
					(mipmapSizes[i].first < bestMipmapWidth) &&
					(mipmapSizes[i].second < bestMipmapHeight)) {
				bestMipmapIndex = i;
				bestMipmapWidth = mipmapSizes[i].first;
				bestMipmapHeight = mipmapSizes[i].second;
			}
		}
	}

	SDL_LOG("Reading mip map level: " << bestMipmapIndex);
	if (!in->seek_subimage(0, bestMipmapIndex))
		throw runtime_error(
			"Unable to read mip map level: " + ToString(bestMipmapIndex)
		);

	const ImageSpec &spec = in->spec();
	u_int width = spec.width;
	u_int height = spec.height;
	u_int channelCount = spec.nchannels;

	if ((channelCount != 1) && (channelCount != 2) &&
			(channelCount != 3) && (channelCount != 4))
		throw runtime_error(
			"Unsupported number of channels in an ImageMap: " + ToString(channelCount)
		);

	// Anything not TypeDesc::UCHAR or TypeDesc::HALF, is stored in float format

	ImageMapStorage::StorageType selectedStorageType = cfg.GetStorageType();
	if (selectedStorageType == ImageMapStorage::AUTO) {
		// Automatically select the storage type

		if (spec.format == TypeDesc::UCHAR)
			selectedStorageType = ImageMapStorage::BYTE;
		else if (spec.format == TypeDesc::HALF)
			selectedStorageType = ImageMapStorage::HALF;
		else
			selectedStorageType = ImageMapStorage::FLOAT;
	}

	// Allocate storage
	TypeDesc td;
	std::string tdstr;
	switch (selectedStorageType) {
		case ImageMapStorage::BYTE:
			pixelStorage = AllocImageMapStorage<u_char>(
				channelCount, width, height, cfg.GetWrapType(), cfg.GetFilterType()
			);
			td = TypeDesc::UCHAR;
			tdstr = "UCHAR";
			break;
		case ImageMapStorage::HALF:
			pixelStorage = AllocImageMapStorage<half>(
				channelCount, width, height, cfg.GetWrapType(), cfg.GetFilterType()
			);
			td = TypeDesc::HALF;
			tdstr = "HALF";
			break;
		case ImageMapStorage::FLOAT:
			pixelStorage = AllocImageMapStorage<float>(
				channelCount, width, height, cfg.GetWrapType(), cfg.GetFilterType()
			);
			td = TypeDesc::FLOAT;
			tdstr = "FLOAT";
			break;
		default: throw runtime_error(
			"Unsupported selected storage type in an ImageMap: "
			+ ToString(selectedStorageType)
		);
	}

	// Read image
	bool res = in->read_image(
		0, bestMipmapIndex, 0, channelCount, td, pixelStorage->GetPixelsData()
	);
	if (not res) {
		auto error = in->geterror();
		SDL_LOG("Error reading image map: " << error);
	}
	in->close();

	switch (cfg.colorSpaceCfg.colorSpaceType) {
		case ColorSpaceConfig::NOP_COLORSPACE:
			// Nothing to do
			break;
		case ColorSpaceConfig::LUXCORE_COLORSPACE:
			pixelStorage->ReverseGammaCorrection(cfg.colorSpaceCfg.luxcore.gamma);
			break;
		case ColorSpaceConfig::OPENCOLORIO_COLORSPACE:
			ConvertColorSpace(cfg.colorSpaceCfg.ocio.configName,
					cfg.colorSpaceCfg.ocio.colorSpaceName,
					OCIO::ROLE_SCENE_LINEAR);
			break;
		default:
			throw runtime_error("Unknown color space in ImageMap::ImageMap(" +
					fileName + "): " + ToString(cfg.colorSpaceCfg.colorSpaceType));
	}
	auto span = pixelStorage->ToSpan();
	SDL_LOG("Image dimensions: "
		<< span.width() << "x" << span.height()
		<< "@" << span.nchannels()
		<< " (type=" << tdstr << ", "
		<< "contiguous="
		<< std::string(span.is_contiguous() ? std::string("yes") : std::string("no"))
		<< ")"
	);

	SelectChannel(cfg.GetSelectionType());
	Preprocess();
}

float ImageMap::GetFloat(const UV &uv) const {
	if (instrumentationInfo && instrumentationInfo->enabled)
		instrumentationInfo->ThreadAddSample(uv);

	return pixelStorage->GetFloat(uv);
}

Spectrum ImageMap::GetSpectrum(const UV &uv) const {
	if (instrumentationInfo && instrumentationInfo->enabled)
		instrumentationInfo->ThreadAddSample(uv);

	return pixelStorage->GetSpectrum(uv);
}

float ImageMap::GetAlpha(const UV &uv) const {
	if (instrumentationInfo && instrumentationInfo->enabled)
		instrumentationInfo->ThreadAddSample(uv);

	return pixelStorage->GetAlpha(uv);
}

UV ImageMap::GetDuv(const UV &uv) const {
	if (instrumentationInfo && instrumentationInfo->enabled)
		instrumentationInfo->ThreadAddSample(uv);

	return pixelStorage->GetDuv(uv);
}

ImageMapUPtr ImageMap::AllocImageMap(
	const u_int channels,
	const u_int width,
	const u_int height,
	const ImageMapConfig &cfg
) {

	// This lambda avoids to deal with a predeclared UPtr (at low level, it avoids a
	// a release)
	auto createIMS = [&]() {
		switch (cfg.GetStorageType()) {
			case ImageMapStorage::BYTE:
				return AllocImageMapStorage<u_char>(
					channels, width, height,cfg.GetWrapType(), cfg.GetFilterType()
				);
				break;
			case ImageMapStorage::HALF:
				return AllocImageMapStorage<half>(
					channels, width, height, cfg.GetWrapType(), cfg.GetFilterType()
				);
				break;
			case ImageMapStorage::FLOAT:
				return AllocImageMapStorage<float>(
					channels, width, height, cfg.GetWrapType(), cfg.GetFilterType()
				);
				break;
			default:
				throw runtime_error(
					"Unknown storage type in ImageMap::AllocImageMap(): "
					+ ToString(cfg.GetStorageType())
				);
		}  // Switch
	};  // lambda

	return std::make_unique<ImageMap>(std::move(createIMS()), 0.f, 0.f, cfg);

}

ImageMapUPtr ImageMap::AllocImageMap(
	void *pixels, const u_int channels,
	const u_int width, const u_int height, const ImageMapConfig &cfg
) {

	// This lambda avoids to deal with a predeclared UPtr (at low level, it avoids a
	// a release)
	auto&& createIMS = [&]() {
		switch (cfg.GetStorageType()) {
			case ImageMapStorage::BYTE:
				return AllocImageMapStorage<u_char>(
					channels, width, height, cfg.GetWrapType(), cfg.GetFilterType()
				);
				break;
			case ImageMapStorage::HALF:
				return AllocImageMapStorage<half>(
					channels, width, height, cfg.GetWrapType(), cfg.GetFilterType()
				);
				break;
			case ImageMapStorage::FLOAT:
				return AllocImageMapStorage<float>(
					channels, width, height, cfg.GetWrapType(), cfg.GetFilterType()
				);
				break;
			default:
				throw runtime_error(
					"Unknown storage type in ImageMap::AllocImageMap(): "
					+ ToString(cfg.GetStorageType())
				);
		}  // Switch
	};  // lambda

	auto imageMap = std::make_unique<ImageMap>(std::move(createIMS()), 0.f , 0.f, cfg);

	memcpy(imageMap->GetStorage().GetPixelsData(), pixels, imageMap->GetStorage().GetMemorySize());

	switch (cfg.colorSpaceCfg.colorSpaceType) {
		case ColorSpaceConfig::NOP_COLORSPACE:
			// Nothing to do
			break;
		case ColorSpaceConfig::LUXCORE_COLORSPACE:
			imageMap->pixelStorage->ReverseGammaCorrection(cfg.colorSpaceCfg.luxcore.gamma);
			break;
		case ColorSpaceConfig::OPENCOLORIO_COLORSPACE:
			imageMap->ConvertColorSpace(cfg.colorSpaceCfg.ocio.configName,
					cfg.colorSpaceCfg.ocio.colorSpaceName,
					OCIO::ROLE_SCENE_LINEAR);
			break;
		default:
			throw runtime_error("Unknown color space in ImageMap::AllocImageMap(): " +
					ToString(cfg.colorSpaceCfg.colorSpaceType));
	}

	imageMap->SelectChannel(cfg.GetSelectionType());
	imageMap->Preprocess();

	return imageMap;
}

float ImageMap::CalcSpectrumMean() const {
	const u_int pixelCount = pixelStorage->GetWidth() * pixelStorage->GetHeight();

	float mean = 0.f;
	#pragma omp parallel for reduction(+:mean)
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < pixelCount; ++i) {
		const float m = pixelStorage->GetSpectrum(i).Filter();
		assert (!isnan(m) && !isinf(m));

		mean += m;
	}

	const float result = mean / (pixelStorage->GetWidth() * pixelStorage->GetHeight());
	assert (!isnan(result) && !isinf(result));

	return result;
}

float ImageMap::CalcSpectrumMeanY() const {
	const u_int pixelCount = pixelStorage->GetWidth() * pixelStorage->GetHeight();

	float mean = 0.f;
	#pragma omp parallel for reduction(+:mean)
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < pixelCount; ++i) {
		const float m = pixelStorage->GetSpectrum(i).Y();
		assert (!isnan(m) && !isinf(m));

		mean += m;
	}

	const float result = mean / (pixelStorage->GetWidth() * pixelStorage->GetHeight());
	assert (!isnan(result) && !isinf(result));

	return result;
}

void ImageMap::Preprocess() {
	imageMean = CalcSpectrumMean();
	imageMeanY = CalcSpectrumMeanY();
}

void ImageMap::SelectChannel(const ImageMapStorage::ChannelSelectionType selectionType) {
	ImageMapStorageUPtr newPixelStorage = pixelStorage->SelectChannel(selectionType);

	// Replace the old image map storage if required
	if (newPixelStorage) {
		pixelStorage = std::move(newPixelStorage);
	}
}

void ImageMap::ConvertStorage(const ImageMapStorage::StorageType newStorageType,
		const u_int newChannelCount) {
	const ImageMapStorage::StorageType storageType = pixelStorage->GetStorageType();
	const u_int channelCount = pixelStorage->GetChannelCount();
	if ((storageType == newStorageType) && (channelCount == newChannelCount))
		return;

	const u_int width = pixelStorage->GetWidth();
	const u_int height = pixelStorage->GetHeight();
	const ImageMapStorage::WrapType wrapType = pixelStorage->GetWrapType();
	const ImageMapStorage::FilterType filterType = pixelStorage->GetFilterType();

	// Allocate the new image map storage
	ImageMapStorageUPtr newPixelStorage;
	switch (newStorageType) {
		case ImageMapStorage::BYTE: {
			newPixelStorage = AllocImageMapStorage<u_char>(
				newChannelCount, width, height, wrapType, filterType
			);
			break;
		}
		case ImageMapStorage::HALF: {
			newPixelStorage = AllocImageMapStorage<half>(
				newChannelCount, width, height, wrapType, filterType
			);
			break;
		}
		case ImageMapStorage::FLOAT: {
			newPixelStorage = AllocImageMapStorage<float>(newChannelCount, width, height, wrapType, filterType);
			break;
		}
		default:
			throw runtime_error("Unsupported storage type in ImageMap::ConvertStorage(): " + ToString(newStorageType));
	}

	const u_int pixelCount = width * height;
	switch (channelCount) {
		case 1: {
			for (u_int i = 0; i < pixelCount; ++i)
				newPixelStorage->SetFloat(i, pixelStorage->GetFloat(i));
			break;
		}
		case 2: {
			for (u_int i = 0; i < pixelCount; ++i) {
				newPixelStorage->SetFloat(i, pixelStorage->GetFloat(i));
				newPixelStorage->SetAlpha(i, pixelStorage->GetAlpha(i));
			}
			break;
		}
		case 3: {
			for (u_int i = 0; i < pixelCount; ++i)
				newPixelStorage->SetSpectrum(i, pixelStorage->GetSpectrum(i));
			break;
		}
		case 4: {
			for (u_int i = 0; i < pixelCount; ++i) {
				newPixelStorage->SetSpectrum(i, pixelStorage->GetSpectrum(i));
				newPixelStorage->SetAlpha(i, pixelStorage->GetAlpha(i));
			}
			break;
		}
		default:
			throw runtime_error("Unsupported channel count inImageMap::ConvertStorage(): " + ToString(channelCount));
	}
	
	// I can delete the current image
	pixelStorage = std::move(newPixelStorage);
}

void ImageMap::ConvertColorSpace(const string &configFileName,
	const string &inputColorSpace, const string &outputColorSpace) {
	if (inputColorSpace == outputColorSpace)
		return;

	const ImageMapStorage::StorageType storageType = pixelStorage->GetStorageType();
	const u_int channelCount = pixelStorage->GetChannelCount();

	// Convert the image to RGB(A) float
	if (channelCount == 1)
		ConvertStorage(ImageMapStorage::FLOAT, 3);
	else if (channelCount == 2)
		ConvertStorage(ImageMapStorage::FLOAT, 4);
	else if (storageType != ImageMapStorage::FLOAT)
		ConvertStorage(ImageMapStorage::FLOAT, channelCount);

	// Convert the color space
	try {
		OCIO::ConstConfigRcPtr config = (configFileName == "") ?
			OCIO::GetCurrentConfig() :
			OCIO::Config::CreateFromFile(SLG_FileNameResolver.ResolveFile(configFileName).c_str());

		OCIO::ConstProcessorRcPtr processor = config->getProcessor(inputColorSpace.c_str(), outputColorSpace.c_str());

		OCIO::ConstCPUProcessorRcPtr cpu = processor->getDefaultCPUProcessor();

		// Apply the color transform with OpenColorIO
		OCIO::PackedImageDesc img(pixelStorage->GetPixelsData(),
				pixelStorage->GetWidth(), pixelStorage->GetHeight(),
				pixelStorage->GetChannelCount());
		cpu->apply(img);
	} catch (OCIO::Exception &exception) {
		throw runtime_error("OpenColorIO Error in OpenColorIOToneMap::Apply(): " + string(exception.what()));
	}

	// Convert back the image to the original storage type and channel count
	ConvertStorage(storageType, channelCount);
}

void ImageMap::Resize(const u_int newWidth, const u_int newHeight) {
	const u_int width = pixelStorage->GetWidth();
	const u_int height = pixelStorage->GetHeight();
	if ((width == newWidth) && (height == newHeight))
		return;

	ImageMapStorage::StorageType storageType = pixelStorage->GetStorageType();
	const u_int channelCount = pixelStorage->GetChannelCount();

	TypeDesc::BASETYPE baseType;
	switch (storageType) {
		case ImageMapStorage::BYTE:
			baseType = TypeDesc::UCHAR;
			break;
		case ImageMapStorage::HALF:
			baseType = TypeDesc::HALF;
			break;
		case ImageMapStorage::FLOAT:
			baseType = TypeDesc::FLOAT;
			break;
		default:
			throw runtime_error("Unsupported storage type in ImageMap::Resize(): " + ToString(storageType));
	}

	ImageSpec sourceSpec(width, height, channelCount, baseType);
	ImageBuf source(sourceSpec, pixelStorage->ToSpan());
	SLG_LOG("Resizing to " << width << "x" << height << " (" << channelCount << ")");

	ImageBufAlgo::KWArgs options = {};
	ROI roi(0, newWidth, 0, newHeight, 0, 1, 0, source.nchannels());
	ImageBuf dest = ImageBufAlgo::resize(source, options, roi);

	// Save the wrap mode
	const ImageMapStorage::WrapType wrapType = pixelStorage->GetWrapType();
	// Save the filter mode
	const ImageMapStorage::FilterType filterType = pixelStorage->GetFilterType();
	// I can delete the current image
	pixelStorage.reset();

	// Allocate the new image map storage
	switch (storageType) {
		case ImageMapStorage::BYTE: {
			pixelStorage = AllocImageMapStorage<u_char>(channelCount, newWidth, newHeight, wrapType, filterType);
			break;
		}
		case ImageMapStorage::HALF: {
			pixelStorage = AllocImageMapStorage<half>(channelCount, newWidth, newHeight, wrapType, filterType);
			break;
		}
		case ImageMapStorage::FLOAT: {
			pixelStorage = AllocImageMapStorage<float>(channelCount, newWidth, newHeight, wrapType, filterType);
			break;
		}
		default:
			throw runtime_error("Unsupported storage type in ImageMap::Resize(): " + ToString(storageType));
	}

	dest.get_pixels(roi, baseType, pixelStorage->GetPixelsData());
}

string ImageMap::GetFileExtension() const {
	ImageMapStorage::StorageType storageType = pixelStorage->GetStorageType();

	switch (pixelStorage->GetStorageType()) {
		case ImageMapStorage::BYTE:
			return "png";
		case ImageMapStorage::HALF:
		case ImageMapStorage::FLOAT:
			return "exr";
		default:
			throw runtime_error("Unsupported storage type in ImageMap::GetFileExtension(): " + ToString(storageType));
	}
}

void ImageMap::WriteImage(const string &fileName) const {
	std::unique_ptr<ImageOutput> out(ImageOutput::create(fileName));
	if (!out) throw runtime_error("Failed image save: " + fileName);

	ImageMapStorage::StorageType storageType = pixelStorage->GetStorageType();

	switch (storageType) {
		case ImageMapStorage::BYTE: {
			ImageSpec spec(
				pixelStorage->GetWidth(),
				pixelStorage->GetHeight(),
				pixelStorage->GetChannelCount(),
				TypeDesc::UCHAR
			);
			out->open(fileName, spec);
			out->write_image(TypeDesc::UCHAR, pixelStorage->GetPixelsData());
			out->close();
			break;
		}
		case ImageMapStorage::HALF: {
			ImageSpec spec(
				pixelStorage->GetWidth(),
				pixelStorage->GetHeight(),
				pixelStorage->GetChannelCount(),
				TypeDesc::HALF
			);
			out->open(fileName, spec);
			out->write_image(TypeDesc::HALF, pixelStorage->GetPixelsData());
			out->close();
			break;
		}
		case ImageMapStorage::FLOAT: {
			if (pixelStorage->GetChannelCount() == 1) {
				// OIIO 1 channel EXR output is apparently not working, I write
				// 3 channels as temporary workaround
				const u_int size = pixelStorage->GetWidth() * pixelStorage->GetHeight();
				const float *srcBuffer = (float *)pixelStorage->GetPixelsData();
				float *tmpBuffer = new float[size * 3];

				float *tmpBufferPtr = tmpBuffer;
				for (u_int i = 0; i < size; ++i) {
					const float v = srcBuffer[i];
					*tmpBufferPtr++ = v;
					*tmpBufferPtr++ = v;
					*tmpBufferPtr++ = v;
				}

				ImageSpec spec(
					pixelStorage->GetWidth(), pixelStorage->GetHeight(), 3, TypeDesc::FLOAT
				);
				out->open(fileName, spec);
				out->write_image(TypeDesc::FLOAT, tmpBuffer);
				out->close();

				delete[] tmpBuffer;
			} else {
				ImageSpec spec(
					pixelStorage->GetWidth(),
					pixelStorage->GetHeight(),
					pixelStorage->GetChannelCount(),
					TypeDesc::FLOAT
				);
				out->open(fileName, spec);
				out->write_image(TypeDesc::FLOAT, pixelStorage->GetPixelsData());
				out->close();
			}
			break;
		}
		default:
			throw runtime_error(
				"Unsupported storage type in ImageMap::WriteImage(): "
				+ ToString(storageType)
			);
	}  // switch(storageType)
}

ImageMapUPtr ImageMap::Copy() const {
	return std::make_unique<ImageMap>(
		pixelStorage->Copy(), imageMean, imageMeanY, imageMapConfig
	);
}

ImageMapUPtr ImageMap::Merge(
	ImageMapConstRef map0,
	ImageMapConstRef map1,
	const u_int channels,
	const u_int width,
	const u_int height
) {
	if (channels == 1) {
		// I assume the images have the same gamma
		auto imgMap = AllocImageMap(
			1, width, height,
			ImageMapConfig(
				1.f,
				ImageMapStorage::StorageType::FLOAT,
				map0.GetStorage().GetWrapType(),
				ImageMapStorage::ChannelSelectionType::DEFAULT
			)
		);
		float *mergedImg = (float *)imgMap->GetStorage().GetPixelsData();

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				mergedImg[x + y * width] = map0.GetFloat(uv) * map1.GetFloat(uv);
			}
		}

		return imgMap;
	} else if (channels == 3) {
		// I assume the images have the same gamma
		auto imgMap = AllocImageMap(
			3, width, height,
			ImageMapConfig(
				1.f,
				ImageMapStorage::StorageType::FLOAT,
				map0.GetStorage().GetWrapType(),
				ImageMapStorage::ChannelSelectionType::DEFAULT)
			);
		float *mergedImg = (float *)imgMap->GetStorage().GetPixelsData();

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				const Spectrum c = map0.GetSpectrum(uv) * map1.GetSpectrum(uv);

				const u_int dstIndex = (x + y * width) * 3;
				mergedImg[dstIndex] = c.c[0];
				mergedImg[dstIndex + 1] = c.c[1];
				mergedImg[dstIndex + 2] = c.c[2];
			}
		}

		return imgMap;
	} else
		throw runtime_error("Unsupported number of channels in ImageMap::Merge(): " + ToString(channels));
}

ImageMapUPtr ImageMap::Merge(ImageMapConstRef map0, ImageMapConstRef map1, const u_int channels) {
	const u_int width = Max(map0.GetWidth(), map1.GetWidth());
	const u_int height = Max(map0.GetHeight(), map1.GetHeight());

	return ImageMap::Merge(map0, map1, channels, width, height);
}

ImageMapUPtr ImageMap::Resample(ImageMapConstRef map, const u_int channels,
		const u_int width, const u_int height) {
	if (channels == 1) {
		auto imgMap = AllocImageMap(1, width, height,
				ImageMapConfig(1.f,
					ImageMapStorage::StorageType::FLOAT,
					map.GetStorage().GetWrapType(),
					ImageMapStorage::ChannelSelectionType::DEFAULT));
		float *newImg = (float *)imgMap->GetStorage().GetPixelsData();

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				newImg[x + y * width] = map.GetFloat(uv);
			}
		}

		return imgMap;
	} else if (channels == 3) {
		auto imgMap = AllocImageMap(3, width, height,
				ImageMapConfig(1.f,
					ImageMapStorage::StorageType::FLOAT,
					map.GetStorage().GetWrapType(),
					ImageMapStorage::ChannelSelectionType::DEFAULT));
		float *newImg = (float *)imgMap->GetStorage().GetPixelsData();

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				const Spectrum c = map.GetSpectrum(uv);

				const u_int index = (x + y * width) * 3;
				newImg[index] = c.c[0];
				newImg[index + 1] = c.c[1];
				newImg[index + 2] = c.c[2];
			}
		}

		return imgMap;
	} else
		throw runtime_error("Unsupported number of channels in ImageMap::Resample(): " + ToString(channels));
}

pair<u_int, u_int> ImageMap::GetSize(const std::string &fileName) {
	const string resolvedFileName = SLG_FileNameResolver.ResolveFile(fileName);

	if (!std::filesystem::exists(resolvedFileName))
		throw runtime_error("ImageMap file doesn't exist: " + resolvedFileName + " (GetSize)");
	else {
		OIIO::attribute("openexr:core", 1);  // Nota: crash if not set
		ImageSpec config;
		config["oiio:UnassociatedAlpha"] = 1;
		auto in = ImageInput::open(resolvedFileName, &config);

		if (in.get()) {
			const ImageSpec &spec = in->spec();

			return make_pair(spec.width, spec.height);
		} else
			throw runtime_error("Error opening image file: " + resolvedFileName +
					" (error = " + geterror() +")");
	}
}

void ImageMap::MakeTx(const std::string &srcFileName, const std::string &dstFileName) {
	ImageBuf Input(srcFileName);

	ImageSpec config;
	stringstream s;
	if (!ImageBufAlgo::make_texture(ImageBufAlgo::MakeTxTexture, Input, dstFileName, config, &s))
		throw runtime_error("ImageMap::MakeTx error: " + s.str());
}

ImageMapUPtr ImageMap::FromProperties(const Properties &props, const string &prefix) {
	ImageMapUPtr im;
	if (props.IsDefined(prefix + ".file")) {
		// Read the image map from a file
		const string fileName = props.Get(Property(prefix + ".file")("image.png")).Get<string>();

		im = std::make_unique<ImageMap>(fileName, ImageMapConfig(props, prefix));
	} else if (props.IsDefined(prefix + ".blob")) {
		// Read the image map from embedded data
		const u_int width = props.Get(Property(prefix + ".blob.width")(512)).Get<u_int>();
		const u_int height = props.Get(Property(prefix + ".blob.height")(512)).Get<u_int>();
		const u_int channelCount = props.Get(Property(prefix + ".blob.channelcount")(3)).Get<u_int>();

		const ImageMapStorage::StorageType storageType = ImageMapStorage::String2StorageType(
			props.Get(Property(prefix + ".storage")("auto")).Get<string>());
		const ImageMapStorage::WrapType wrapType = ImageMapStorage::String2WrapType(
				props.Get(Property(prefix + ".wrap")("repeat")).Get<string>());
		const ImageMapStorage::FilterType filterType = ImageMapStorage::String2FilterType(
				props.Get(Property(prefix + ".filter")("linear")).Get<string>());

		ImageMapStorageUPtr pixelStorage;
		switch (storageType) {
			case ImageMapStorage::BYTE: {
				pixelStorage = AllocImageMapStorage<u_char>(channelCount, width, height, wrapType, filterType);
				break;
			}
			case ImageMapStorage::HALF: {
				pixelStorage = AllocImageMapStorage<half>(channelCount, width, height, wrapType, filterType);
				break;
			}
			case ImageMapStorage::FLOAT: {
				pixelStorage = AllocImageMapStorage<float>(channelCount, width, height, wrapType, filterType);
				break;
			}
			default:
				throw runtime_error("Unsupported selected storage type in ImageMap::FromProperties(): " + ToString(storageType));
		}

		const Blob &blob = props.Get(Property(prefix + ".blob")).Get<const Blob &>();
		copy(blob.GetData(), blob.GetData() + blob.GetSize(), (char *)pixelStorage->GetPixelsData());

		ImageMapConfig imageMapConfig;
		ImageMapConfig::FromProperties(props, prefix, imageMapConfig);

		im = std::make_unique<ImageMap>(
			std::move(pixelStorage), 0.f, 0.f, imageMapConfig
		);
		im->Preprocess();
	} else
		throw runtime_error("Missing data ImageMap::FromProperties()");

	return im;
}

PropertiesUPtr ImageMap::ToProperties(const string &prefix, const bool includeBlobImg) const {
	auto props_ptr = std::make_unique<Properties>();
	auto& props = *props_ptr;

	props <<
			// The image is internally stored always in NOP_COLORSPACE
			Property(prefix + ".colorspace")("nop") <<
			Property(prefix + ".storage")(ImageMapStorage::StorageType2String(pixelStorage->GetStorageType()));
			Property(prefix + ".wrap")(ImageMapStorage::WrapType2String(pixelStorage->GetWrapType()));
			Property(prefix + ".filter")(ImageMapStorage::FilterType2String(pixelStorage->GetFilterType()));

	if (includeBlobImg)
		props <<
				Property(prefix + ".blob")(
					std::make_shared<Blob>((char *)pixelStorage->GetPixelsData(), pixelStorage->GetMemorySize())
					) <<
				Property(prefix + ".blob.width")(pixelStorage->GetWidth()) <<
				Property(prefix + ".blob.height")(pixelStorage->GetHeight()) <<
				Property(prefix + ".blob.channelcount")(pixelStorage->GetChannelCount());

	return props_ptr;
}


bool operator==(slg::ImageMapConstRef im1, slg::ImageMapConstRef im2) {
	return &im1 == &im2;
}

//------------------------------------------------------------------------------
// Static random image map used by some texture
//------------------------------------------------------------------------------

ImageMapUPtr ImageMap::AllocRandomImageMap(const u_int size) {

	// Initialize the random image map
	auto randomImageMap = ImageMap::AllocImageMap(3, size, size, ImageMapConfig());

	randomImageMap->SetName("Random-Image-Map");


	RandomGenerator rndGen(123);
	float *randomMapData = (float *)randomImageMap->GetStorage().GetPixelsData();
	for (u_int i = 0; i < 3 * size * size; ++i)
		randomMapData[i] = rndGen.floatValue();

	return randomImageMap;
}

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
