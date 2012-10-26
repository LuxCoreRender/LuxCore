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

//------------------------------------------------------------------------------
// Base class for all Film implementation
//------------------------------------------------------------------------------

#define GAMMA_TABLE_SIZE 1024

class Film {
public:
	Film(const unsigned int w, const unsigned int h, const bool perScreenNorm);
	~Film();

	void Init(const unsigned int w, const unsigned int h);
	void InitGammaTable(const float gamma = 2.2f);
	void Reset();

	void EnableAlphaChannel(const bool alphaChannel) {
		enableAlphaChannel = alphaChannel;
	}
	bool IsAlphaChannelEnabled() const { return enableAlphaChannel; }
	void EnableOverlappedScreenBufferUpdate(const bool overlappedScreenBufferUpdate) {
		enabledOverlappedScreenBufferUpdate = overlappedScreenBufferUpdate;
	}

	void SetFilterType(const FilterType filter) {
		filterType = filter;
	}
	FilterType GetFilterType() const { return filterType; }

	const ToneMapParams *GetToneMapParams() const { return toneMapParams; }
	void SetToneMapParams(const ToneMapParams &params) {
		delete toneMapParams;

		toneMapParams = params.Copy();
	}

	void AddFilm(const Film &film);

	void SaveScreenBuffer(const std::string &filmFile);
	void UpdateScreenBuffer();
	const float *GetScreenBuffer() const {
		return (const float *)frameBuffer->GetPixels();
	}

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }
	double GetTotalSampleCount() const { return statsTotalSampleCount; }
	double GetTotalTime() const {
		return WallClockTime() - statsStartSampleTime;
	}
	double GetAvgSampleSec() {
		/*const double elapsedTime = WallClockTime() - statsStartSampleTime;
		const double k = (elapsedTime < 10.0) ? 1.0 : (1.0 / (2.5 * elapsedTime));
		statsAvgSampleSec = k * statsTotalSampleCount / elapsedTime +
				(1.0 - k) * statsAvgSampleSec;

		return statsAvgSampleSec;*/

		return statsTotalSampleCount / GetTotalTime();
	}

	//--------------------------------------------------------------------------
	
	void ResetConvergenceTest();
	unsigned int RunConvergenceTest();
	
	//--------------------------------------------------------------------------

	void AddSampleCount(const double count) { statsTotalSampleCount += 1.f; }

	void AddRadiance(const unsigned int x, const unsigned int y, const Spectrum &radiance, const float weight) {
		const unsigned int offset = x + y * width;
		SamplePixel *sp = &(sampleFrameBuffer->GetPixels()[offset]);

		sp->radiance += radiance;
		sp->weight += weight;
	}
	
	void AddAlpha(const unsigned int x, const unsigned int y, const float alpha) {
		const unsigned int offset = x + y * width;
		AlphaPixel *ap = &(alphaFrameBuffer->GetPixels()[offset]);

		ap->alpha += alpha;
	}

	void SplatFiltered(const float screenX, const float screenY, const Spectrum &radiance);
	void SplatFilteredAlpha(const float screenX, const float screenY, const float a);

private:
	float Radiance2PixelFloat(const float x) const {
		// Very slow !
		//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

		const unsigned int index = Min<unsigned int>(
			Floor2UInt(GAMMA_TABLE_SIZE * Clamp(x, 0.f, 1.f)),
				GAMMA_TABLE_SIZE - 1);
		return gammaTable[index];
	}

	Spectrum Radiance2Pixel(const Spectrum& c) const {
		return Spectrum(Radiance2PixelFloat(c.r), Radiance2PixelFloat(c.g), Radiance2PixelFloat(c.b));
	}

	void SplatRadiance(const Spectrum radiance, const unsigned int x, const unsigned int y, const float weight) {
		const unsigned int offset = x + y * width;
		SamplePixel *sp = &(sampleFrameBuffer->GetPixels()[offset]);

		sp->radiance += weight * radiance;
		sp->weight += weight;
	}

	void SplatRadiance(const Spectrum radiance, const unsigned int x, const unsigned int y) {
		const unsigned int offset = x + y * width;
		SamplePixel *sp = &(sampleFrameBuffer->GetPixels()[offset]);

		sp->radiance += radiance;
		sp->weight += 1.f;
	}

	void SplatAlpha(const float alpha, const unsigned int x, const unsigned int y, const float weight) {
		const unsigned int offset = x + y * width;
		AlphaPixel *sp = &(alphaFrameBuffer->GetPixels()[offset]);

		sp->alpha += weight * alpha;
	}

	unsigned int width, height;
	unsigned int pixelCount;

	double statsTotalSampleCount, statsStartSampleTime, statsAvgSampleSec;

	float gammaTable[GAMMA_TABLE_SIZE];

	FilterType filterType;
	ToneMapParams *toneMapParams;

	SampleFrameBuffer *sampleFrameBuffer;
	AlphaFrameBuffer *alphaFrameBuffer;
	FrameBuffer *frameBuffer;

	Filter *filter;
	FilterLUTs *filterLUTs;
	
	ConvergenceTest convTest;

	bool enableAlphaChannel, usePerScreenNormalization,
		enabledOverlappedScreenBufferUpdate;
};

} }

#endif	/* _LUXRAYS_FILM_FILM_H */
