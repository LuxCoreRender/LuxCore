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
#include "luxrays/core/pixel/samplebuffer.h"
#include "luxrays/core/pixeldevice.h"
#include "luxrays/core/pixel/samplebuffer.h"

namespace luxrays { namespace utils {

//------------------------------------------------------------------------------
// Base class for all Film implementation
//------------------------------------------------------------------------------

#define GAMMA_TABLE_SIZE 1024

class Film {
public:
	Film(const unsigned int w, const unsigned int h);
	virtual ~Film();

	virtual void Init(const unsigned int w, const unsigned int h);

	virtual void InitGammaTable(const float gamma = 2.2f);

	void SetFilterType(const FilterType filter) {
		filterType = filter;
	}
	FilterType GetFilterType() const { return filterType; }

	const ToneMapParams *GetToneMapParams() const { return toneMapParams; }
	void SetToneMapParams(const ToneMapParams &params) {
		delete toneMapParams;

		toneMapParams = params.Copy();
	}

	void AddFilm(const std::string &filmFile);
	void SaveFilm(const std::string &filmFile);

	void StartSampleTime() {
		statsStartSampleTime = WallClockTime();
	}

	virtual void Reset() {
		statsTotalSampleCount = 0;
		statsAvgSampleSec = 0.0;
		statsStartSampleTime = WallClockTime();
	}

	virtual void UpdateScreenBuffer() = 0;
	virtual const float *GetScreenBuffer() const = 0;

	virtual SampleBuffer *GetFreeSampleBuffer() = 0;
	virtual void FreeSampleBuffer(SampleBuffer *sampleBuffer) = 0;
	virtual void SplatSampleBuffer(const bool preview, SampleBuffer *sampleBuffer) {
		// Update statistics
		statsTotalSampleCount += (unsigned int)sampleBuffer->GetSampleCount();
	}

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }
	unsigned int GetTotalSampleCount() const { return statsTotalSampleCount; }
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

	virtual void Save(const std::string &fileName) = 0;

protected:
	void SaveImpl(const std::string &fileName);

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

	virtual const SampleFrameBuffer *GetSampleFrameBuffer() = 0;
	virtual void AddSampleFrameBuffer(const SampleFrameBuffer *sfb) = 0;

	// This can return NULL if alpha channel is not supported
	virtual const AlphaFrameBuffer *GetAlphaFrameBuffer() {
		return NULL;
	}

	unsigned int width, height;
	unsigned int pixelCount;

	unsigned int statsTotalSampleCount;
	double statsStartSampleTime, statsAvgSampleSec;

	float gammaTable[GAMMA_TABLE_SIZE];

	FilterType filterType;
	ToneMapParams *toneMapParams;
};

} }

#endif	/* _LUXRAYS_FILM_FILM_H */
