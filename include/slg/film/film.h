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

#ifndef _SLG_FILM_H
#define	_SLG_FILM_H

#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

#if defined (WIN32)
#include <windows.h>
#endif

#include <FreeImage.h>

#include <boost/thread/mutex.hpp>

#include "slg/slg.h"
#include "slg/film/filter.h"
#include "slg/film/tonemapping.h"
#include "slg/film/framebuffer.h"
#include "slg/utils/convtest/convtest.h"

namespace slg {

typedef enum {
	PER_PIXEL_NORMALIZED = 0,
	PER_SCREEN_NORMALIZED = 1
} FilmBufferType;

//------------------------------------------------------------------------------
// Base class for all Film implementation
//------------------------------------------------------------------------------

#define GAMMA_TABLE_SIZE 1024

class Film {
public:
	Film(const u_int w, const u_int h);
	~Film();

	void Init() { Init(width, height); }
	void Init(const u_int w, const u_int h);
	void InitGammaTable(const float gamma = 2.2f);
	void Reset();

	//--------------------------------------------------------------------------
	// Dynamic settings
	//--------------------------------------------------------------------------

	void SetPerPixelNormalizedBufferFlag(const bool enablePerPixelNormBuffer) {
		enablePerPixelNormalizedBuffer = enablePerPixelNormBuffer;
	}
	void SetPerScreenNormalizedBufferFlag(const bool enablePerScreenNormBuffer) {
		enablePerScreenNormalizedBuffer = enablePerScreenNormBuffer;
	}
	void SetFrameBufferFlag(const bool enableFrmBuffer) {
		enableFrameBuffer = enableFrmBuffer;
	}

	bool HasPerPixelNormalizedBuffer() const { return enablePerPixelNormalizedBuffer; }
	bool HasPerScreenNormalizedBuffer() const { return enablePerScreenNormalizedBuffer; }
	bool HasFrameBuffer() const { return enableFrameBuffer; }

	void SetAlphaChannelFlag(const bool alphaChannel) {
		// Alpha buffer uses the weights in PER_PIXEL_NORMALIZED buffer
		assert (!alphaChannel || (alphaChannel && enablePerPixelNormalizedBuffer));

		enableAlphaChannel = alphaChannel;
	}
	bool IsAlphaChannelEnabled() const { return enableAlphaChannel; }

	void SetOverlappedScreenBufferUpdateFlag(const bool overlappedScreenBufferUpdate) {
		enabledOverlappedScreenBufferUpdate = overlappedScreenBufferUpdate;
	}
	bool IsOverlappedScreenBufferUpdate() const { return enabledOverlappedScreenBufferUpdate; }

	void SetFilter(Filter *flt);
	const Filter *GetFilter() const { return filter; }

	const ToneMapParams *GetToneMapParams() const { return toneMapParams; }
	void SetToneMapParams(const ToneMapParams &params) {
		delete toneMapParams;

		toneMapParams = params.Copy();
	}

	void CopyDynamicSettings(const Film &film) {
		SetPerPixelNormalizedBufferFlag(film.HasPerPixelNormalizedBuffer());
		SetPerScreenNormalizedBufferFlag(film.HasPerScreenNormalizedBuffer());
		SetFrameBufferFlag(film.HasFrameBuffer());
		SetAlphaChannelFlag(film.IsAlphaChannelEnabled());
		SetFilter(film.GetFilter() ? film.GetFilter()->Clone() : NULL);
		SetToneMapParams(*(film.GetToneMapParams()));
		SetOverlappedScreenBufferUpdateFlag(film.IsOverlappedScreenBufferUpdate());
	}

	//--------------------------------------------------------------------------

	void AddFilm(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY);
	void AddFilm(const Film &film) {
		AddFilm(film, 0, 0, width, height, 0, 0);
	}

	void SaveScreenBuffer(const std::string &filmFile);
	void UpdateScreenBuffer();
	float *GetScreenBuffer() const {
		return (float *)frameBuffer->GetPixels();
	}

	//--------------------------------------------------------------------------

	float GetGamma() const { return gamma; }
	u_int GetWidth() const { return width; }
	u_int GetHeight() const { return height; }
	double GetTotalSampleCount() const {
		return statsTotalSampleCount;
	}
	double GetTotalTime() const {
		return luxrays::WallClockTime() - statsStartSampleTime;
	}
	double GetAvgSampleSec() {
		return GetTotalSampleCount() / GetTotalTime();
	}

	const SamplePixel *GetSamplePixel(const FilmBufferType type,
		const u_int x, const u_int y) const {
		return sampleFrameBuffer[type]->GetPixel(x, y);
	}

	const AlphaPixel *GetAlphaPixel(const u_int x, const u_int y) const {
		return alphaFrameBuffer->GetPixel(x, y);
	}

	//--------------------------------------------------------------------------

	void ResetConvergenceTest();
	u_int RunConvergenceTest();

	//--------------------------------------------------------------------------

	void AddSampleCount(const double count) {
		statsTotalSampleCount += count;
	}

	void SetPixel(const FilmBufferType type,
		const u_int x, const u_int y,
		const luxrays::Spectrum &radiance, const float alpha,
		const float weight = 1.f);

	void AddSample(const FilmBufferType type,
		const u_int x, const u_int y,
		const float u0, const float u1,
		const luxrays::Spectrum &radiance, const float alpha,
		const float weight = 1.f);

	void SplatSample(const FilmBufferType type, const float filmX,
		const float filmY, const luxrays::Spectrum &radiance, const float alpha,
		const float weight = 1.f);

private:
	void UpdateScreenBufferImpl(const ToneMapType type);
	void MergeSampleBuffers(Pixel *p, std::vector<bool> &frameBufferMask) const;

	float Radiance2PixelFloat(const float x) const {
		// Very slow !
		//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

		const u_int index = luxrays::Clamp(luxrays::Floor2Int(GAMMA_TABLE_SIZE * x), 0, GAMMA_TABLE_SIZE - 1);
		return gammaTable[index];
	}

	Pixel Radiance2Pixel(const luxrays::Spectrum& c) const {
		return luxrays::Spectrum(
				Radiance2PixelFloat(c.r),
				Radiance2PixelFloat(c.g),
				Radiance2PixelFloat(c.b));
	}

	void SetRadiance(const FilmBufferType type, const u_int x, const u_int y,
		const luxrays::Spectrum &radiance, const float weight) {
		SamplePixel *sp = sampleFrameBuffer[type]->GetPixel(x, y);

		sp->radiance = weight * radiance;
		sp->weight = weight;
	}

	void AddRadiance(const FilmBufferType type, const u_int x, const u_int y,
		const luxrays::Spectrum &radiance, const float weight) {
		SamplePixel *sp = sampleFrameBuffer[type]->GetPixel(x, y);

		sp->radiance += weight * radiance;
		sp->weight += weight;
	}

	void SetAlpha(const u_int x, const u_int y, const float alpha) {
		alphaFrameBuffer->SetPixel(x, y, alpha);
	}

	void AddAlpha(const u_int x, const u_int y, const float alpha,
		const float weight) {
		AlphaPixel *ap = alphaFrameBuffer->GetPixel(x, y);

		ap->alpha += weight * alpha;
	}

	u_int width, height, pixelCount;

	double statsTotalSampleCount, statsStartSampleTime, statsAvgSampleSec;

	float gamma;
	float gammaTable[GAMMA_TABLE_SIZE];

	ToneMapParams *toneMapParams;

	// Two sample buffers, one PER_PIXEL_NORMALIZED and the other PER_SCREEN_NORMALIZED
	SampleFrameBuffer *sampleFrameBuffer[2];
	AlphaFrameBuffer *alphaFrameBuffer;
	FrameBuffer *frameBuffer;

	ConvergenceTest *convTest;

	Filter *filter;
	PrecomputedFilter *precompFilter;
	FilterLUTs *filterLUTs;

	bool enableAlphaChannel, enabledOverlappedScreenBufferUpdate,
		enablePerPixelNormalizedBuffer, enablePerScreenNormalizedBuffer,
		enableFrameBuffer;
};

}

#endif	/* _SLG_FILM_H */
