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

#ifndef _LUXRAYS_UTILS_NATIVE_FILM_H
#define	_LUXRAYS_UTILS_NATIVE_FILM_H

#include <boost/thread/mutex.hpp>

#include "luxrays/utils/film/film.h"
#include "luxrays/utils/film/filter.h"
#include "luxrays/utils/convtest/convtest.h"

namespace luxrays { namespace utils {

//------------------------------------------------------------------------------
// Native CPU Film implementations
//------------------------------------------------------------------------------

class NativeFilm : public Film {
public:
	NativeFilm(const unsigned int w, const unsigned int h, const bool perScreenNorm);
	virtual ~NativeFilm();

	void Init(const unsigned int w, const unsigned int h);
	void Reset();
	void UpdateScreenBuffer();

	const float *GetScreenBuffer() const;

	void Save(const std::string &fileName);

	void EnableAlphaChannel(const bool alphaChannel) {
		enableAlphaChannel = alphaChannel;
	}
	bool IsAlphaChannelEnabled() const { return enableAlphaChannel; }

	void AddFilm(const NativeFilm &film);

	//--------------------------------------------------------------------------
	
	void ResetConvergenceTest();
	unsigned int RunConvergenceTest();
	
	//--------------------------------------------------------------------------

	void AddSampleCount(const unsigned int count) { ++statsTotalSampleCount; }

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

protected:
	const SampleFrameBuffer *GetSampleFrameBuffer() {
		return sampleFrameBuffer;
	}
	void AddSampleFrameBuffer(const SampleFrameBuffer *sfb);

	const AlphaFrameBuffer *GetAlphaFrameBuffer() {
		return alphaFrameBuffer;
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

	SampleFrameBuffer *sampleFrameBuffer;
	AlphaFrameBuffer *alphaFrameBuffer;
	FrameBuffer *frameBuffer;

	Filter *filter;
	FilterLUTs *filterLUTs;
	
	ConvergenceTest convTest;

	bool enableAlphaChannel;
};

} }

#endif	/* _LUXRAYS_FILM_FILM_H */
