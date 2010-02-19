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

#ifndef _SAMPLER_H
#define	_SAMPLER_H

#include "luxrays/utils/core/randomgen.h"

class Sample;

class Sampler {
public:
	virtual ~Sampler() { }

	virtual void Init(const unsigned width, const unsigned height) = 0;
	virtual unsigned int GetPass() = 0;

	virtual void GetNextSample(Sample *sample) = 0;
	virtual float GetLazyValue(Sample *sample) = 0;

	virtual bool IsLowLatency() const = 0;
};

class Sample {
public:
	Sample() { }

	void Init(Sampler *s,
		const float x, const float y,
		const unsigned p) {
		sampler = s;

		screenX = x;
		screenY = y;
		pass = p;
	}

	float GetLazyValue() {
		return sampler->GetLazyValue(this);
	}

	float screenX, screenY;
	unsigned int pass;

private:
	Sampler *sampler;
};

class RandomSampler : public Sampler {
public:
	RandomSampler(const bool lowLat, unsigned int startSeed,
			const unsigned width, const unsigned height) :
		seed(startSeed), lowLatency(lowLat) {
		rndGen = new RandomGenerator();

		Init(width, height);
	}
	~RandomSampler() {
		delete rndGen;
	}

	void Init(const unsigned width, const unsigned height) {
		rndGen->init(seed);
		screenWidth = width;
		screenHeight = height;
		currentSampleScreenX = 0;
		currentSampleScreenY = 0;
		currentSubSampleIndex = 0;
		pass = 0;
	}

	void GetNextSample(Sample *sample) {
		unsigned int scrX, scrY;

		 if (!lowLatency || (pass >= 64)) {
			// In order to improve ray coherency
			unsigned int stepX = currentSubSampleIndex % 4;
			unsigned int stepY = currentSubSampleIndex / 4;

			scrX = currentSampleScreenX;
			scrY = currentSampleScreenY;

			currentSubSampleIndex++;
			if (currentSubSampleIndex == 16) {
				currentSubSampleIndex = 0;
				currentSampleScreenX++;
				if (currentSampleScreenX  >= screenWidth) {
					currentSampleScreenX = 0;
					currentSampleScreenY++;

					if (currentSampleScreenY >= screenHeight) {
						currentSampleScreenY = 0;
						pass += 16;
					}
				}
			}

			const float r1 = (stepX + rndGen->floatValue()) / 4.f - .5f;
			const float r2 = (stepY + rndGen->floatValue()) / 4.f - .5f;

			sample->Init(this,
					scrX + r1, scrY + r2,
					pass);
		} else if (pass >= 32) {
			scrX = currentSampleScreenX;
			scrY = currentSampleScreenY;

			currentSampleScreenX++;
			if (currentSampleScreenX >= screenWidth) {
				currentSampleScreenX = 0;
				currentSampleScreenY++;

				if (currentSampleScreenY >= screenHeight) {
					currentSampleScreenY = 0;
					pass++;
				}
			}

			const float r1 = rndGen->floatValue() - .5f;
			const float r2 = rndGen->floatValue() - .5f;

			sample->Init(this,
					scrX + r1, scrY + r2,
					pass);
		} else {
			// In order to update the screen faster for the first 16 passes
			for (;;) {
				unsigned int stepX = pass % 4;
				unsigned int stepY = (pass / 4) % 4;

				scrX = currentSampleScreenX * 4 + stepX;
				scrY = currentSampleScreenY * 4 + stepY;

				currentSampleScreenX++;
				if (currentSampleScreenX * 4 >= screenWidth) {
					currentSampleScreenX = 0;
					currentSampleScreenY++;

					if (currentSampleScreenY * 4 >= screenHeight) {
						currentSampleScreenY = 0;
						pass++;
					}
				}

				// Check if we are inside the screen
				if ((scrX < screenWidth) && (scrY < screenHeight)) {
					// Ok, it is a valid sample
					break;
				} else if (pass >= 32) {
					GetNextSample(sample);
					return;
				}
			}

			const float r1 = rndGen->floatValue() - .5f;
			const float r2 = rndGen->floatValue() - .5f;

			sample->Init(this,
					scrX + r1, scrY + r2,
					pass);
		}
	}

	float GetLazyValue(Sample *sample) {
		return rndGen->floatValue();
	}

	unsigned int GetPass() { return pass; }
	bool IsLowLatency() const { return lowLatency; }

private:
	RandomGenerator *rndGen;
	unsigned int seed;
	unsigned int screenWidth, screenHeight;
	unsigned int currentSampleScreenX, currentSampleScreenY, currentSubSampleIndex;
	unsigned int pass;
	bool lowLatency;
};

#endif	/* _FILM_H */
