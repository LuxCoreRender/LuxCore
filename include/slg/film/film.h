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
#include <set>

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

class SampleResult {
public:
	SampleResult() { }
	SampleResult(const float x, const float y,
		const luxrays::Spectrum *radiancePPN,
		const luxrays::Spectrum *radiancePSN,
		const float a) {
		Init(x, y, radiancePPN, radiancePSN, a);
	}
	~SampleResult() { }

	void Init(const float x, const float y,
		const luxrays::Spectrum *radiancePPN,
		const luxrays::Spectrum *radiancePSN,
		const float a) {
		filmX = x;
		filmY = y;

		hasPerPixelNormalizedRadiance = (radiancePPN != NULL);
		if (hasPerPixelNormalizedRadiance)
			radiancePerPixelNormalized = *radiancePPN;

		hasPerScreenNormalizedRadiance = (radiancePSN != NULL);
		if (hasPerScreenNormalizedRadiance)
			radiancePerScreenNormalized = *radiancePSN;

		alpha = a;
	}

	float filmX, filmY;
	luxrays::Spectrum radiancePerPixelNormalized, radiancePerScreenNormalized;
	float alpha;
	bool hasPerPixelNormalizedRadiance, hasPerScreenNormalizedRadiance;
};

inline void AddSampleResult(std::vector<SampleResult> &sampleResults,
	const float filmX, const float filmY,
	const luxrays::Spectrum *radiancePPN,
	const luxrays::Spectrum *radiancePSN,
	const float alpha) {
	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);
	sampleResults[size].Init(filmX, filmY, radiancePPN, radiancePSN, alpha);
}

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

typedef enum {
	RGB_PER_PIXEL_NORMALIZED = 0,
	RGB_PER_SCREEN_NORMALIZED = 1,
	ALPHA = 2,
	TONEMAPPED_FRAMEBUFFER = 3
} FilmChannelType;

#define GAMMA_TABLE_SIZE 1024

class Film {
public:
	Film(const u_int w, const u_int h);
	~Film();

	// This one must be called before Init()
	void AddChannel(const FilmChannelType type);
	// This one must be called before Init()
	void RemoveChannel(const FilmChannelType type);
	bool HasChannel(const FilmChannelType type) const { return channels.count(type) > 0; }

	void Init() { Init(width, height); }
	void Init(const u_int w, const u_int h);
	void SetGamma(const float gamma = 2.2f);
	void Reset();

	//--------------------------------------------------------------------------
	// Dynamic settings
	//--------------------------------------------------------------------------

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
		channels = film.channels;
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
		return channel_RGB_TONEMAPPED->GetPixels();
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

	//--------------------------------------------------------------------------

	void ResetConvergenceTest();
	u_int RunConvergenceTest();

	//--------------------------------------------------------------------------

	void AddSampleCount(const double count) {
		statsTotalSampleCount += count;
	}

	void AddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight = 1.f);
	void SplatSample(const SampleResult &sampleResult, const float weight = 1.f);

private:
	void UpdateScreenBufferImpl(const ToneMapType type);
	void MergeSampleBuffers(luxrays::Spectrum *p, std::vector<bool> &frameBufferMask) const;

	float Radiance2PixelFloat(const float x) const {
		// Very slow !
		//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

		const u_int index = luxrays::Clamp(luxrays::Floor2Int(GAMMA_TABLE_SIZE * x), 0, GAMMA_TABLE_SIZE - 1);
		return gammaTable[index];
	}

	luxrays::Spectrum Radiance2Pixel(const luxrays::Spectrum &c) const {
		return luxrays::Spectrum(
				Radiance2PixelFloat(c.r),
				Radiance2PixelFloat(c.g),
				Radiance2PixelFloat(c.b));
	}

	void AddSampleResult(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight);

	std::set<FilmChannelType> channels;
	u_int width, height, pixelCount;
	GenericFrameBuffer<4, float> *channel_RGB_PER_PIXEL_NORMALIZED;
	GenericFrameBuffer<4, float> *channel_RGB_PER_SCREEN_NORMALIZED;
	GenericFrameBuffer<1, float> *channel_ALPHA;
	GenericFrameBuffer<3, float> *channel_RGB_TONEMAPPED;

	double statsTotalSampleCount, statsStartSampleTime, statsAvgSampleSec;

	float gamma;
	float gammaTable[GAMMA_TABLE_SIZE];

	ToneMapParams *toneMapParams;

	ConvergenceTest *convTest;

	Filter *filter;
	FilterLUTs *filterLUTs;

	bool initialized, enabledOverlappedScreenBufferUpdate;
};

}

#endif	/* _SLG_FILM_H */
