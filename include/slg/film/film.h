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

#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/normal.h"
#include "slg/slg.h"
#include "slg/film/filter.h"
#include "slg/film/tonemapping.h"
#include "slg/film/framebuffer.h"
#include "slg/utils/convtest/convtest.h"

namespace slg {

//------------------------------------------------------------------------------
// FilmOutput
//------------------------------------------------------------------------------

class FilmOutputs {
public:
	typedef enum {
		RGB, RGBA, RGB_TONEMAPPED, RGBA_TONEMAPPED, ALPHA, DEPTH, POSITION,
		GEOMETRY_NORMAL, SHADING_NORMAL, MATERIAL_ID, DIRECT_DIFFUSE,
		DIRECT_GLOSSY
	} FilmOutputType;

	FilmOutputs() { }
	~FilmOutputs() { }

	u_int GetCount() const { return types.size(); }
	FilmOutputType GetType(const u_int index) const { return types[index]; }
	const std::string &GetFileName(const u_int index) const { return fileNames[index]; }

	void Add(const FilmOutputType type, const std::string &fileName);

private:
	std::vector<FilmOutputType> types;
	std::vector<std::string> fileNames;
};

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

class SampleResult;

#define GAMMA_TABLE_SIZE 1024

class Film {
public:
	typedef enum {
		RADIANCE_PER_PIXEL_NORMALIZED = 1<<0,
		RADIANCE_PER_SCREEN_NORMALIZED = 1<<1,
		ALPHA = 1<<2,
		TONEMAPPED_FRAMEBUFFER = 1<<3,
		DEPTH = 1<<4,
		POSITION = 1<<5,
		GEOMETRY_NORMAL = 1<<6,
		SHADING_NORMAL = 1<<7,
		MATERIAL_ID = 1<<8,
		DIRECT_DIFFUSE = 1<<9,
		DIRECT_GLOSSY = 1<<10
	} FilmChannelType;

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

	void Output(const FilmOutputs &filmOutputs);
	void Output(const FilmOutputs::FilmOutputType type, const std::string &fileName);

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

	void AddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight);
	void AddSampleResultNoColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight);
	void AddSampleResult(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight);

	std::set<FilmChannelType> channels;
	u_int width, height, pixelCount;
	GenericFrameBuffer<4, float> *channel_RADIANCE_PER_PIXEL_NORMALIZED;
	GenericFrameBuffer<4, float> *channel_RADIANCE_PER_SCREEN_NORMALIZED;
	GenericFrameBuffer<2, float> *channel_ALPHA;
	GenericFrameBuffer<3, float> *channel_RGB_TONEMAPPED;
	GenericFrameBuffer<1, float> *channel_DEPTH;
	GenericFrameBuffer<3, float> *channel_POSITION;
	GenericFrameBuffer<3, float> *channel_GEOMETRY_NORMAL;
	GenericFrameBuffer<3, float> *channel_SHADING_NORMAL;
	GenericFrameBuffer<1, u_int> *channel_MATERIAL_ID;
	GenericFrameBuffer<4, float> *channel_DIRECT_DIFFUSE;
	GenericFrameBuffer<4, float> *channel_DIRECT_GLOSSY;

	double statsTotalSampleCount, statsStartSampleTime, statsAvgSampleSec;

	float gamma;
	float gammaTable[GAMMA_TABLE_SIZE];

	ToneMapParams *toneMapParams;

	ConvergenceTest *convTest;

	Filter *filter;
	FilterLUTs *filterLUTs;

	bool initialized, enabledOverlappedScreenBufferUpdate;
};

//------------------------------------------------------------------------------
// SampleResult
//------------------------------------------------------------------------------

class SampleResult {
public:
	SampleResult() { }
	SampleResult(const u_int channelTypes) { channels = channelTypes; }
	~SampleResult() { }

	void Init(const u_int channelTypes) { channels = channelTypes; };
	bool HasChannel(const Film::FilmChannelType type) const { return (channels & type); }

	float filmX, filmY;
	luxrays::Spectrum radiancePerPixelNormalized, radiancePerScreenNormalized;
	float alpha, depth;
	luxrays::Point position;
	luxrays::Normal geometryNormal, shadingNormal;
	u_int materialID;
	luxrays::Spectrum directDiffuse, directGlossy;

private:
	u_int channels;
};

inline void AddSampleResult(std::vector<SampleResult> &sampleResults,
	const float filmX, const float filmY,
	const luxrays::Spectrum &radiancePPN,
	const float alpha) {
	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	sampleResults[size].Init(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA);
	sampleResults[size].filmX = filmX;
	sampleResults[size].filmY = filmY;
	sampleResults[size].radiancePerPixelNormalized = radiancePPN;
	sampleResults[size].alpha = alpha;
}

inline void AddSampleResult(std::vector<SampleResult> &sampleResults,
	const float filmX, const float filmY,
	const luxrays::Spectrum &radiancePSN) {
	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	sampleResults[size].Init(Film::RADIANCE_PER_SCREEN_NORMALIZED);
	sampleResults[size].filmX = filmX;
	sampleResults[size].filmY = filmY;
	sampleResults[size].radiancePerScreenNormalized = radiancePSN;
}

}

#endif	/* _SLG_FILM_H */
