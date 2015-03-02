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

#include <sstream>
#include <algorithm>
#include <numeric>
#include <memory>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>

#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/dassert.h>

#include "slg/imagemap/imagemap.h"
#include "slg/sdl/sdl.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMapStorageImpl
//------------------------------------------------------------------------------

template <class T, u_int CHANNELS>
float ImageMapStorageImpl<T, CHANNELS>::GetFloat(const UV &uv) const {
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

template <class T, u_int CHANNELS>
float ImageMapStorageImpl<T, CHANNELS>::GetFloat(const u_int index) const {
	assert (index >= 0);
	assert (index < width * height);

	return pixels[index].GetFloat();
}

template <class T, u_int CHANNELS>
Spectrum ImageMapStorageImpl<T, CHANNELS>::GetSpectrum(const UV &uv) const {
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
const ImageMapPixel<T, CHANNELS> *ImageMapStorageImpl<T, CHANNELS>::GetTexel(const int s, const int t) const {
	const u_int u = Mod<int>(s, width);
	const u_int v = Mod<int>(t, height);

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
				// Visusl C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
				unsigned
#endif
				int i = 0; i < width * height; i++)
			pixels[i].ReverseGammaCorrection(gamma);
	}
}

//------------------------------------------------------------------------------
// ImageMap
//------------------------------------------------------------------------------

ImageMap::ImageMap(const string &fileName, const float g,
		const ChannelSelectionType selectionType,
		const ImageMapStorage::StorageType storageType) {
	gamma = g;

	SDL_LOG("Reading texture map: " << fileName);

	if (!boost::filesystem::exists(fileName))
		throw runtime_error("ImageMap file doesn't exist: " + fileName);
	else {
		auto_ptr<ImageInput> in(ImageInput::open(fileName));

		if (in.get()) {
			const ImageSpec &spec = in->spec();

			u_int width = spec.width;
			u_int height = spec.height;
			u_int channelCount = spec.nchannels;

			if ((channelCount != 1) && (channelCount != 3) && (channelCount != 4))
				throw runtime_error("Unsupported number of channels in an ImageMap: " + ToString(channelCount));

			ImageMapStorage::StorageType selectedStorageType = storageType;
			if (selectedStorageType == ImageMapStorage::AUTO) {
				// Automatically select the storage type

				if (spec.format == TypeDesc::UCHAR)
					selectedStorageType = ImageMapStorage::BYTE;
				else if (spec.format == TypeDesc::HALF)
					selectedStorageType = ImageMapStorage::HALF;
				else
					selectedStorageType = ImageMapStorage::FLOAT;
			}
			
			const u_int pixelCount = width * height;
			switch (selectedStorageType) {
				case ImageMapStorage::BYTE: {
					u_char *pixels = new u_char[pixelCount * channelCount];
					in->read_image(TypeDesc::UCHAR, pixels);
					in->close();
					in.reset();

					pixelStorage = AllocImageMapStorage<u_char>(pixels, channelCount, width, height);
					break;
				}
				case ImageMapStorage::HALF: {
					half *pixels = new half[pixelCount * channelCount];
					in->read_image(TypeDesc::HALF, pixels);
					in->close();
					in.reset();

					pixelStorage = AllocImageMapStorage<half>(pixels, channelCount, width, height);
					break;
				}
				case ImageMapStorage::FLOAT: {
					float *pixels = new float[pixelCount * channelCount];
					in->read_image(TypeDesc::FLOAT, pixels);
					in->close();
					in.reset();

					pixelStorage = AllocImageMapStorage<float>(pixels, channelCount, width, height);
					break;
				}
				default:
					throw runtime_error("Unsupported selected storage type in an ImageMap: " + ToString(selectedStorageType));
			}

// TODO
// TODO
// TODO
			// Convert the image if required
//			switch (selectionType) {
//				case ImageMap::DEFAULT:
//					// Nothing to do
//					break;
//				case ImageMap::RED:
//				case ImageMap::GREEN:
//				case ImageMap::BLUE:
//				case ImageMap::ALPHA: {
//					// Nothing to do
//					if (channelCount == 1) {
//						// Nothing to do
//					} else if (channelCount == 2) {
//						auto_ptr<float> newPixels(new float[pixelCount]);
//
//						const float *src = pixels.get();
//						float *dst = newPixels.get();
//						const u_int channel = (
//							(selectionType == ImageMap::RED) ||
//							(selectionType == ImageMap::GREEN) ||
//							(selectionType == ImageMap::BLUE)) ? 0 : 1;
//
//						for (u_int i = 0; i < pixelCount; ++i) {
//							*dst++ = src[channel];
//							src += channelCount;
//						}
//						
//						pixels.reset(newPixels.release());
//						channelCount = 1;
//					} else {
//						auto_ptr<float> newPixels(new float[pixelCount]);
//
//						const float *src = pixels.get();
//						float *dst = newPixels.get();
//						const u_int channel = selectionType - ImageMap::RED;
//
//						for (u_int i = 0; i < pixelCount; ++i) {
//							*dst++ = src[channel];
//							src += channelCount;
//						}
//						
//						pixels.reset(newPixels.release());
//						channelCount = 1;
//					}
//					break;
//				}
//				case ImageMap::MEAN:
//				case ImageMap::WEIGHTED_MEAN: {
//					// Nothing to do
//					if (channelCount == 1) {
//						// Nothing to do
//					} else if (channelCount == 2) {
//						auto_ptr<float> newPixels(new float[pixelCount]);
//
//						const float *src = pixels.get();
//						float *dst = newPixels.get();
//						const u_int channel = 0;
//
//						for (u_int i = 0; i < pixelCount; ++i) {
//							*dst++ = src[channel];
//							src += channelCount;
//						}
//
//						pixels.reset(newPixels.release());
//						channelCount = 1;
//					} else {
//						auto_ptr<float> newPixels(new float[pixelCount]);
//
//						const float *src = pixels.get();
//						float *dst = newPixels.get();
//
//						if (selectionType == ImageMap::MEAN) {
//							for (u_int i = 0; i < pixelCount; ++i) {
//								*dst++ = Spectrum(src).Filter();
//								src += channelCount;
//							}
//						} else {
//							for (u_int i = 0; i < pixelCount; ++i) {
//								*dst++ = Spectrum(src).Y();
//								src += channelCount;
//							}							
//						}
//						
//						pixels.reset(newPixels.release());
//						channelCount = 1;
//					}
//				}
//				case ImageMap::RGB: {
//					// Nothing to do
//					if ((channelCount == 1) || (channelCount == 2) || (channelCount == 3)) {
//						// Nothing to do
//					} else {
//						auto_ptr<float> newPixels(new float[pixelCount * 3]);
//
//						const float *src = pixels.get();
//						float *dst = newPixels.get();
//
//						for (u_int i = 0; i < pixelCount; ++i) {
//							*dst++ = *src++;
//							*dst++ = *src++;
//							*dst++ = *src++;
//							src++;
//						}
//						
//						pixels.reset(newPixels.release());
//						channelCount = 1;
//					}
//					break;
//				}
//				default:
//					throw runtime_error("Unknown channel selection type in an ImageMap: " + ToString(selectionType));
//			}
		} else
			throw runtime_error("Unknown image file format: " + fileName);

		pixelStorage->ReverseGammaCorrection(gamma);
	}
}

ImageMap::ImageMap(ImageMapStorage *pixels, const float g) {
	pixelStorage = pixels;
	gamma = g;
}

ImageMap::~ImageMap() {
	delete pixelStorage;
}

void ImageMap::Resize(const u_int newWidth, const u_int newHeight) {
	const u_int width = pixelStorage->width;
	const u_int height = pixelStorage->height;
	if ((width == newHeight) && (height == newHeight))
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
	
	ImageSpec spec(width, height, channelCount, baseType);
	
	ImageBuf source(spec, (void *)pixelStorage->GetPixelsData());
	ImageBuf dest;
	
	ROI roi(0, newWidth, 0,newHeight, 0, 1, 0, source.nchannels());
	ImageBufAlgo::resize(dest, source, "", 0, roi);

	// I can delete the current image
	delete pixelStorage;

	// Allocate the new image map storage
	switch (storageType) {
		case ImageMapStorage::BYTE: {
			u_char *pixels = new u_char[newWidth * newHeight * channelCount];
			dest.get_pixels(0, newHeight, 0, newHeight, 0, 1, baseType, pixels);

			pixelStorage = AllocImageMapStorage<u_char>(pixels, channelCount, newWidth, newHeight);
			break;
		}
		case ImageMapStorage::HALF: {
			half *pixels = new half[newWidth * newHeight * channelCount];
			dest.get_pixels(0, newHeight, 0, newHeight, 0, 1, baseType, pixels);

			pixelStorage = AllocImageMapStorage<half>(pixels, channelCount, newWidth, newHeight);
			break;
		}
		case ImageMapStorage::FLOAT: {
			float *pixels = new float[newWidth * newHeight * channelCount];
			dest.get_pixels(0, newHeight, 0, newHeight, 0, 1, baseType, pixels);

			pixelStorage = AllocImageMapStorage<float>(pixels, channelCount, newWidth, newHeight);
			break;
		}
		default:
			throw runtime_error("Unsupported storage type in ImageMap::Resize(): " + ToString(storageType));
	}
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
	ImageOutput *out = ImageOutput::create(fileName);
	if (out) {
		ImageMapStorage::StorageType storageType = pixelStorage->GetStorageType();

		switch (storageType) {
			case ImageMapStorage::BYTE: {
				ImageSpec spec(pixelStorage->width, pixelStorage->height, pixelStorage->GetChannelCount(), TypeDesc::UCHAR);
				out->open(fileName, spec);
				out->write_image(TypeDesc::UCHAR, pixelStorage->GetPixelsData());
				out->close();
				break;
			}
			case ImageMapStorage::HALF: {
				ImageSpec spec(pixelStorage->width, pixelStorage->height, pixelStorage->GetChannelCount(), TypeDesc::HALF);
				out->open(fileName, spec);
				out->write_image(TypeDesc::HALF, pixelStorage->GetPixelsData());
				out->close();
				break;
			}
			case ImageMapStorage::FLOAT: {
				ImageSpec spec(pixelStorage->width, pixelStorage->height, pixelStorage->GetChannelCount(), TypeDesc::FLOAT);
				out->open(fileName, spec);
				out->write_image(TypeDesc::FLOAT, pixelStorage->GetPixelsData());
				out->close();
				break;
			}
			default:
				throw runtime_error("Unsupported storage type in ImageMap::WriteImage(): " + ToString(storageType));
		}

		delete out;
	} else
		throw runtime_error("Failed image save: " + fileName);
}

float ImageMap::GetSpectrumMean() const {
	float mean = 0.f;	
	for (u_int y = 0; y < pixelStorage->height; ++y) {
		for (u_int x = 0; x < pixelStorage->width; ++x) {
			const u_int index = x + y * pixelStorage->width;
			
			const Spectrum s = pixelStorage->GetSpectrum(index);
			mean += (s.c[0] + s.c[1] + s.c[2]) * (1.f / 3.f);
		}
	}

	return mean / (pixelStorage->width * pixelStorage->height);
}

float ImageMap::GetSpectrumMeanY() const {
	float mean = 0.f;	
	for (u_int y = 0; y < pixelStorage->height; ++y) {
		for (u_int x = 0; x < pixelStorage->width; ++x) {
			const u_int index = x + y * pixelStorage->width;
			
			const Spectrum s = pixelStorage->GetSpectrum(index);
			mean += s.Y();
		}
	}

	return mean / (pixelStorage->width * pixelStorage->height);
}

ImageMap *ImageMap::Merge(const ImageMap *map0, const ImageMap *map1, const u_int channels,
		const u_int width, const u_int height) {
	if (channels == 1) {
		float *mergedImg = new float[width * height];

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				mergedImg[x + y * width] = map0->GetFloat(uv) * map1->GetFloat(uv);
			}
		}

		// I assume the images have the same gamma
		return AllocImageMap<float>(mergedImg, map0->GetGamma(), 1, width, height);
	} else if (channels == 3) {
		float *mergedImg = new float[width * height * 3];

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				const Spectrum c = map0->GetSpectrum(uv) * map1->GetSpectrum(uv);

				const u_int dstIndex = (x + y * width) * 3;
				mergedImg[dstIndex] = c.c[0];
				mergedImg[dstIndex + 1] = c.c[1];
				mergedImg[dstIndex + 2] = c.c[2];
			}
		}

		// I assume the images have the same gamma
		return AllocImageMap<float>(mergedImg, map0->GetGamma(), 3, width, height);
	} else
		throw runtime_error("Unsupported number of channels in ImageMap::Merge(): " + ToString(channels));
}

ImageMap *ImageMap::Merge(const ImageMap *map0, const ImageMap *map1, const u_int channels) {
	const u_int width = Max(map0->GetWidth(), map1->GetWidth());
	const u_int height = Max(map0->GetHeight(), map1->GetHeight());

	return ImageMap::Merge(map0, map1, channels, width, height);
}

ImageMap *ImageMap::Resample(const ImageMap *map, const u_int channels,
		const u_int width, const u_int height) {
	if (channels == 1) {
		float *newImg = new float[width * height];

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				newImg[x + y * width] = map->GetFloat(uv);
			}
		}

		return AllocImageMap<float>(newImg, map->GetGamma(), 1, width, height);
	} else if (channels == 3) {
		float *newImg = new float[width * height * 3];

		for (u_int y = 0; y < height; ++y) {
			for (u_int x = 0; x < width; ++x) {
				const UV uv((x + .5f) / width, (y + .5f) / height);
				const Spectrum c = map->GetSpectrum(uv);

				const u_int index = (x + y * width) * 3;
				newImg[index] = c.c[0];
				newImg[index + 1] = c.c[1];
				newImg[index + 2] = c.c[2];
			}
		}

		return AllocImageMap<float>(newImg, map->GetGamma(), 3, width, height);
	} else
		throw runtime_error("Unsupported number of channels in ImageMap::Merge(): " + ToString(channels));
}
