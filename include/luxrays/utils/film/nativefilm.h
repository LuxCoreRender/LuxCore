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
#include "luxrays/core/pixel/samplebuffer.h"
#include "luxrays/core/pixel/filter.h"
#include "luxrays/core/pixel/framebuffer.h"
#include "luxrays/core/pixel/samplebuffer.h"

namespace luxrays { namespace utils {

//------------------------------------------------------------------------------
// Native CPU Film implementations
//------------------------------------------------------------------------------

class NativeFilm : public Film {
public:
	NativeFilm(const unsigned int w, const unsigned int h);
	virtual ~NativeFilm();

	void Init(const unsigned int w, const unsigned int h);
	void Reset();
	void UpdateScreenBuffer();

	const float *GetScreenBuffer() const;

	SampleBuffer *GetFreeSampleBuffer();
	void FreeSampleBuffer(SampleBuffer *sampleBuffer);

	void SplatSampleBuffer(const bool preview, SampleBuffer *sampleBuffer);

	void Save(const std::string &fileName);

	//--------------------------------------------------------------------------

	void AddRadiance(const unsigned int x, const unsigned int y, const Spectrum radiance, const float weight) {
		const unsigned int offset = x + y * width;
		SamplePixel *sp = &(sampleFrameBuffer->GetPixels()[offset]);

		sp->radiance += radiance;
		sp->weight += weight;
	}

	void SplatPreview(const SampleBufferElem *sampleElem);
	void SplatFiltered(const SampleBufferElem *sampleElem);

	static size_t SampleBufferSize;

protected:
	const SampleFrameBuffer *GetSampleFrameBuffer() {
		return sampleFrameBuffer;
	}

	void AddSampleFrameBuffer(const SampleFrameBuffer *sfb);

	void AddSampleBuffer(const FilterType type, SampleBuffer *sampleBuffer);

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

	SampleFrameBuffer *sampleFrameBuffer;
	FrameBuffer *frameBuffer;

	std::vector<SampleBuffer *> sampleBuffers;
	std::deque<SampleBuffer *> freeSampleBuffers;

	Filter *filter;
	FilterLUTs *filterLUTs;
};

} }

#endif	/* _LUXRAYS_FILM_FILM_H */
