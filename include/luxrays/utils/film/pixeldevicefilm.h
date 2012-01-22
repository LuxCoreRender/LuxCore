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

#ifndef _LUXRAYS_UTILS_PIXELDEVICE_FILM_H
#define	_LUXRAYS_UTILS_PIXELDEVICE_FILM_H

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

#include "luxrays/utils/film/film.h"
#include "luxrays/core/pixel/samplebuffer.h"
#include "luxrays/core/pixeldevice.h"
#include "luxrays/core/pixel/samplebuffer.h"

namespace luxrays { namespace utils {

//------------------------------------------------------------------------------
// PixelDevice Film implementations
//------------------------------------------------------------------------------

class PixelDeviceFilm : public Film {
public:
	PixelDeviceFilm(Context *context, const unsigned int w,
			const unsigned int h, DeviceDescription *deviceDesc);
	virtual ~PixelDeviceFilm() { }

	virtual void Init(const unsigned int w, const unsigned int h) {
		pixelDevice->Init(w, h);
		Film::Init(w, h);
	}

	virtual void InitGammaTable(const float gamma = 2.2f) {
		pixelDevice->SetGamma(gamma);
	}

	virtual void Reset() {
		pixelDevice->ClearSampleFrameBuffer();
		Film::Reset();
	}

	void UpdateScreenBuffer() {
		pixelDevice->UpdateFrameBuffer(*toneMapParams);
	}

	const float *GetScreenBuffer() const {
		return (const float *)pixelDevice->GetFrameBuffer()->GetPixels();
	}

	SampleBuffer *GetFreeSampleBuffer() {
		return pixelDevice->GetFreeSampleBuffer();
	}

	void FreeSampleBuffer(SampleBuffer *sampleBuffer) {
		pixelDevice->FreeSampleBuffer(sampleBuffer);
	}

	void SplatSampleBuffer(const bool preview, SampleBuffer *sampleBuffer) {
		Film::SplatSampleBuffer(preview, sampleBuffer);

		if (preview)
			pixelDevice->AddSampleBuffer(filterType, sampleBuffer);
		else
			pixelDevice->AddSampleBuffer(FILTER_PREVIEW, sampleBuffer);
	}

	void Save(const std::string &fileName) {
		// Update pixels
		pixelDevice->UpdateFrameBuffer(*toneMapParams);
		SaveImpl(fileName);
	}

protected:
	const SampleFrameBuffer *GetSampleFrameBuffer() {
		return pixelDevice->GetSampleFrameBuffer();
	}

	void AddSampleFrameBuffer(const SampleFrameBuffer *sfb) {
		pixelDevice->Merge(sfb);
	}

	Context *ctx;
	PixelDevice *pixelDevice;
};

} }

#endif	/* _LUXRAYS_FILM_FILM_H */
