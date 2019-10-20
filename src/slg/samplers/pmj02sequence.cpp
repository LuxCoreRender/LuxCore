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

PMJ02Sequence::PMJ02Sequence(luxrays::RandomGenerator *rnd) : 
	rndGen(rnd), num_samples(1024) {

	SLG_LOG("Random in sequence constructor: "<< rndGen->uintValue());
}

PMJ02Sequence::~PMJ02Sequence() {
}

void PMJ02Sequence::RequestSamples(const u_int size) {
	// SLG_LOG("5.1: " << size);
	// We cannot generate an odd number of dimensions
	u_int tablesToGenerate = size / 2;
	if (size % 2) tablesToGenerate += 1;

	samplePoints.resize(tablesToGenerate);

	// SLG_LOG("Generating " << num_samples << " samples for " << size << " dimensions on " << tablesToGenerate << " tables");
	for (u_int i = 0; i < tablesToGenerate; i++) {
		samplePoints[i].resize(num_samples);
		float2 *points = samplePoints[i].data();
		generate_2D(points, num_samples);
		shuffle(points, num_samples);
		// for (u_int j = 0; j < num_samples; j++) {
		// 	SLG_LOG("dump: " << i << "," << j << "," << points[j].x << "," << points[j].y);
		// }
	}
	// SLG_LOG("Generated " << num_samples << " samples for " << size << " dimensions on " << tablesToGenerate << " tables");
}

float PMJ02Sequence::GetSample(const u_int pass, const u_int index) {

	if (pass > num_samples) return rndGen->floatValue();

	const u_int dimensionIndex = index / 2;
	
	if (index % 2) {
		return samplePoints[dimensionIndex][pass].y;
	} 
	return samplePoints[dimensionIndex][pass].x;
}


void PMJ02Sequence::generate_2D(float2 points[], u_int size) {
	current_sample = 1;
	points[0].x = rndGen->floatValue();
	points[0].y = rndGen->floatValue();
	for (u_int N = 1; N < size; N = 4 *N) {
		extend_sequence_even(points, N);
		extend_sequence_odd(points, 2 * N);
	}
}
void PMJ02Sequence::mark_occupied_strata(float2 points[], u_int N) {
	u_int NN = 2 * N;
	u_int num_shapes = (int)log2f(NN) + 1;
	occupiedStrata.resize(num_shapes);
	for (u_int shape = 0; shape < num_shapes; ++shape) {
		occupiedStrata[shape].resize(NN);
		for (int n = 0; n < NN; ++n) {
			occupiedStrata[shape][n] = false;
		}
	}
	for (u_int s = 0; s < N; ++s) {
		mark_occupied_strata1(points[s], NN);
	}
}

void PMJ02Sequence::mark_occupied_strata1(float2 pt, u_int NN) {
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

void PMJ02Sequence::generate_sample_point(float2 points[], float i, float j, float xhalf, float yhalf, u_int n, u_int N) {
	u_int NN = 2 * N;
	float2 pt;
	do {
		pt.x = (i + 0.5f * (xhalf + rndGen->floatValue())) / n;
		pt.y = (j + 0.5f * (yhalf + rndGen->floatValue())) / n;
	} while (is_occupied(pt, NN));
	mark_occupied_strata1(pt, NN);
	points[current_sample] = pt;
	++current_sample;
}
void PMJ02Sequence::extend_sequence_even(float2 points[], u_int N) {
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

void PMJ02Sequence::extend_sequence_odd(float2 points[], u_int N) {
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

bool PMJ02Sequence::is_occupied(float2 pt, u_int NN) {
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

void PMJ02Sequence::shuffle(float2 points[], u_int size) {

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
