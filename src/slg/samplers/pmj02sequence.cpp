/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include <math.h>

#include "slg/samplers/pmj02sequence.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PMJ02Sequence
//------------------------------------------------------------------------------

PMJ02Sequence::PMJ02Sequence(luxrays::RandomGenerator *rnd) : rndGen(rnd), current_sample(0) {
}

PMJ02Sequence::~PMJ02Sequence() {
}

void PMJ02Sequence::RequestSamples(const u_int size) {
	const u_int currentSize = samplePoints.size();

	// More dimensions than generated are being requested
	if (size > currentSize) {
		u_int dimensionsToGenerate = size - currentSize;
		
		// We cannot generate an odd number of dimensions
		if (dimensionsToGenerate % 2) {
			dimensionsToGenerate += 1;
		}

		samplePoints.resize(size);
		for (u_int i = currentSize; i < dimensionsToGenerate; i++) {
			samplePoints[i].reserve(num_samples);

			generate_2D(samplePoints[i].data(), rndGen);
		}
	}
}

float PMJ02Sequence::GetSample(const u_int pass, const u_int index) {
	// More samples than generated are boing requested
	if (pass > current_sample) {
		RequestSamples(samplePoints.size() * 2);
	}
	
	const u_int vectorIndex = index / 2;
	
	if (index % 2) {
		return samplePoints[vectorIndex].y;
	} 
	return samplePoints[vectorIndex].x;
}


void PMJ02Sampler::generate_2D(float2 points[], u_int size, luxrays::RandomGenerator *rndGen) {
	points[0].x = rndGen->floatValue();
	points[0].y = rndGen->floatValue();
	for (u_int N = 1; N < size; N = 4 *N) {
		extend_sequence_even(points, N);
		extend_sequence_odd(points, 2 * N);
	}
}
void PMJ02Sampler::mark_occupied_strata(float2 points[], u_int N) {
	u_int NN = 2 * N;
	u_int num_shapes = (int)log2f(NN) + 1;
	occupiedStrata.resize(num_shapes);
	for (u_int shape = 0; shape < num_shapes; ++shape) {
		occupiedStrata[shape].resize(NN);
		for (int n = 0; n < NN; ++n) {
			occupiedStrata[shape][n] = false;
		}
	}
	for (int s = 0; s < N; ++s) {
		mark_occupied_strata1(points[s], NN);
	}
}

void PMJ02Sampler::mark_occupied_strata1(float2 pt, u_int NN) {
	u_int shape = 0;
	u_int xdivs = NN;
	u_int ydivs = 1;
	do {
		u_int xstratum = (u_int)(xdivs * pt.x);
		u_int ystratum = (u_int)(ydivs * pt.y);
		size_t index = ystratum * xdivs + xstratum;

		occupiedStrata[shape][index] = true;
		shape = shape + 1;
		xdivs = xdivs / 2;
		ydivs = ydivs * 2;
	} while (xdivs > 0);
}

void PMJ02Sampler::generate_sample_point(float2 points[], float i, float j, float xhalf, float yhalf, u_int n, u_int N) {
	u_int NN = 2 * N;
	float2 pt;
	do {
		pt.x = (i + 0.5f * (xhalf + rndGen->floatValue())) / n;
		pt.y = (j + 0.5f * (yhalf + rndGen->floatValue())) / n;
	} while (is_occupied(pt, NN));
	mark_occupied_strata1(pt, NN);
	points[num_samples] = pt;
	++num_samples;
}
void PMJ02Sampler::extend_sequence_even(float2 points[], u_int N) {
	u_int n = (int)sqrtf(N);
	occupied1Dx.resize(2 * N);
	occupied1Dy.resize(2 * N);
	mark_occupied_strata(points, N);
	for (int s = 0; s < N; ++s) {
		float2 oldpt = points[s];
		float i = floorf(n * oldpt.x);
		float j = floorf(n * oldpt.y);
		float xhalf = floorf(2.0f * (n * oldpt.x - i));
		float yhalf = floorf(2.0f * (n * oldpt.y - j));
		xhalf = 1.0f - xhalf;
		yhalf = 1.0f - yhalf;
		generate_sample_point(points, i, j, xhalf, yhalf, n, N);
	}
}

void PMJ02Sampler::extend_sequence_odd(float2 points[], u_int N) {
	u_int n = (int)sqrtf(N / 2);
	occupied1Dx.resize(2 * N);
	occupied1Dy.resize(2 * N);
	mark_occupied_strata(points, N);
	std::vector<float> xhalves(N / 2);
	std::vector<float> yhalves(N / 2);
	for (int s = 0; s < N / 2; ++s) {
		float2 oldpt = points[s];
		float i = floorf(n * oldpt.x);
		float j = floorf(n * oldpt.y);
		float xhalf = floorf(2.0f * (n * oldpt.x - i));
		float yhalf = floorf(2.0f * (n * oldpt.y - j));
		if (rndGen->floatValue() > 0.5f) {
			xhalf = 1.0f - xhalf;
		}
		else {
			yhalf = 1.0f - yhalf;
		}
		xhalves[s] = xhalf;
		yhalves[s] = yhalf;
		generate_sample_point(points, i, j, xhalf, yhalf, n, N);
	}
	for (int s = 0; s < N / 2; ++s) {
		float2 oldpt = points[s];
		float i = floorf(n * oldpt.x);
		float j = floorf(n * oldpt.y);
		float xhalf = 1.0f - xhalves[s];
		float yhalf = 1.0f - yhalves[s];
		generate_sample_point(points, i, j, xhalf, yhalf, n, N);
	}
}

bool PMJ02Sampler::is_occupied(float2 pt, u_int NN) {
	u_int shape = 0;
	u_int xdivs = NN;
	u_int ydivs = 1;
	do {
		u_int xstratum = (u_int)(xdivs * pt.x);
		u_int ystratum = (u_int)(ydivs * pt.y);
		size_t index = ystratum * xdivs + xstratum;
		if (occupiedStrata[shape][index]) {
			return true;
		}
		shape = shape + 1;
		xdivs = xdivs / 2;
		ydivs = ydivs * 2;
	} while (xdivs > 0);
	return false;
}

void PMJ02Sampler::shuffle(float2 points[], u_int size) {

	constexpr u_int odd[8] = { 0, 1, 4, 5, 10, 11, 14, 15 };
	constexpr u_int even[8] = { 2, 3, 6, 7, 8, 9, 12, 13 };

	u_int rng_index = 0;
	for (u_int yy = 0; yy < size / 16; ++yy) {
		for (u_int xx = 0; xx < 8; ++xx) {
			u_int other = (u_int)(rndGen->floatValue() * (8.0f - xx) + xx);
			float2 tmp = points[odd[other] + yy * 16];
			points[odd[other] + yy * 16] = points[odd[xx] + yy * 16];
			points[odd[xx] + yy * 16] = tmp;
		}
		for (u_int xx = 0; xx < 8; ++xx) {
			u_int other = (u_int)(rndGen->floatValue() * (8.0f - xx) + xx);
			float2 tmp = points[even[other] + yy * 16];
			points[even[other] + yy * 16] = points[even[xx] + yy * 16];
			points[even[xx] + yy * 16] = tmp;
		}
	}
}
