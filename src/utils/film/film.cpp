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

#include "luxrays/utils/film/film.h"

using namespace luxrays;
using namespace luxrays::utils;

Film::Film(const unsigned int w, const unsigned int h, const bool perScreenNorm) : convTest(w, h) {
	filterType = FILTER_GAUSSIAN;
	toneMapParams = new LinearToneMapParams();
	usePerScreenNormalization = perScreenNorm;

	sampleFrameBuffer = NULL;
	alphaFrameBuffer = NULL;
	frameBuffer = NULL;

	enableAlphaChannel = false;

	InitGammaTable();
	Init(w, h);

	// Initialize Filter LUTs
	filter = new GaussianFilter(1.5f, 1.5f, 2.f);
	filterLUTs = new FilterLUTs(*filter, 4);
}

Film::~Film() {
	delete toneMapParams;

	delete sampleFrameBuffer;
	delete alphaFrameBuffer;
	delete frameBuffer;

	delete filterLUTs;
	delete filter;
}

void Film::Init(const unsigned int w, const unsigned int h) {
	width = w;
	height = h;
	pixelCount = w * h;

	delete sampleFrameBuffer;
	delete alphaFrameBuffer;
	delete frameBuffer;

	sampleFrameBuffer = new SampleFrameBuffer(width, height);
	sampleFrameBuffer->Clear();

	if (enableAlphaChannel) {
		alphaFrameBuffer = new AlphaFrameBuffer(width, height);
		alphaFrameBuffer->Clear();
	}

	frameBuffer = new FrameBuffer(width, height);
	frameBuffer->Clear();

	convTest.Reset(width, height);

	statsTotalSampleCount = 0;
	statsAvgSampleSec = 0.0;
	statsStartSampleTime = WallClockTime();

}

void Film::InitGammaTable(const float gamma) {
	float x = 0.f;
	const float dx = 1.f / GAMMA_TABLE_SIZE;
	for (unsigned int i = 0; i < GAMMA_TABLE_SIZE; ++i, x += dx)
		gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / gamma);
}

void Film::Reset() {
	sampleFrameBuffer->Clear();
	if (enableAlphaChannel)
		alphaFrameBuffer->Clear();

	// convTest has to be reseted explicitely

	statsTotalSampleCount = 0;
	statsAvgSampleSec = 0.0;
	statsStartSampleTime = WallClockTime();
}

void Film::AddFilm(const Film &film) {
	// TODO: add alpha buffer support
	SamplePixel *spDst = sampleFrameBuffer->GetPixels();
	SamplePixel *spSrc = film.sampleFrameBuffer->GetPixels();

	if (usePerScreenNormalization) {
		if (film.usePerScreenNormalization) {
			// TODO
		} else {
			// TODO
		}
	} else {
		if (film.usePerScreenNormalization) {
			statsTotalSampleCount += film.statsTotalSampleCount;
			for (unsigned int i = 0; i < pixelCount; ++i) {
				spDst[i].radiance += spSrc[i].radiance;
				spDst[i].weight += film.statsTotalSampleCount;
			}
		} else {
			for (unsigned int i = 0; i < pixelCount; ++i) {
				spDst[i].radiance += spSrc[i].radiance;
				spDst[i].weight += spSrc[i].weight;
			}
		}
	}
}

void Film::SaveScreenBuffer(const std::string &fileName) {
	UpdateScreenBuffer();

	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(fileName.c_str());
	if (fif != FIF_UNKNOWN) {
		if ((fif == FIF_HDR) || (fif == FIF_EXR)) {
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

							const SamplePixel *sp = sampleFrameBuffer->GetPixel(ridx);
							const float weight = sp->weight;
							if (weight == 0.f) {
								pixel[x].red = 0.f;
								pixel[x].green = 0.f;
								pixel[x].blue = 0.f;
								pixel[x].alpha = 0.f;
							} else {
								const float iw = 1.f / weight;
								pixel[x].red = sp->radiance.r * iw;
								pixel[x].green =  sp->radiance.g * iw;
								pixel[x].blue =  sp->radiance.b * iw;

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
							const SamplePixel *sp = sampleFrameBuffer->GetPixel(x, y);
							const float weight = sp->weight;

							if (weight == 0.f) {
								pixel[x].red = 0.f;
								pixel[x].green = 0.f;
								pixel[x].blue = 0.f;
							} else {
								pixel[x].red = sp->radiance.r / weight;
								pixel[x].green =  sp->radiance.g / weight;
								pixel[x].blue =  sp->radiance.b / weight;
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
			}
		} else {
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

							const SamplePixel *sp = sampleFrameBuffer->GetPixel(x, y);
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
	switch (toneMapParams->GetType()) {
		case TONEMAP_LINEAR: {
			const LinearToneMapParams &tm = (LinearToneMapParams &)(*toneMapParams);
			const SamplePixel *sp = sampleFrameBuffer->GetPixels();
			Pixel *p = frameBuffer->GetPixels();
			const unsigned int pixelCount = width * height;
			const float perScreenNormalizationFactor = tm.scale / (float)statsTotalSampleCount;

			for (unsigned int i = 0; i < pixelCount; ++i) {
				const float weight = sp[i].weight;

				if (weight > 0.f) {
					if (usePerScreenNormalization) {
						p[i].r = Radiance2PixelFloat(sp[i].radiance.r * perScreenNormalizationFactor);
						p[i].g = Radiance2PixelFloat(sp[i].radiance.g * perScreenNormalizationFactor);
						p[i].b = Radiance2PixelFloat(sp[i].radiance.b * perScreenNormalizationFactor);						
					} else {
						const float invWeight = tm.scale / weight;

						p[i].r = Radiance2PixelFloat(sp[i].radiance.r * invWeight);
						p[i].g = Radiance2PixelFloat(sp[i].radiance.g * invWeight);
						p[i].b = Radiance2PixelFloat(sp[i].radiance.b * invWeight);
					}
				} else {
					p[i].r = 0.f;
					p[i].g = 0.f;
					p[i].b = 0.f;
				}
			}
			break;
		}
		case TONEMAP_REINHARD02: {
			const Reinhard02ToneMapParams &tm = (Reinhard02ToneMapParams &)(*toneMapParams);

			const float alpha = .1f;
			const float preScale = tm.preScale;
			const float postScale = tm.postScale;
			const float burn = tm.burn;

			const SamplePixel *sp = sampleFrameBuffer->GetPixels();
			Pixel *p = frameBuffer->GetPixels();
			const unsigned int pixelCount = width * height;
			const float perScreenNormalizationFactor = 1.f / (float)statsTotalSampleCount;

			// Use the frame buffer as temporary storage and calculate the average luminance
			float Ywa = 0.f;

			for (unsigned int i = 0; i < pixelCount; ++i) {
				const float weight = sp[i].weight;
				Spectrum rgb = sp[i].radiance;

				if ((weight > 0.f) && !rgb.IsNaN()) {
					if (usePerScreenNormalization)
						rgb *= perScreenNormalizationFactor;
					else
						rgb /= weight;

					// Convert to XYZ color space
					p[i].r = 0.412453f * rgb.r + 0.357580f * rgb.g + 0.180423f * rgb.b;
					p[i].g = 0.212671f * rgb.r + 0.715160f * rgb.g + 0.072169f * rgb.b;
					p[i].b = 0.019334f * rgb.r + 0.119193f * rgb.g + 0.950227f * rgb.b;

					Ywa += p[i].g;
				} else {
					p[i].r = 0.f;
					p[i].g = 0.f;
					p[i].b = 0.f;
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
			break;
		}
		default:
			assert (false);
			break;
	}
}

void Film::SplatFiltered(const float screenX, const float screenY, const Spectrum &radiance) {
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
		} else if(iy >= int(height))
			break;

		for (int ix = x0; ix < x1; ++ix) {
			const float filterWt = *lut++;

			if ((ix < 0) || (ix >= int(width)))
				continue;

			SplatRadiance(radiance, ix, iy, filterWt);
		}
	}
}

void Film::SplatFilteredAlpha(const float screenX, const float screenY,
		const float alpha) {
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
		} else if(iy >= int(height))
			break;

		for (int ix = x0; ix < x1; ++ix) {
			const float filterWt = *lut++;

			if ((ix < 0) || (ix >= int(width)))
				continue;

			SplatAlpha(alpha, ix, iy, filterWt);
		}
	}
}

void Film::ResetConvergenceTest() {
	convTest.Reset();
}

unsigned int Film::RunConvergenceTest() {
	return convTest.Test((const float *)frameBuffer->GetPixels());
}
