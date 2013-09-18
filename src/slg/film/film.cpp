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

#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include "slg/film/film.h"
#include "luxrays/core/geometry/point.h"
#include "slg/editaction.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// FilmOutput
//------------------------------------------------------------------------------

void FilmOutputs::Add(const FilmOutputType type, const std::string &fileName) {
	types.push_back(type);
	fileNames.push_back(fileName);
}

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

Film::Film(const u_int w, const u_int h) {
	initialized = false;

	width = w;
	height = h;

	channel_RADIANCE_PER_PIXEL_NORMALIZED = NULL;
	channel_RADIANCE_PER_SCREEN_NORMALIZED = NULL;
	channel_ALPHA = NULL;
	channel_RGB_TONEMAPPED = NULL;
	channel_DEPTH = NULL;
	channel_POSITION = NULL;
	channel_GEOMETRY_NORMAL = NULL;
	channel_SHADING_NORMAL = NULL;
	channel_MATERIAL_ID = NULL;
	channel_DIRECT_DIFFUSE = NULL;
	channel_DIRECT_GLOSSY = NULL;

	convTest = NULL;

	enabledOverlappedScreenBufferUpdate = true;

	filter = NULL;
	filterLUTs = NULL;
	SetFilter(new GaussianFilter(1.5f, 1.5f, 2.f));

	toneMapParams = new LinearToneMapParams();

	SetGamma();
}

Film::~Film() {
	delete toneMapParams;

	delete convTest;

	delete channel_RADIANCE_PER_PIXEL_NORMALIZED;
	delete channel_RADIANCE_PER_SCREEN_NORMALIZED;
	delete channel_ALPHA;
	delete channel_RGB_TONEMAPPED;
	delete channel_DEPTH;
	delete channel_POSITION;
	delete channel_GEOMETRY_NORMAL;
	delete channel_SHADING_NORMAL;
	delete channel_MATERIAL_ID;
	delete channel_DIRECT_DIFFUSE;
	delete channel_DIRECT_GLOSSY;

	delete filterLUTs;
	delete filter;
}

void Film::AddChannel(const FilmChannelType type) {
	if (initialized)
		throw std::runtime_error("it is possible to add a channel to a Film only before the initialization");

	channels.insert(type);
}

void Film::RemoveChannel(const FilmChannelType type) {
	if (initialized)
		throw std::runtime_error("it is possible to remove a channel of a Film only before the initialization");

	channels.erase(type);
}

void Film::Init(const u_int w, const u_int h) {
	if (initialized)
		throw std::runtime_error("A Film can not be initialized multiple times");

	initialized = true;
	width = w;
	height = h;
	pixelCount = w * h;

	delete convTest;
	convTest = NULL;

	// Delete all already allocated channels
	delete channel_RADIANCE_PER_PIXEL_NORMALIZED;
	delete channel_RADIANCE_PER_SCREEN_NORMALIZED;
	delete channel_ALPHA;
	delete channel_RGB_TONEMAPPED;
	delete channel_DEPTH;
	delete channel_POSITION;
	delete channel_GEOMETRY_NORMAL;
	delete channel_SHADING_NORMAL;
	delete channel_MATERIAL_ID;
	delete channel_DIRECT_DIFFUSE;
	delete channel_DIRECT_GLOSSY;
	
	// Allocate all required channels
	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		channel_RADIANCE_PER_PIXEL_NORMALIZED = new GenericFrameBuffer<4, float>(width, height);
		channel_RADIANCE_PER_PIXEL_NORMALIZED->Clear();
	}
	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		channel_RADIANCE_PER_SCREEN_NORMALIZED = new GenericFrameBuffer<4, float>(width, height);
		channel_RADIANCE_PER_SCREEN_NORMALIZED->Clear();
	}
	if (HasChannel(ALPHA)) {
		channel_ALPHA = new GenericFrameBuffer<2, float>(width, height);
		channel_ALPHA->Clear();
	}
	if (HasChannel(TONEMAPPED_FRAMEBUFFER)) {
		channel_RGB_TONEMAPPED = new GenericFrameBuffer<3, float>(width, height);
		channel_RGB_TONEMAPPED->Clear();

		convTest = new ConvergenceTest(width, height);
	}
	if (HasChannel(DEPTH)) {
		channel_DEPTH = new GenericFrameBuffer<1, float>(width, height);
		channel_DEPTH->Clear(std::numeric_limits<float>::infinity());
	}
	if (HasChannel(POSITION)) {
		channel_POSITION = new GenericFrameBuffer<3, float>(width, height);
		channel_POSITION->Clear(std::numeric_limits<float>::infinity());
	}
	if (HasChannel(GEOMETRY_NORMAL)) {
		channel_GEOMETRY_NORMAL = new GenericFrameBuffer<3, float>(width, height);
		channel_GEOMETRY_NORMAL->Clear(std::numeric_limits<float>::infinity());
	}
	if (HasChannel(SHADING_NORMAL)) {
		channel_SHADING_NORMAL = new GenericFrameBuffer<3, float>(width, height);
		channel_SHADING_NORMAL->Clear(std::numeric_limits<float>::infinity());
	}
	if (HasChannel(MATERIAL_ID)) {
		channel_MATERIAL_ID = new GenericFrameBuffer<1, u_int>(width, height);
		channel_MATERIAL_ID->Clear(std::numeric_limits<u_int>::max());
	}
	if (HasChannel(DIRECT_DIFFUSE)) {
		channel_DIRECT_DIFFUSE = new GenericFrameBuffer<4, float>(width, height);
		channel_DIRECT_DIFFUSE->Clear();
	}
	if (HasChannel(DIRECT_GLOSSY)) {
		channel_DIRECT_GLOSSY = new GenericFrameBuffer<4, float>(width, height);
		channel_DIRECT_GLOSSY->Clear();
	}
		
	// Initialize the stats
	statsTotalSampleCount = 0.0;
	statsAvgSampleSec = 0.0;
	statsStartSampleTime = WallClockTime();
}

void Film::SetGamma(const float g) {
	gamma = g;

	float x = 0.f;
	const float dx = 1.f / GAMMA_TABLE_SIZE;
	for (u_int i = 0; i < GAMMA_TABLE_SIZE; ++i, x += dx)
		gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / g);
}

void Film::SetFilter(Filter *flt) {
	delete filterLUTs;
	filterLUTs = NULL;
	delete filter;
	filter = flt;

	if (filter) {
		const u_int size = Max<u_int>(4, Max(filter->xWidth, filter->yWidth) + 1);
		filterLUTs = new FilterLUTs(*filter, size);
	}
}

void Film::Reset() {
	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED))
		channel_RADIANCE_PER_PIXEL_NORMALIZED->Clear();
	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED))
		channel_RADIANCE_PER_SCREEN_NORMALIZED->Clear();
	if (HasChannel(ALPHA))
		channel_ALPHA->Clear();
	if (HasChannel(DEPTH))
		channel_DEPTH->Clear(std::numeric_limits<float>::infinity());
	if (HasChannel(POSITION))
		channel_POSITION->Clear(std::numeric_limits<float>::infinity());
	if (HasChannel(GEOMETRY_NORMAL))
		channel_GEOMETRY_NORMAL->Clear(std::numeric_limits<float>::infinity());
	if (HasChannel(SHADING_NORMAL))
		channel_SHADING_NORMAL->Clear(std::numeric_limits<float>::infinity());
	if (HasChannel(MATERIAL_ID))
		channel_MATERIAL_ID->Clear(std::numeric_limits<float>::max());
	if (HasChannel(DIRECT_DIFFUSE))
		channel_DIRECT_DIFFUSE->Clear();
	if (HasChannel(DIRECT_GLOSSY))
		channel_DIRECT_GLOSSY->Clear();

	// convTest has to be reseted explicitely

	statsTotalSampleCount = 0.0;
	statsAvgSampleSec = 0.0;
	statsStartSampleTime = WallClockTime();
}

void Film::AddFilm(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY) {
	statsTotalSampleCount += film.statsTotalSampleCount;

	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && film.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_RADIANCE_PER_PIXEL_NORMALIZED->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_RADIANCE_PER_PIXEL_NORMALIZED->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED) && film.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_RADIANCE_PER_SCREEN_NORMALIZED->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_RADIANCE_PER_SCREEN_NORMALIZED->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(ALPHA) && film.HasChannel(ALPHA)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_ALPHA->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_ALPHA->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(POSITION) && film.HasChannel(POSITION)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const float *srcPixel = film.channel_POSITION->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_POSITION->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_POSITION->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_POSITION->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(GEOMETRY_NORMAL) && film.HasChannel(GEOMETRY_NORMAL)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const float *srcPixel = film.channel_GEOMETRY_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_GEOMETRY_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_GEOMETRY_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_GEOMETRY_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(SHADING_NORMAL) && film.HasChannel(SHADING_NORMAL)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const float *srcPixel = film.channel_SHADING_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_SHADING_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_SHADING_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_SHADING_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(MATERIAL_ID) && film.HasChannel(MATERIAL_ID)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const u_int *srcPixel = film.channel_MATERIAL_ID->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_MATERIAL_ID->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const u_int *srcPixel = film.channel_MATERIAL_ID->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_MATERIAL_ID->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(DIRECT_DIFFUSE) && film.HasChannel(DIRECT_DIFFUSE)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_DIFFUSE->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_DIRECT_DIFFUSE->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(DIRECT_GLOSSY) && film.HasChannel(DIRECT_GLOSSY)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_GLOSSY->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_DIRECT_GLOSSY->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	// NOTE: update DEPTH channel as last because it is used to merge other channels
	if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_DEPTH->MinPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}
}

void Film::Output(const FilmOutputs &filmOutputs) {
	for (u_int i = 0; i < filmOutputs.GetCount(); ++i)
		Output(filmOutputs.GetType(i), filmOutputs.GetFileName(i));
}

void Film::Output(const FilmOutputs::FilmOutputType type, const std::string &fileName) {
	// Image format
	FREE_IMAGE_FORMAT fif = FREEIMAGE_GETFIFFROMFILENAME(FREEIMAGE_CONVFILENAME(fileName).c_str());
	if (fif == FIF_UNKNOWN)
		throw std::runtime_error("Image type unknown");

	// HDR image or not
	const bool hdrImage = ((fif == FIF_HDR) || (fif == FIF_EXR));

	// Image type and bit count
	FREE_IMAGE_TYPE imageType;
	u_int bitCount;
	switch (type) {
		case FilmOutputs::RGB:
			if (!hdrImage)
				return;
			imageType = FIT_RGBF;
			bitCount = 96;
			break;
		case FilmOutputs::RGB_TONEMAPPED:
			imageType = hdrImage ? FIT_RGBF : FIT_BITMAP;
			bitCount = hdrImage ? 96 : 24;
			break;
		case FilmOutputs::RGBA:
			if (!hdrImage)
				return;
			imageType = FIT_RGBAF;
			bitCount = 128;
			break;
		case FilmOutputs::RGBA_TONEMAPPED:
			imageType = hdrImage ? FIT_RGBAF : FIT_BITMAP;
			bitCount = hdrImage ? 128 : 32;
			break;
		case FilmOutputs::ALPHA:
			if (HasChannel(ALPHA)) {
				imageType = hdrImage ? FIT_FLOAT : FIT_BITMAP;
				bitCount = hdrImage ? 32 : 8;
			} else
				return;
			break;
		case FilmOutputs::DEPTH:
			if (HasChannel(DEPTH) && hdrImage) {
				imageType = FIT_FLOAT;
				bitCount = 32;
			} else
				return;
			break;
		case FilmOutputs::POSITION:
			if (HasChannel(POSITION) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::GEOMETRY_NORMAL:
			if (HasChannel(GEOMETRY_NORMAL) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::SHADING_NORMAL:
			if (HasChannel(SHADING_NORMAL) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::MATERIAL_ID:
			if (HasChannel(MATERIAL_ID) && !hdrImage) {
				imageType = FIT_BITMAP;
				bitCount = 24;
			} else
				return;
			break;
		case FilmOutputs::DIRECT_DIFFUSE:
			if (HasChannel(DIRECT_DIFFUSE) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::DIRECT_GLOSSY:
			if (HasChannel(DIRECT_GLOSSY) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		default:
			throw std::runtime_error("Unknown film output type");
	}

	if ((type == FilmOutputs::RGB) || (type == FilmOutputs::RGBA)) {
		// In order to merge the 2 sample buffers
		UpdateScreenBufferImpl(TONEMAP_NONE);
	}

	// Allocate the image
	FIBITMAP *dib = FreeImage_AllocateT(imageType, width, height, bitCount);
	if (!dib)
		throw std::runtime_error("Unable to allocate FreeImage image");

	// Build the image
	u_int pitch = FreeImage_GetPitch(dib);
	BYTE *bits = (BYTE *)FreeImage_GetBits(dib);
	for (u_int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x) {
			switch (type) {
				case FilmOutputs::RGB: {
					FIRGBF *dst = (FIRGBF *)bits;
					// channel_RGB_TONEMAPPED has the _untonemapped_ version of
					// the image in the case of an HDR because of the call
					// to UpdateScreenBufferImpl(TONEMAP_NONE)
					const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
					dst[x].red = src[0];
					dst[x].green = src[1];
					dst[x].blue = src[2];
					break;
				}
				case FilmOutputs::RGB_TONEMAPPED: {
					if (hdrImage) {
						FIRGBF *dst = (FIRGBF *)bits;
						const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
						dst[x].red = src[0];
						dst[x].green = src[1];
						dst[x].blue = src[2];
					} else {
						BYTE *dst = &bits[x * 3];
						const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
						dst[FI_RGBA_RED] = (BYTE)(src[0] * 255.f + .5f);
						dst[FI_RGBA_GREEN] = (BYTE)(src[1] * 255.f + .5f);
						dst[FI_RGBA_BLUE] = (BYTE)(src[2] * 255.f + .5f);
					}
					break;
				}
				case FilmOutputs::RGBA: {
					FIRGBAF *dst = (FIRGBAF *)bits;
					// channel_RGB_TONEMAPPED has the _untonemapped_ version of
					// the image in the case of an HDR because of the call
					// to UpdateScreenBufferImpl(TONEMAP_NONE)
					const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
					dst[x].red = src[0];
					dst[x].green = src[1];
					dst[x].blue = src[2];

					const float *alphaData = channel_ALPHA->GetPixel(x, y);
					if (alphaData[1] == 0.f)
						dst[x].alpha = 0.f;
					else
						dst[x].alpha = alphaData[0] / alphaData[1];
					break;
				}
				case FilmOutputs::RGBA_TONEMAPPED: {
					if (hdrImage) {
						FIRGBAF *dst = (FIRGBAF *)bits;
						const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
						dst[x].red = src[0];
						dst[x].green = src[1];
						dst[x].blue = src[2];

						const float *alphaData = channel_ALPHA->GetPixel(x, y);
						if (alphaData[1] == 0.f)
							dst[x].alpha = 0.f;
						else
							dst[x].alpha = alphaData[0] / alphaData[1];
					} else {
						BYTE *dst = &bits[x * 3];
						const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
						dst[FI_RGBA_RED] = (BYTE)(src[0] * 255.f + .5f);
						dst[FI_RGBA_GREEN] = (BYTE)(src[1] * 255.f + .5f);
						dst[FI_RGBA_BLUE] = (BYTE)(src[2] * 255.f + .5f);

						const float *alphaData = channel_ALPHA->GetPixel(x, y);
						if (alphaData[1] == 0.f)
							dst[FI_RGBA_ALPHA] = 0;
						else
							dst[FI_RGBA_ALPHA] = (BYTE)((alphaData[0] / alphaData[1]) * 255.f + .5f);
					}
					break;
				}
				case FilmOutputs::ALPHA: {
					if (hdrImage) {
						float *dst = (float *)bits;

						const float *alphaData = channel_ALPHA->GetPixel(x, y);
						if (alphaData[1] == 0.f)
							dst[x] = 0.f;
						else
							dst[x] = alphaData[0] / alphaData[1];
					} else {
						BYTE *dst = &bits[x];

						const float *alphaData = channel_ALPHA->GetPixel(x, y);
						if (alphaData[1] == 0.f)
							*dst = 0;
						else
							*dst = (BYTE)((alphaData[0] / alphaData[1]) * 255.f + .5f);
					}
					break;
				}
				case FilmOutputs::DEPTH: {
					float *dst = (float *)bits;

					dst[x] = channel_DEPTH->GetPixel(x, y)[0];
					break;
				}
				case FilmOutputs::POSITION: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_POSITION->GetPixel(x, y);
					dst[x].red = src[0];
					dst[x].green = src[1];
					dst[x].blue = src[2];
					break;
				}
				case FilmOutputs::GEOMETRY_NORMAL: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_GEOMETRY_NORMAL->GetPixel(x, y);
					dst[x].red = src[0];
					dst[x].green = src[1];
					dst[x].blue = src[2];
					break;
				}
				case FilmOutputs::SHADING_NORMAL: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_SHADING_NORMAL->GetPixel(x, y);
					dst[x].red = src[0];
					dst[x].green = src[1];
					dst[x].blue = src[2];
					break;
				}
				case FilmOutputs::MATERIAL_ID: {
					BYTE *dst = &bits[x * 3];
					const u_int *src = channel_MATERIAL_ID->GetPixel(x, y);
					dst[FI_RGBA_RED] = (BYTE)(src[0] & 0xff);
					dst[FI_RGBA_GREEN] = (BYTE)((src[0] & 0xff00) >> 8);
					dst[FI_RGBA_BLUE] = (BYTE)((src[0] & 0xff0000) >> 16);
					break;
				}
				case FilmOutputs::DIRECT_DIFFUSE: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_DIRECT_DIFFUSE->GetPixel(x, y);
					if (src[3] == 0.f) {
						dst[x].red = 0.f;
						dst[x].green = 0.f;
						dst[x].blue = 0.f;
					} else {
						const float iweight = 1.f / src[3];
						dst[x].red = src[0] * iweight;
						dst[x].green = src[1] * iweight;
						dst[x].blue = src[2] * iweight;
					}
					break;
				}
				case FilmOutputs::DIRECT_GLOSSY: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_DIRECT_GLOSSY->GetPixel(x, y);
					if (src[3] == 0.f) {
						dst[x].red = 0.f;
						dst[x].green = 0.f;
						dst[x].blue = 0.f;
					} else {
						const float iweight = 1.f / src[3];
						dst[x].red = src[0] * iweight;
						dst[x].green = src[1] * iweight;
						dst[x].blue = src[2] * iweight;
					}
					break;
				}
				default:
					throw std::runtime_error("Unknown film output type");
			}
		}

		// Next line
		bits += pitch;
	}

	if (!FREEIMAGE_SAVE(fif, dib, FREEIMAGE_CONVFILENAME(fileName).c_str(), 0))
		throw std::runtime_error("Failed image save");

	FreeImage_Unload(dib);

	if ((type == FilmOutputs::RGB) || (type == FilmOutputs::RGBA)) {
		// To restore the used tonemapping instead of NONE
		UpdateScreenBuffer();
	}
}

void Film::UpdateScreenBuffer() {
	UpdateScreenBufferImpl(toneMapParams->GetType());
}

void Film::MergeSampleBuffers(Spectrum *p, std::vector<bool> &frameBufferMask) const {
	const u_int pixelCount = width * height;

	// Merge RADIANCE_PER_PIXEL_NORMALIZED and RADIANCE_PER_SCREEN_NORMALIZED buffers

	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < pixelCount; ++i) {
			const float *sp = channel_RADIANCE_PER_PIXEL_NORMALIZED->GetPixel(i);

			const float weight = sp[3];
			if (weight > 0.f) {
				p[i] = Spectrum(sp) / weight;
				frameBufferMask[i] = true;
			}
		}
	}

	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		const float factor = pixelCount / statsTotalSampleCount;

		for (u_int i = 0; i < pixelCount; ++i) {
			const float *sp = channel_RADIANCE_PER_SCREEN_NORMALIZED->GetPixel(i);

			if (sp[3] > 0.f) {
				if (frameBufferMask[i])
					p[i] += Spectrum(sp) * factor;
				else
					p[i] = Spectrum(sp) * factor;
				frameBufferMask[i] = true;
			}
		}
	}

	if (!enabledOverlappedScreenBufferUpdate) {
		for (u_int i = 0; i < pixelCount; ++i) {
			if (!frameBufferMask[i]) {
				p[i].r = 0.f;
				p[i].g = 0.f;
				p[i].b = 0.f;
			}
		}
	}
}

void Film::UpdateScreenBufferImpl(const ToneMapType type) {
	if ((!HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && !HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) ||
			!HasChannel(TONEMAPPED_FRAMEBUFFER)) {
		// Nothing to do
		return;
	}

	switch (type) {
		case TONEMAP_NONE: {
			Spectrum *p = (Spectrum *)channel_RGB_TONEMAPPED->GetPixels();
			std::vector<bool> frameBufferMask(pixelCount, false);

			MergeSampleBuffers(p, frameBufferMask);
			break;
		}
		case TONEMAP_LINEAR: {
			const LinearToneMapParams &tm = (LinearToneMapParams &)(*toneMapParams);
			Spectrum *p = (Spectrum *)channel_RGB_TONEMAPPED->GetPixels();
			const u_int pixelCount = width * height;
			std::vector<bool> frameBufferMask(pixelCount, false);

			MergeSampleBuffers(p, frameBufferMask);

			// Gamma correction
			for (u_int i = 0; i < pixelCount; ++i) {
				if (frameBufferMask[i])
					p[i] = Radiance2Pixel(tm.scale * p[i]);
			}
			break;
		}
		case TONEMAP_REINHARD02: {
			const Reinhard02ToneMapParams &tm = (Reinhard02ToneMapParams &)(*toneMapParams);

			const float alpha = .1f;
			const float preScale = tm.preScale;
			const float postScale = tm.postScale;
			const float burn = tm.burn;

			Spectrum *p = (Spectrum *)channel_RGB_TONEMAPPED->GetPixels();
			const u_int pixelCount = width * height;

			std::vector<bool> frameBufferMask(pixelCount, false);
			MergeSampleBuffers(p, frameBufferMask);

			// Use the frame buffer as temporary storage and calculate the average luminance
			float Ywa = 0.f;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (frameBufferMask[i]) {
					// Convert to XYZ color space
					Spectrum xyz;
					xyz.r = 0.412453f * p[i].r + 0.357580f * p[i].g + 0.180423f * p[i].b;
					xyz.g = 0.212671f * p[i].r + 0.715160f * p[i].g + 0.072169f * p[i].b;
					xyz.b = 0.019334f * p[i].r + 0.119193f * p[i].g + 0.950227f * p[i].b;
					p[i] = xyz;

					Ywa += p[i].g;
				}
			}
			Ywa /= pixelCount;

			// Avoid division by zero
			if (Ywa == 0.f)
				Ywa = 1.f;

			const float Yw = preScale * alpha * burn;
			const float invY2 = 1.f / (Yw * Yw);
			const float pScale = postScale * preScale * alpha / Ywa;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (frameBufferMask[i]) {
					Spectrum xyz = p[i];

					const float ys = xyz.g;
					xyz *= pScale * (1.f + ys * invY2) / (1.f + ys);

					// Convert back to RGB color space
					p[i].r =  3.240479f * xyz.r - 1.537150f * xyz.g - 0.498535f * xyz.b;
					p[i].g = -0.969256f * xyz.r + 1.875991f * xyz.g + 0.041556f * xyz.b;
					p[i].b =  0.055648f * xyz.r - 0.204043f * xyz.g + 1.057311f * xyz.b;

					// Gamma correction
					p[i].r = Radiance2PixelFloat(p[i].r);
					p[i].g = Radiance2PixelFloat(p[i].g);
					p[i].b = Radiance2PixelFloat(p[i].b);
				}
			}
			break;
		}
		default:
			assert (false);
			break;
	}
}

void Film::AddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight)  {
	// Faster than HasChannel(channel_RADIANCE_PER_PIXEL_NORMALIZED)
	if (channel_RADIANCE_PER_PIXEL_NORMALIZED && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) &&
			!sampleResult.radiancePerPixelNormalized.IsNaN() && !sampleResult.radiancePerPixelNormalized.IsInf()) {
		float pixel[4];
		pixel[0] = sampleResult.radiancePerPixelNormalized.r * weight;
		pixel[1] = sampleResult.radiancePerPixelNormalized.g * weight;
		pixel[2] = sampleResult.radiancePerPixelNormalized.b * weight;
		pixel[3] = weight;
		channel_RADIANCE_PER_PIXEL_NORMALIZED->AddPixel(x, y, pixel);
	}

	// Faster than HasChannel(channel_RADIANCE_PER_SCREEN_NORMALIZED)
	if (channel_RADIANCE_PER_SCREEN_NORMALIZED && sampleResult.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED) &&
			!sampleResult.radiancePerScreenNormalized.IsNaN() && !sampleResult.radiancePerScreenNormalized.IsInf()) {
		float pixel[4];
		pixel[0] = sampleResult.radiancePerScreenNormalized.r * weight;
		pixel[1] = sampleResult.radiancePerScreenNormalized.g * weight;
		pixel[2] = sampleResult.radiancePerScreenNormalized.b * weight;
		pixel[3] = weight;
		channel_RADIANCE_PER_SCREEN_NORMALIZED->AddPixel(x, y, pixel);
	}

	// Faster than HasChannel(ALPHA)
	if (channel_ALPHA && sampleResult.HasChannel(ALPHA) && !isnan(sampleResult.alpha) && !isinf(sampleResult.alpha)) {
		float pixel[2];
		pixel[0] = weight * sampleResult.alpha;
		pixel[1] = weight;
		channel_ALPHA->AddPixel(x, y, pixel);
	}

	// Faster than HasChannel(DIRECT_DIFFUSE)
	if (channel_DIRECT_DIFFUSE && sampleResult.HasChannel(DIRECT_DIFFUSE) &&
			!sampleResult.directDiffuse.IsNaN() && !sampleResult.directDiffuse.IsInf()) {
		float pixel[4];
		pixel[0] = sampleResult.directDiffuse.r * weight;
		pixel[1] = sampleResult.directDiffuse.g * weight;
		pixel[2] = sampleResult.directDiffuse.b * weight;
		pixel[3] = weight;
		channel_DIRECT_DIFFUSE->AddPixel(x, y, pixel);
	}

	// Faster than HasChannel(DIRECT_GLOSSY)
	if (channel_DIRECT_GLOSSY && sampleResult.HasChannel(DIRECT_GLOSSY) &&
			!sampleResult.directGlossy.IsNaN() && !sampleResult.directGlossy.IsInf()) {
		float pixel[4];
		pixel[0] = sampleResult.directGlossy.r * weight;
		pixel[1] = sampleResult.directGlossy.g * weight;
		pixel[2] = sampleResult.directGlossy.b * weight;
		pixel[3] = weight;
		channel_DIRECT_GLOSSY->AddPixel(x, y, pixel);
	}
}

void Film::AddSampleResultNoColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight)  {
	bool depthWrite = true;

	// Faster than HasChannel(DEPTH)
	if (channel_DEPTH && sampleResult.HasChannel(DEPTH) && !isnan(sampleResult.depth))
		depthWrite = channel_DEPTH->MinPixel(x, y, &sampleResult.depth);

	// Faster than HasChannel(POSITION)
	if (channel_POSITION && depthWrite && sampleResult.HasChannel(POSITION) && !sampleResult.position.IsNaN())
		channel_POSITION->SetPixel(x, y, &sampleResult.position.x);

	// Faster than HasChannel(GEOMETRY_NORMAL)
	if (channel_GEOMETRY_NORMAL && depthWrite && sampleResult.HasChannel(GEOMETRY_NORMAL) && !sampleResult.geometryNormal.IsNaN())
		channel_GEOMETRY_NORMAL->SetPixel(x, y, &sampleResult.geometryNormal.x);

	// Faster than HasChannel(SHADING_NORMAL)
	if (channel_SHADING_NORMAL && depthWrite && sampleResult.HasChannel(SHADING_NORMAL) && !sampleResult.shadingNormal.IsNaN())
		channel_SHADING_NORMAL->SetPixel(x, y, &sampleResult.shadingNormal.x);

	// Faster than HasChannel(MATERIAL_ID)
	if (channel_MATERIAL_ID && depthWrite && sampleResult.HasChannel(MATERIAL_ID))
		channel_MATERIAL_ID->SetPixel(x, y, &sampleResult.materialID);
}

void Film::AddSampleResult(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight)  {
	AddSampleResultColor(x, y, sampleResult, weight);
	AddSampleResultNoColor(x, y, sampleResult, weight);
}

void Film::AddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight) {
	if ((x < 0) || (x >= width) || (y < 0) || (y >= height))
		return;

	AddSampleResult(x, y, sampleResult, weight);
}

void Film::SplatSample(const SampleResult &sampleResult, const float weight) {
	if (!filter) {
		const int x = Ceil2Int(sampleResult.filmX - .5f);
		const int y = Ceil2Int(sampleResult.filmY - .5f);

		if ((x >= 0.f) && (x < (int)width) && (y >= 0.f) && (y < (int)height))
			AddSampleResult(x, y, sampleResult, weight);
	} else {
		//----------------------------------------------------------------------
		// Add all no color related information (not filtered)
		//----------------------------------------------------------------------

		const int x = Ceil2Int(sampleResult.filmX - .5f);
		const int y = Ceil2Int(sampleResult.filmY - .5f);

		if ((x >= 0.f) && (x < (int)width) && (y >= 0.f) && (y < (int)height))
			AddSampleResultNoColor(x, y, sampleResult, weight);

		//----------------------------------------------------------------------
		// Add all color related information (filtered)
		//----------------------------------------------------------------------

		// Compute sample's raster extent
		const float dImageX = sampleResult.filmX - .5f;
		const float dImageY = sampleResult.filmY - .5f;
		const FilterLUT *filterLUT = filterLUTs->GetLUT(dImageX - floorf(sampleResult.filmX), dImageY - floorf(sampleResult.filmY));
		const float *lut = filterLUT->GetLUT();

		const int x0 = Ceil2Int(dImageX - filter->xWidth);
		const int x1 = x0 + filterLUT->GetWidth();
		const int y0 = Ceil2Int(dImageY - filter->yWidth);
		const int y1 = y0 + filterLUT->GetHeight();

		for (int iy = y0; iy < y1; ++iy) {
			if (iy < 0) {
				lut += filterLUT->GetWidth();
				continue;
			} else if(iy >= (int)height)
				break;

			for (int ix = x0; ix < x1; ++ix) {
				const float filterWeight = *lut++;

				if ((ix < 0) || (ix >= (int)width))
					continue;

				const float filteredWeight = weight * filterWeight;
				AddSampleResultColor(ix, iy, sampleResult, filteredWeight);
			}
		}
	}
}

void Film::ResetConvergenceTest() {
	if (convTest)
		convTest->Reset();
}

u_int Film::RunConvergenceTest() {
	return convTest->Test((const float *)channel_RGB_TONEMAPPED->GetPixels());
}
