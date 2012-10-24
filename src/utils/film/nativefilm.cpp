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

#include "luxrays/utils/film/nativefilm.h"

using namespace luxrays;
using namespace luxrays::utils;

NativeFilm::NativeFilm(const unsigned int w, const unsigned int h, const bool perScreenNorm) :
	Film(w, h, perScreenNorm), convTest(w, h) {
	sampleFrameBuffer = NULL;
	alphaFrameBuffer = NULL;
	frameBuffer = NULL;

	enableAlphaChannel = false;

	// Initialize Filter LUTs
	filter = new GaussianFilter(1.5f, 1.5f, 2.f);
	filterLUTs = new FilterLUTs(*filter, 4);
}

NativeFilm::~NativeFilm() {
	delete sampleFrameBuffer;
	delete alphaFrameBuffer;
	delete frameBuffer;

	delete filterLUTs;
	delete filter;
}

void NativeFilm::Init(const unsigned int w, const unsigned int h) {
	Film::Init(w, h);

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
}

void NativeFilm::Reset() {
	sampleFrameBuffer->Clear();
	if (enableAlphaChannel)
		alphaFrameBuffer->Clear();
	Film::Reset();

	// convTest has to be reseted explicitely
}

void NativeFilm::AddFilm(const NativeFilm &film) {
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

void NativeFilm::UpdateScreenBuffer() {
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

const float *NativeFilm::GetScreenBuffer() const {
	return (const float *)frameBuffer->GetPixels();
}

void NativeFilm::SplatFiltered(const float screenX, const float screenY, const Spectrum &radiance) {
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

void NativeFilm::SplatFilteredAlpha(const float screenX, const float screenY,
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

void NativeFilm::Save(const std::string &fileName) {
	UpdateScreenBuffer();
	Film::SaveImpl(fileName);
}

void NativeFilm::AddSampleFrameBuffer(const SampleFrameBuffer *sfb) {
	const unsigned int pixelCount = width * height;

	for (unsigned int i = 0; i < pixelCount; ++i) {
		SamplePixel *sp = sfb->GetPixel(i);
		SamplePixel *spbase = sampleFrameBuffer->GetPixel(i);

		spbase->radiance += sp->radiance;
		spbase->weight += sp->weight;
	}
}

void NativeFilm::ResetConvergenceTest() {
	convTest.Reset();
}

unsigned int NativeFilm::RunConvergenceTest() {
	return convTest.Test((const float *)frameBuffer->GetPixels());
}
