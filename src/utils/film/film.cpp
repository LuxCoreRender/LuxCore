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

#include <boost/lexical_cast.hpp>

#include "luxrays/utils/film/film.h"

using namespace luxrays;
using namespace luxrays::utils;

Film::Film(const unsigned int w, const unsigned int h,
			const bool enablePerPixelNormBuffer,
			const bool enablePerScreenNormBuffer,
			const bool enableFrameBuf) {
	sampleFrameBuffer[PER_PIXEL_NORMALIZED] = NULL;
	sampleFrameBuffer[PER_SCREEN_NORMALIZED] = NULL;
	alphaFrameBuffer = NULL;
	frameBuffer = NULL;
	convTest = NULL;

	enableAlphaChannel = false;
	enabledOverlappedScreenBufferUpdate = true;
	enablePerPixelNormalizedBuffer = enablePerPixelNormBuffer;
	enablePerScreenNormalizedBuffer = enablePerScreenNormBuffer;
	enableFrameBuffer = enableFrameBuf;

	filter = NULL;
	filterLUTs = NULL;
	SetFilterType(FILTER_GAUSSIAN);

	toneMapParams = new LinearToneMapParams();

	InitGammaTable();
	Init(w, h);
}

Film::~Film() {
	delete toneMapParams;

	delete convTest;

	delete sampleFrameBuffer[PER_PIXEL_NORMALIZED];
	delete sampleFrameBuffer[PER_SCREEN_NORMALIZED];
	delete alphaFrameBuffer;
	delete frameBuffer;

	delete filterLUTs;
	delete filter;
}

void Film::Init(const unsigned int w, const unsigned int h) {
	width = w;
	height = h;
	pixelCount = w * h;

	delete sampleFrameBuffer[PER_PIXEL_NORMALIZED];
	delete sampleFrameBuffer[PER_SCREEN_NORMALIZED];
	delete alphaFrameBuffer;
	delete frameBuffer;

	if (enablePerPixelNormalizedBuffer) {
		sampleFrameBuffer[PER_PIXEL_NORMALIZED] = new SampleFrameBuffer(width, height);
		sampleFrameBuffer[PER_PIXEL_NORMALIZED]->Clear();
	}

	if (enablePerScreenNormalizedBuffer) {
		sampleFrameBuffer[PER_SCREEN_NORMALIZED] = new SampleFrameBuffer(width, height);
		sampleFrameBuffer[PER_SCREEN_NORMALIZED]->Clear();
	}

	if (enableAlphaChannel) {
		alphaFrameBuffer = new AlphaFrameBuffer(width, height);
		alphaFrameBuffer->Clear();
	}

	if (enableFrameBuffer) {
		frameBuffer = new FrameBuffer(width, height);
		frameBuffer->Clear();

		convTest = new ConvergenceTest(width, height);
	}

	statsTotalSampleCount = 0.0;
	statsAvgSampleSec = 0.0;
	statsStartSampleTime = WallClockTime();

}

void Film::InitGammaTable(const float gamma) {
	float x = 0.f;
	const float dx = 1.f / GAMMA_TABLE_SIZE;
	for (unsigned int i = 0; i < GAMMA_TABLE_SIZE; ++i, x += dx)
		gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / gamma);
}

void Film::SetFilterType(const FilterType type) {
	delete filterLUTs;
	delete filter;

	filterType = type;

	// Initialize Filter LUTs
	switch (type) {
		case FILTER_NONE:
			filter = NULL;
			filterLUTs = NULL;
			break;
		case FILTER_BOX:
			filter = new BoxFilter(1.5f, 1.5f);
			break;
		case FILTER_GAUSSIAN:
			filter = new GaussianFilter(1.5f, 1.5f, 2.f);
			break;
		case FILTER_MITCHELL:
			filter = new MitchellFilter(1.5f, 1.5f);
			break;
		case FILTER_MITCHELL_SS:
			filter = new MitchellFilterSS(1.5f, 1.5f);
			break;
		default:
			throw std::runtime_error("Unknown filter type: " + boost::lexical_cast<std::string>(type));
	}

	if (filter) {
		u_int size = Max<u_int>(4, Max(filter->xWidth, filter->yWidth) + 1);
		filterLUTs = new FilterLUTs(*filter, size);
	}
}

void Film::Reset() {
	if (enablePerPixelNormalizedBuffer)
		sampleFrameBuffer[PER_PIXEL_NORMALIZED]->Clear();
	if (enablePerScreenNormalizedBuffer)
		sampleFrameBuffer[PER_SCREEN_NORMALIZED]->Clear();
	if (enableAlphaChannel)
		alphaFrameBuffer->Clear();

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

	if (enablePerPixelNormalizedBuffer && film.enablePerPixelNormalizedBuffer) {
		SampleFrameBuffer *fbDst = sampleFrameBuffer[PER_PIXEL_NORMALIZED];
		const SampleFrameBuffer *fbSrc = film.sampleFrameBuffer[PER_PIXEL_NORMALIZED];

		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const SamplePixel *srcPixel = fbSrc->GetPixel(srcOffsetX + x, srcOffsetY + y);
				fbDst->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel->radiance, srcPixel->weight);
			}
		}
	}

	if (enablePerScreenNormalizedBuffer && film.enablePerScreenNormalizedBuffer) {
		SampleFrameBuffer *fbDst = sampleFrameBuffer[PER_SCREEN_NORMALIZED];
		const SampleFrameBuffer *fbSrc = film.sampleFrameBuffer[PER_SCREEN_NORMALIZED];

		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const SamplePixel *srcPixel = fbSrc->GetPixel(srcOffsetX + x, srcOffsetY + y);
				fbDst->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel->radiance, srcPixel->weight);
			}
		}
	}

	if (enableAlphaChannel && film.enableAlphaChannel) {
		AlphaFrameBuffer *fbDst = alphaFrameBuffer;
		const AlphaFrameBuffer *fbSrc = film.alphaFrameBuffer;

		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const AlphaPixel *srcPixel = fbSrc->GetPixel(srcOffsetX + x, srcOffsetY + y);
				fbDst->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel->alpha);
			}
		}
	}
}

void Film::SaveScreenBuffer(const std::string &fileName) {
	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(fileName.c_str());
	if (fif != FIF_UNKNOWN) {
		if ((fif == FIF_HDR) || (fif == FIF_EXR)) {
			// In order to merge the 2 sample buffers
			UpdateScreenBufferImpl(TONEMAP_NONE);

			if (alphaFrameBuffer) {
				// Save the alpha channel too
				FIBITMAP *dib = FreeImage_AllocateT(FIT_RGBAF, width, height, 128);

				if (dib) {
					unsigned int pitch = FreeImage_GetPitch(dib);
					BYTE *bits = (BYTE *)FreeImage_GetBits(dib);

					for (unsigned int y = 0; y < height; ++y) {
						FIRGBAF *pixel = (FIRGBAF *)bits;
						for (unsigned int x = 0; x < width; ++x) {
							const unsigned int ridx = y * width + x;

							pixel[x].red = frameBuffer->GetPixel(ridx)->r;
							pixel[x].green = frameBuffer->GetPixel(ridx)->g;
							pixel[x].blue = frameBuffer->GetPixel(ridx)->b;

							const SamplePixel *sp = sampleFrameBuffer[PER_PIXEL_NORMALIZED]->GetPixel(ridx);
							const float weight = sp->weight;
							if (weight == 0.f) {
								pixel[x].alpha = 0.f;
							} else {
								const float iw = 1.f / weight;
								const AlphaPixel *ap = alphaFrameBuffer->GetPixel(ridx);
								pixel[x].alpha = ap->alpha * iw;
							}
						}

						// Next line
						bits += pitch;
					}

					if (!FreeImage_Save(fif, dib, fileName.c_str(), 0))
						throw std::runtime_error("Failed image save");

					FreeImage_Unload(dib);
				} else
					throw std::runtime_error("Unable to allocate FreeImage HDR image");
			} else {
				// No alpha channel available
				FIBITMAP *dib = FreeImage_AllocateT(FIT_RGBF, width, height, 96);

				if (dib) {
					unsigned int pitch = FreeImage_GetPitch(dib);
					BYTE *bits = (BYTE *)FreeImage_GetBits(dib);

					for (unsigned int y = 0; y < height; ++y) {
						FIRGBF *pixel = (FIRGBF *)bits;
						for (unsigned int x = 0; x < width; ++x) {
							const unsigned int ridx = y * width + x;
							pixel[x].red = frameBuffer->GetPixel(ridx)->r;
							pixel[x].green = frameBuffer->GetPixel(ridx)->g;
							pixel[x].blue = frameBuffer->GetPixel(ridx)->b;
						}

						// Next line
						bits += pitch;
					}

					if (!FreeImage_Save(fif, dib, fileName.c_str(), 0))
						throw std::runtime_error("Failed image save");

					FreeImage_Unload(dib);
				} else
					throw std::runtime_error("Unable to allocate FreeImage HDR image");
			}

			// To restore the used tonemapping instead of NONE
			UpdateScreenBuffer();
		} else {
			UpdateScreenBuffer();

			if (alphaFrameBuffer) {
				// Save the alpha channel too
				FIBITMAP *dib = FreeImage_Allocate(width, height, 32);

				if (dib) {
					unsigned int pitch = FreeImage_GetPitch(dib);
					BYTE *bits = (BYTE *)FreeImage_GetBits(dib);
					const float *pixels = GetScreenBuffer();
					const AlphaPixel *alphaPixels = alphaFrameBuffer->GetPixels();

					for (unsigned int y = 0; y < height; ++y) {
						BYTE *pixel = (BYTE *)bits;
						for (unsigned int x = 0; x < width; ++x) {
							const int offset = 3 * (x + y * width);
							pixel[FI_RGBA_RED] = (BYTE)(pixels[offset] * 255.f + .5f);
							pixel[FI_RGBA_GREEN] = (BYTE)(pixels[offset + 1] * 255.f + .5f);
							pixel[FI_RGBA_BLUE] = (BYTE)(pixels[offset + 2] * 255.f + .5f);

							const SamplePixel *sp = sampleFrameBuffer[PER_PIXEL_NORMALIZED]->GetPixel(x, y);
							const float weight = sp->weight;

							if (weight == 0.f)
								pixel[FI_RGBA_ALPHA] = (BYTE)0;
							else {
								const int alphaOffset = (x + y * width);
								const float alpha = Clamp(
									alphaPixels[alphaOffset].alpha / weight,
									0.f, 1.f);
								pixel[FI_RGBA_ALPHA] = (BYTE)(alpha * 255.f + .5f);
							}

							pixel += 4;
						}

						// Next line
						bits += pitch;
					}

					if (!FreeImage_Save(fif, dib, fileName.c_str(), 0))
						throw std::runtime_error("Failed image save");

					FreeImage_Unload(dib);
				} else
					throw std::runtime_error("Unable to allocate FreeImage image");
			} else {
				// No alpha channel available
				FIBITMAP *dib = FreeImage_Allocate(width, height, 24);

				if (dib) {
					unsigned int pitch = FreeImage_GetPitch(dib);
					BYTE *bits = (BYTE *)FreeImage_GetBits(dib);
					const float *pixels = GetScreenBuffer();

					for (unsigned int y = 0; y < height; ++y) {
						BYTE *pixel = (BYTE *)bits;
						for (unsigned int x = 0; x < width; ++x) {
							const int offset = 3 * (x + y * width);
							pixel[FI_RGBA_RED] = (BYTE)(pixels[offset] * 255.f + .5f);
							pixel[FI_RGBA_GREEN] = (BYTE)(pixels[offset + 1] * 255.f + .5f);
							pixel[FI_RGBA_BLUE] = (BYTE)(pixels[offset + 2] * 255.f + .5f);
							pixel += 3;
						}

						// Next line
						bits += pitch;
					}

					if (!FreeImage_Save(fif, dib, fileName.c_str(), 0))
						throw std::runtime_error("Failed image save");

					FreeImage_Unload(dib);
				} else
					throw std::runtime_error("Unable to allocate FreeImage image");
			}
		}
	} else
		throw std::runtime_error("Image type unknown");
}

void Film::UpdateScreenBuffer() {
	UpdateScreenBufferImpl(toneMapParams->GetType());
}

void Film::MergeSampleBuffers(Pixel *p, std::vector<bool> &frameBufferMask) const {
	const unsigned int pixelCount = width * height;

	// Merge PER_PIXEL_NORMALIZED and PER_SCREEN_NORMALIZED buffers

	if (enablePerPixelNormalizedBuffer) {
		const SamplePixel *sp = sampleFrameBuffer[PER_PIXEL_NORMALIZED]->GetPixels();
		for (unsigned int i = 0; i < pixelCount; ++i) {
			const float weight = sp[i].weight;

			if (weight > 0.f) {
				p[i] = sp[i].radiance / weight;
				frameBufferMask[i] = true;
			}
		}
	}

	if (enablePerScreenNormalizedBuffer) {
		const SamplePixel *sp = sampleFrameBuffer[PER_SCREEN_NORMALIZED]->GetPixels();
		const float factor = pixelCount / statsTotalSampleCount;

		for (unsigned int i = 0; i < pixelCount; ++i) {
			const float weight = sp[i].weight;

			if (weight > 0.f) {
				if (frameBufferMask[i])
					p[i] += sp[i].radiance * factor;
				else
					p[i] = sp[i].radiance * factor;
				frameBufferMask[i] = true;
			}
		}
	}

	if (!enabledOverlappedScreenBufferUpdate) {
		for (unsigned int i = 0; i < pixelCount; ++i) {
			if (!frameBufferMask[i]) {
				p[i].r = 0.f;
				p[i].g = 0.f;
				p[i].b = 0.f;
			}
		}
	}
}

void Film::UpdateScreenBufferImpl(const ToneMapType type) {
	switch (type) {
		case TONEMAP_NONE: {
			Pixel *p = frameBuffer->GetPixels();
			std::vector<bool> frameBufferMask(pixelCount, false);

			MergeSampleBuffers(p, frameBufferMask);
			break;
		}
		case TONEMAP_LINEAR: {
			const LinearToneMapParams &tm = (LinearToneMapParams &)(*toneMapParams);
			Pixel *p = frameBuffer->GetPixels();
			const unsigned int pixelCount = width * height;
			std::vector<bool> frameBufferMask(pixelCount, false);

			MergeSampleBuffers(p, frameBufferMask);

			// Gamma correction
			for (unsigned int i = 0; i < pixelCount; ++i) {
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

			Pixel *p = frameBuffer->GetPixels();
			const unsigned int pixelCount = width * height;

			std::vector<bool> frameBufferMask(pixelCount, false);
			MergeSampleBuffers(p, frameBufferMask);

			// Use the frame buffer as temporary storage and calculate the average luminance
			float Ywa = 0.f;

			for (unsigned int i = 0; i < pixelCount; ++i) {
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

			for (unsigned int i = 0; i < pixelCount; ++i) {
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

void Film::SplatFiltered(const FilmBufferType type, const float screenX,
		const float screenY, const Spectrum &radiance, const float alpha,
		const float weight) {
	assert (!radiance.IsNaN() && !radiance.IsInf());

	if (radiance.IsNaN() || radiance.IsInf())
		return;

	if (filterType == FILTER_NONE) {
		const int x = Ceil2Int(screenX - 0.5f);
		const int y = Ceil2Int(screenY - 0.5f);

		if ((x >= 0.f) && (x < (int)width) && (y >= 0.f) && (y < (int)height)) {
			AddRadiance(type, x, y, radiance, weight);

			if (enableAlphaChannel)
				AddAlpha(x, y, alpha, weight);
		}
	} else {
		// Compute sample's raster extent
		const float dImageX = screenX - 0.5f;
		const float dImageY = screenY - 0.5f;
		const FilterLUT *filterLUT = filterLUTs->GetLUT(dImageX - floorf(screenX), dImageY - floorf(screenY));
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
				const float filterWt = *lut++;

				if ((ix < 0) || (ix >= (int)width))
					continue;

				const float filteredWeight = weight * filterWt;
				AddRadiance(type, ix, iy, radiance, filteredWeight);

				if (enableAlphaChannel)
					AddAlpha(ix, iy, alpha, filteredWeight);
			}
		}
	}
}

void Film::ResetConvergenceTest() {
	convTest->Reset();
}

unsigned int Film::RunConvergenceTest() {
	return convTest->Test((const float *)frameBuffer->GetPixels());
}

FilterType Filter::String2FilterType(const std::string &type) {
	if ((type.compare("0") == 0) || (type.compare("NONE") == 0))
		return FILTER_NONE;
	else if ((type.compare("1") == 0) || (type.compare("BOX") == 0))
		return FILTER_BOX;
	else if ((type.compare("2") == 0) || (type.compare("GAUSSIAN") == 0))
		return FILTER_GAUSSIAN;
	else if ((type.compare("3") == 0) || (type.compare("MITCHELL") == 0))
		return FILTER_MITCHELL;
	else if ((type.compare("4") == 0) || (type.compare("MITCHELL_SS") == 0))
		return FILTER_MITCHELL_SS;
	throw std::runtime_error("Unknown filter type: " + type);
}
