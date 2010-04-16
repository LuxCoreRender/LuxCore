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

#ifndef _SAMPLEBUFFER_H
#define	_SAMPLEBUFFER_H

#include "smalllux.h"
#include "sampler.h"

#define SAMPLE_BUFFER_SIZE (4096)

typedef struct {
	float screenX, screenY;
	Spectrum radiance;
} SampleBufferElem;

class SampleBuffer {
public:
	SampleBuffer(size_t bufferSize) : size(bufferSize) {
		samples = new SampleBufferElem[size];
		Reset();
	}
	~SampleBuffer() {
		delete[] samples;
	}

	Sampler *GetSampler() const { return sampler; }
	void Reset() { currentFreeSample = 0; };
	bool IsFull() const { return (currentFreeSample >= size); }

	void SplatSample(const Sample *sample, const Spectrum &radiance) {
		// Safety check
		if (radiance.IsNaN())
			cerr << "Internal error: NaN in SampleBuffer::SplatSample()\n";
		else {
			SampleBufferElem *s = &samples[currentFreeSample++];

			s->screenX = sample->screenX;
			s->screenY = sample->screenY;
			s->radiance = radiance;
		}
	}

	SampleBufferElem *GetSampleBuffer() const { return samples; }

	size_t GetSampleCount() const { return currentFreeSample; }

private:
	Sampler *sampler;
	size_t size;
	size_t currentFreeSample;

	SampleBufferElem *samples;
};

#endif	/* _SAMPLEBUFFER_H */
