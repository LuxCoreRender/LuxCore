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

#ifndef _LUXRAYS_UTILS_FILM_H
#define	_LUXRAYS_UTILS_FILM_H

#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

#if defined (WIN32)
#include <windows.h>
#endif

#include <FreeImage.h>

#include <boost/thread/mutex.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/utils/film/filter.h"
#include "luxrays/utils/film/tonemapping.h"
#include "luxrays/utils/film/framebuffer.h"
#include "luxrays/utils/convtest/convtest.h"

namespace luxrays { namespace utils {

enum FilmBufferType {
	PER_PIXEL_NORMALIZED = 0,
	PER_SCREEN_NORMALIZED = 1
};

//------------------------------------------------------------------------------
// Base class for all Film implementation
//------------------------------------------------------------------------------

#define GAMMA_TABLE_SIZE 1024

class Film {
public:
	Film(const unsigned int w, const unsigned int h,
			const bool enablePerPixelNormalizedBuffer,
			const bool enablePerScreenNormalizedBuffer,
			const bool enableFrameBuffer);
	~Film();

	void Init(const unsigned int w, const unsigned int h);
	void InitGammaTable(const float gamma = 2.2f);
	void Reset();

	//--------------------------------------------------------------------------
	// Dynamic settings
	//--------------------------------------------------------------------------

	void EnableAlphaChannel(const bool alphaChannel) {
		// Alpha buffer uses the weights in PER_PIXEL_NORMALIZED buffer
		assert (enablePerPixelNormalizedBuffer);

		enableAlphaChannel = alphaChannel;
	}
	bool IsAlphaChannelEnabled() const { return enableAlphaChannel; }

	void EnableOverlappedScreenBufferUpdate(const bool overlappedScreenBufferUpdate) {
		enabledOverlappedScreenBufferUpdate = overlappedScreenBufferUpdate;
	}
	bool IsOverlappedScreenBufferUpdate() const { return enabledOverlappedScreenBufferUpdate; }

	void SetFilterType(const FilterType filter);
	FilterType GetFilterType() const { return filterType; }

	const ToneMapParams *GetToneMapParams() const { return toneMapParams; }
	void SetToneMapParams(const ToneMapParams &params) {
		delete toneMapParams;

		toneMapParams = params.Copy();
	}

	void CopyDynamicSettings(const Film &film) {
		EnableAlphaChannel(film.IsAlphaChannelEnabled());
		SetFilterType(film.GetFilterType());
		SetToneMapParams(*(film.GetToneMapParams()));
		EnableOverlappedScreenBufferUpdate(film.IsOverlappedScreenBufferUpdate());
	}

	//--------------------------------------------------------------------------

	void AddFilm(const Film &film);

	void SaveScreenBuffer(const std::string &filmFile);
	void UpdateScreenBuffer();
	const float *GetScreenBuffer() const {
		return (const float *)frameBuffer->GetPixels();
	}

	//--------------------------------------------------------------------------

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }
	double GetTotalSampleCount() const {
		return statsTotalSampleCount;
	}
	double GetTotalTime() const {
		return WallClockTime() - statsStartSampleTime;
	}
	double GetAvgSampleSec() {
		return GetTotalSampleCount() / GetTotalTime();
	}

	//--------------------------------------------------------------------------

	void ResetConvergenceTest();
	unsigned int RunConvergenceTest();

	//--------------------------------------------------------------------------

	void AddSampleCount(const double count) {
		statsTotalSampleCount += count;
	}

	void AddRadiance(const FilmBufferType type, const unsigned int x, const unsigned int y,
		const Spectrum &radiance, const float weight) {
		SamplePixel *sp = sampleFrameBuffer[type]->GetPixel(x, y);

		sp->radiance += weight * radiance;
		sp->weight += weight;
	}

	void AddAlpha(const unsigned int x, const unsigned int y, const float alpha,
		const float weight) {
		AlphaPixel *ap = alphaFrameBuffer->GetPixel(x, y);

		ap->alpha += weight * alpha;
	}

	void SplatFiltered(const FilmBufferType type, const float screenX,
		const float screenY, const Spectrum &radiance, const float weight = 1.f);
	void SplatFilteredAlpha(const float screenX, const float screenY,
		const float a, const float weight = 1.f);

private:
	void UpdateScreenBufferImpl(const ToneMapType type);
	void MergeSampleBuffers(Pixel *p, vector<bool> &frameBufferMask) const;

	float Radiance2PixelFloat(const float x) const {
		// Very slow !
		//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

		const unsigned int index = Min<unsigned int>(
			Floor2UInt(GAMMA_TABLE_SIZE * Clamp(x, 0.f, 1.f)),
				GAMMA_TABLE_SIZE - 1);
		return gammaTable[index];
	}

	Pixel Radiance2Pixel(const Spectrum& c) const {
		return Spectrum(
				Radiance2PixelFloat(c.r),
				Radiance2PixelFloat(c.g),
				Radiance2PixelFloat(c.b));
	}

	unsigned int width, height, pixelCount;

	double statsTotalSampleCount, statsStartSampleTime, statsAvgSampleSec;

	float gammaTable[GAMMA_TABLE_SIZE];

	FilterType filterType;
	ToneMapParams *toneMapParams;

	// Two sample buffers, one PER_PIXEL_NORMALIZED and the other PER_SCREEN_NORMALIZED
	SampleFrameBuffer *sampleFrameBuffer[2];
	AlphaFrameBuffer *alphaFrameBuffer;
	FrameBuffer *frameBuffer;

	ConvergenceTest *convTest;

	Filter *filter;
	FilterLUTs *filterLUTs;

	bool enableAlphaChannel, enabledOverlappedScreenBufferUpdate,
		enablePerPixelNormalizedBuffer, enablePerScreenNormalizedBuffer,
		enableFrameBuffer;
};

} }

#endif	/* _LUXRAYS_FILM_FILM_H */
